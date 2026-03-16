/**
 * @file Config.cpp
 * @brief Implementation of the Lua-driven configuration loader.
 *
 * Uses sol2 (`sol::state`) to execute Lua scripts in a sandboxed environment.
 * The validation script injected before the user config detects undefined
 * global accesses and reports them as warnings rather than silent failures.
 *
 * ### Load pipeline
 * @code
 * Config()
 *   initDefaults()          ← populate values map with built-in defaults
 *   initSchema()            ← populate schema map with type/range constraints
 *   load(end.lua)
 *     lua.safe_script(validationScript)   ← install _undefined tracker
 *     lua.safe_script_file(end.lua)       ← execute user config
 *     iterate _undefined → warnings
 *     iterate END.* → validate + store
 *   (AppState loads state.xml separately)
 * @endcode
 *
 * ### Colour parsing
 * `parseColour()` handles `#RGB`, `#RRGGBB`, `#AARRGGBB`, and
 * `rgba(r,g,b,a)` formats.  All other formats trigger `jassertfalse` and
 * return magenta as a visible error sentinel.
 *
 * @see Config
 * @see Config::Key
 * @see Config::Theme
 */

#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>

#include "Config.h"

/**
 * @brief Populates the values map with all built-in default settings.
 *
 * Every key defined in `Config::Key` must have a corresponding entry here.
 * Defaults are applied first; `end.lua` values overlay them.
 *
 * @note Called at construction and at the start of `reload()`.
 */

#if JUCE_WINDOWS
/**
 * @brief Detects the best available shell on Windows.
 *
 * Search order:
 * 1. zsh — MSYS2 (`C:/msys64/usr/bin/zsh.exe`)
 * 2. zsh — Git for Windows (`C:/Program Files/Git/usr/bin/zsh.exe`)
 * 3. pwsh — PowerShell 7+ (`C:/Program Files/PowerShell/7/pwsh.exe`)
 * 4. powershell.exe — legacy fallback (always available)
 *
 * @return Pair of {shell path, shell args}.
 */
static std::pair<juce::String, juce::String> findDefaultWindowsShell() noexcept
{
    static constexpr const char* zshPaths[] { "C:\\msys64\\usr\\bin\\zsh.exe",
                                              "C:\\Program Files\\Git\\usr\\bin\\zsh.exe" };

    for (const auto* path : zshPaths)
    {
        if (juce::File (path).existsAsFile())
            return { path, "-l" };
    }

    const juce::File pwsh { "C:\\Program Files\\PowerShell\\7\\pwsh.exe" };

    if (pwsh.existsAsFile())
        return { pwsh.getFullPathName(), "" };

    return { "powershell.exe", "" };
}
#endif

void Config::initDefaults()
{
    values[Key::fontFamily] = "Display Mono";
#if JUCE_WINDOWS
    values[Key::fontSize] = 10.0f;
#else
    values[Key::fontSize] = 14.0f;
#endif
    values[Key::fontLigatures] = true;
    values[Key::fontEmbolden] = false;

    values[Key::cursorChar] = juce::String::charToString (static_cast<juce::juce_wchar> (0x2588));
    values[Key::cursorBlink] = true;
    values[Key::cursorBlinkInterval] = 500;
    values[Key::cursorForce] = false;

    values[Key::coloursForeground] = "#FF4E8C93";///< paradiso
    values[Key::coloursBackground] = "#E0090D12";///< bunker
    values[Key::coloursCursor] = "#FF4E8C93";///< paradiso
    values[Key::coloursSelection] = "#8000C8D8";///< blueBikini

    values[Key::coloursBlack] = "#FF090D12";///< bunker
    values[Key::coloursRed] = "#FFFC704C";///< preciousPersimmon
    values[Key::coloursGreen] = "#FFC5F0E9";///< gentleCold
    values[Key::coloursYellow] = "#FFF3F5C5";///< silkStar
    values[Key::coloursBlue] = "#FF8CC9D9";///< dolphin
    values[Key::coloursMagenta] = "#FF519299";///< lagoon
    values[Key::coloursCyan] = "#FF699DAA";///< tranquiliTeal
    values[Key::coloursWhite] = "#FFFF0000";///< frostbite

    values[Key::coloursBrightBlack] = "#FF33535B";///< mediterranea
    values[Key::coloursBrightRed] = "#FFFC704C";///< preciousPersimmon
    values[Key::coloursBrightGreen] = "#FFBAFFFD";///< paleSky
    values[Key::coloursBrightYellow] = "#FFFEFFD2";///< mattWhite
    values[Key::coloursBrightBlue] = "#FF67DFEF";///< poseidonJr
    values[Key::coloursBrightMagenta] = "#FF01C2D2";///< caribbeanBlue
    values[Key::coloursBrightCyan] = "#FF00C8D8";///< blueBikini
    values[Key::coloursBrightWhite] = "#FFBAFFFD";///< paleSky

    values[Key::windowTitle] = ProjectInfo::projectName;
    values[Key::windowWidth] = 640;
    values[Key::windowHeight] = 480;
    values[Key::windowColour] = "#090D12";///< bunker
    values[Key::windowOpacity] = 0.75f;
    values[Key::windowBlurRadius] = 32.0f;
    values[Key::windowAlwaysOnTop] = false;
    values[Key::windowButtons] = false;
    values[Key::windowZoom] = 1.0f;

    values[Key::tabFamily] = "Display Mono";
    values[Key::tabSize] = 24.0f;
    values[Key::tabForeground] = "#FF00C8D8";///< blueBikini
    values[Key::tabInactive] = "#FF33535B";///< mediterranea
    values[Key::tabPosition] = "left";
    values[Key::tabLine] = "#FF2C4144";///< littleMermaid
    values[Key::tabActive] = "#FF002B35";///< midnightDreams
    values[Key::tabIndicator] = "#FF01C2D2";///< caribbeanBlue
    values[Key::menuOpacity] = 0.65f;

    values[Key::overlayFamily] = "Display Mono";
    values[Key::overlaySize] = 20.0f;
    values[Key::overlayColour] = "#4E8C93";///< paradiso

#if JUCE_MAC
    values[Key::shellProgram] = "zsh";
    values[Key::shellArgs] = "-l";
#elif JUCE_LINUX
    values[Key::shellProgram] = "bash";
    values[Key::shellArgs] = "-l";
#elif JUCE_WINDOWS
    {
        const auto [shell, args] { findDefaultWindowsShell() };
        values[Key::shellProgram] = shell;
        values[Key::shellArgs] = args;
    }
#endif

    values[Key::terminalScrollbackLines] = 10000;
    values[Key::terminalScrollStep]      = 5;
    values[Key::terminalPaddingTop]      = 10;
    values[Key::terminalPaddingRight]    = 10;
    values[Key::terminalPaddingBottom]   = 10;
    values[Key::terminalPaddingLeft]     = 10;

#if JUCE_MAC
    values[Key::keysCopy] = "cmd+c";
    values[Key::keysPaste] = "cmd+v";
#else
    values[Key::keysCopy] = "ctrl+c";
    values[Key::keysPaste] = "ctrl+v";
#endif
    values[Key::keysQuit] = "cmd+q";
    values[Key::keysCloseTab] = "cmd+w";
    values[Key::keysReload] = "cmd+r";
    values[Key::keysZoomIn] = "cmd+=";
    values[Key::keysZoomOut] = "cmd+-";
    values[Key::keysZoomReset] = "cmd+0";
    values[Key::keysNewTab] = "cmd+t";
    values[Key::keysPrevTab] = "cmd+[";
    values[Key::keysNextTab] = "cmd+]";
    values[Key::keysSplitHorizontal] = juce::String::charToString (static_cast<juce::juce_wchar> ('\\'));
    values[Key::keysSplitVertical] = "-";
    values[Key::keysPrefix] = "`";
    values[Key::keysPrefixTimeout] = 1000;
    values[Key::keysPaneLeft] = "h";
    values[Key::keysPaneDown] = "j";
    values[Key::keysPaneUp] = "k";
    values[Key::keysPaneRight] = "l";
    values[Key::keysNewline] = "shift+return";
    values[Key::keysActionList] = "?";
    values[Key::keysActionListPosition] = "top";

    values[Key::popupWidth] = 0.6f;
    values[Key::popupHeight] = 0.5f;
    values[Key::popupPosition] = "center";

    values[Key::paneBarColour] = "#FF1B2A31";///< dark
    values[Key::paneBarHighlight] = "#FF4E8C93";///< paradiso
}

/**
 * @brief Populates the schema map with type and range constraints.
 *
 * Each entry maps a dot-notation key to a `ValueSpec` describing the expected
 * Lua type and, for numbers, the valid range.  Keys absent from the schema are
 * still stored but are not range-validated.
 *
 * @note Called once at construction; not called during `reload()`.
 */
void Config::initSchema()
{
    using T = ValueSpec::Type;

    schema[Key::fontFamily] = { T::string };
    schema[Key::fontSize] = { T::number, 1.0, 200.0, true };
    schema[Key::fontLigatures] = { T::boolean };
    schema[Key::fontEmbolden] = { T::boolean };

    schema[Key::cursorChar] = { T::string };
    schema[Key::cursorBlink] = { T::boolean };
    schema[Key::cursorBlinkInterval] = { T::number, 100.0, 5000.0, true };
    schema[Key::cursorForce] = { T::boolean };

    schema[Key::coloursForeground] = { T::string };
    schema[Key::coloursBackground] = { T::string };
    schema[Key::coloursCursor] = { T::string };
    schema[Key::coloursSelection] = { T::string };
    schema[Key::coloursBlack] = { T::string };
    schema[Key::coloursRed] = { T::string };
    schema[Key::coloursGreen] = { T::string };
    schema[Key::coloursYellow] = { T::string };
    schema[Key::coloursBlue] = { T::string };
    schema[Key::coloursMagenta] = { T::string };
    schema[Key::coloursCyan] = { T::string };
    schema[Key::coloursWhite] = { T::string };
    schema[Key::coloursBrightBlack] = { T::string };
    schema[Key::coloursBrightRed] = { T::string };
    schema[Key::coloursBrightGreen] = { T::string };
    schema[Key::coloursBrightYellow] = { T::string };
    schema[Key::coloursBrightBlue] = { T::string };
    schema[Key::coloursBrightMagenta] = { T::string };
    schema[Key::coloursBrightCyan] = { T::string };
    schema[Key::coloursBrightWhite] = { T::string };

    schema[Key::windowTitle] = { T::string };
    schema[Key::windowColour] = { T::string };
    schema[Key::windowOpacity] = { T::number, 0.0, 1.0, true };
    schema[Key::windowBlurRadius] = { T::number, 0.0, 100.0, true };
    schema[Key::windowAlwaysOnTop] = { T::boolean };
    schema[Key::windowButtons] = { T::boolean };

    schema[Key::tabFamily] = { T::string };
    schema[Key::tabSize] = { T::number, 1.0, 200.0, true };
    schema[Key::tabForeground] = { T::string };
    schema[Key::tabInactive] = { T::string };
    schema[Key::tabPosition] = { T::string };
    schema[Key::tabLine] = { T::string };
    schema[Key::tabActive] = { T::string };
    schema[Key::tabIndicator] = { T::string };
    schema[Key::menuOpacity] = { T::number, 0.0, 1.0, true };

    schema[Key::overlayFamily] = { T::string };
    schema[Key::overlaySize] = { T::number, 1.0, 200.0, true };
    schema[Key::overlayColour] = { T::string };

    schema[Key::shellProgram] = { T::string };
    schema[Key::shellArgs] = { T::string };

    schema[Key::terminalScrollbackLines] = { T::number, 100.0, 1000000.0, true };
    schema[Key::terminalScrollStep]      = { T::number, 1.0,   100.0,     true };
    schema[Key::terminalPaddingTop]      = { T::number, 0.0,   200.0,     true };
    schema[Key::terminalPaddingRight]    = { T::number, 0.0,   200.0,     true };
    schema[Key::terminalPaddingBottom]   = { T::number, 0.0,   200.0,     true };
    schema[Key::terminalPaddingLeft]     = { T::number, 0.0,   200.0,     true };

    schema[Key::keysCopy] = { T::string };
    schema[Key::keysPaste] = { T::string };
    schema[Key::keysQuit] = { T::string };
    schema[Key::keysCloseTab] = { T::string };
    schema[Key::keysReload] = { T::string };
    schema[Key::keysZoomIn] = { T::string };
    schema[Key::keysZoomOut] = { T::string };
    schema[Key::keysZoomReset] = { T::string };
    schema[Key::keysNewTab] = { T::string };
    schema[Key::keysPrevTab] = { T::string };
    schema[Key::keysNextTab] = { T::string };
    schema[Key::keysSplitHorizontal] = { T::string };
    schema[Key::keysSplitVertical] = { T::string };
    schema[Key::keysPrefix] = { T::string };
    schema[Key::keysPrefixTimeout] = { T::number, 100.0, 5000.0, true };
    schema[Key::keysPaneLeft] = { T::string };
    schema[Key::keysPaneDown] = { T::string };
    schema[Key::keysPaneUp] = { T::string };
    schema[Key::keysPaneRight] = { T::string };
    schema[Key::keysNewline] = { T::string };
    schema[Key::keysActionList] = { T::string };
    schema[Key::keysActionListPosition] = { T::string };

    schema[Key::popupWidth] = { T::number, 0.1, 1.0, true };
    schema[Key::popupHeight] = { T::number, 0.1, 1.0, true };
    schema[Key::popupPosition] = { T::string };

    schema[Key::paneBarColour] = { T::string };
    schema[Key::paneBarHighlight] = { T::string };
}

/**
 * @brief Constructs Config: loads defaults, schema, then end.lua.
 *
 * If `end.lua` does not exist it is created with an empty `END = {}` table via
 * `writeDefaults()`.  Any load errors are stored in `loadError` and surfaced by
 * `Terminal::Component` as a startup `MessageOverlay`.
 *
 * @note MESSAGE THREAD — called once from ENDApplication member initialisation.
 */
Config::Config()
{
    initDefaults();
    initSchema();

    if (auto configFile { getConfigFile() }; configFile.existsAsFile())
    {
        load (configFile);
    }
}

/**
 * @brief Returns the path to the config file, creating it if absent.
 *
 * - All platforms: `~/.config/end/end.lua`
 *
 * Creates the directory if it does not exist, then writes a
 * minimal `END = {}` skeleton if `end.lua` is missing.
 *
 * @return The config file; guaranteed to exist after this call.
 */
juce::File Config::getConfigFile() const
{
    auto configDir { juce::File::getSpecialLocation (juce::File::userHomeDirectory).getChildFile (".config/end") };

    if (not configDir.exists())
        configDir.createDirectory();

    auto configFile { configDir.getChildFile ("end.lua") };

    if (not configFile.existsAsFile())
        writeDefaults (configFile);

    return configFile;
}

/**
 * @brief Writes a default `end.lua` from the embedded template.
 *
 * Reads the `default_end.lua` template from BinaryData, replaces every
 * `%%key%%` placeholder with the corresponding value from `initDefaults()`,
 * and writes the result to @p file.
 *
 * @param file  The file to create; parent directory must already exist.
 */
void Config::writeDefaults (const juce::File& file) const
{
    juce::String content { BinaryData::getString ("default_end.lua") };

    content = jreng::String::replaceholder (content, "versionString", ProjectInfo::versionString);

    for (const auto& [key, value] : values)
    {
        const auto placeholder { key.replaceCharacter ('.', '_') };

        if (value.isBool())
            content = jreng::String::replaceholder (content, placeholder, static_cast<bool> (value) ? "true" : "false");
        else
        {
            // Escape backslashes for valid Lua string syntax
            auto str { value.toString() };
            str = str.replace ("\\", "\\\\");
            content = jreng::String::replaceholder (content, placeholder, str);
        }
    }

    file.replaceWithText (content);
}

/**
 * @brief Maps a sol2 type enum to a human-readable string for error messages.
 *
 * @param t  The sol2 type value.
 * @return A string such as "number", "string", "boolean", "nil", etc.
 */
static juce::String luaTypeName (sol::type t)
{
    switch (t)
    {
        case sol::type::lua_nil:
            return "nil";
        case sol::type::boolean:
            return "boolean";
        case sol::type::number:
            return "number";
        case sol::type::string:
            return "string";
        case sol::type::table:
            return "table";
        case sol::type::function:
            return "function";
        case sol::type::userdata:
            return "userdata";
        case sol::type::lightuserdata:
            return "lightuserdata";
        case sol::type::thread:
            return "thread";
        default:
            return "unknown";
    }
}

/**
 * @brief Loads config from @p file, storing errors in `loadError`.
 *
 * Delegates to the two-argument overload with the internal `loadError` member.
 *
 * @param file  The Lua config file to load.
 * @return @c true if the file was parsed without a fatal Lua error.
 */
bool Config::load (const juce::File& file) { return load (file, loadError); }

/**
 * @brief Loads and validates a Lua config file.
 *
 * ### Validation steps
 * 1. Injects `validationScript` to track undefined global accesses.
 * 2. Executes the user file with `safe_script_file`.
 * 3. Iterates `_undefined` to collect undefined-variable warnings.
 * 4. Iterates `END.*.*` to validate types and ranges against the schema.
 * 5. Stores validated values in the `values` map; rejects out-of-range numbers.
 *
 * Non-fatal warnings are appended to @p errorOut but do not prevent a
 * @c true return.  A fatal Lua parse/runtime error causes @c false.
 *
 * @param file      The Lua config file to load.
 * @param errorOut  Receives the combined error/warning string on return.
 * @return @c true if the file was parsed without a fatal Lua error.
 *
 * @see validationScript
 * @see ValueSpec
 */
bool Config::load (const juce::File& file, juce::String& errorOut)
{
    errorOut = {};
    bool success { false };

    if (file.existsAsFile())
    {
        sol::state lua;
        lua.open_libraries (
            sol::lib::base, sol::lib::string, sol::lib::table, sol::lib::os, sol::lib::debug, sol::lib::package);

        auto setupResult { lua.safe_script (validationScript, sol::script_pass_on_error) };

        if (setupResult.valid())
        {
            auto result { lua.safe_script_file (file.getFullPathName().toStdString(), sol::script_pass_on_error) };

            if (result.valid())
            {
                juce::StringArray warnings;

                sol::table undefinedAccesses = lua["_undefined"];

                if (undefinedAccesses.valid())
                {
                    undefinedAccesses.for_each (
                        [&warnings] (const sol::object&, const sol::object& entry)
                        {
                            if (entry.get_type() == sol::type::table)
                            {
                                sol::table t { entry.as<sol::table>() };
                                const juce::String name { t["name"].get<std::string>() };
                                const int line { t["line"].get<int>() };
                                warnings.add ("line " + juce::String (line) + ": undefined variable '" + name + "'");
                            }
                        });
                }

                sol::object rootObj { lua["END"] };

                if (rootObj.get_type() == sol::type::table)
                {
                    sol::table root { rootObj.as<sol::table>() };

                    root.for_each (
                        [this, &warnings] (const sol::object& groupKey, const sol::object& groupVal)
                        {
                            if (groupKey.get_type() == sol::type::string and groupVal.get_type() == sol::type::table)
                            {
                                const juce::String groupName { groupKey.as<std::string>() };

                                if (groupName == "popups")
                                    return;

                                sol::table group { groupVal.as<sol::table>() };

                                group.for_each (
                                    [this, &groupName, &warnings] (
                                        const sol::object& fieldKey, const sol::object& fieldVal)
                                    {
                                        if (fieldKey.get_type() == sol::type::string)
                                        {
                                            const juce::String fieldName { fieldKey.as<std::string>() };
                                            const juce::String dotKey { groupName + "." + fieldName };

                                            // terminal.padding is a 4-element array { top, right, bottom, left }.
                                            // Parse it into the four flat keys rather than treating it as a scalar.
                                            if (groupName == "terminal" and fieldName == "padding"
                                                and fieldVal.get_type() == sol::type::table)
                                            {
                                                sol::table arr { fieldVal.as<sol::table>() };
                                                static const std::array<const juce::String*, 4> paddingKeys {
                                                    &Key::terminalPaddingTop,
                                                    &Key::terminalPaddingRight,
                                                    &Key::terminalPaddingBottom,
                                                    &Key::terminalPaddingLeft
                                                };

                                                for (int i { 0 }; i < 4; ++i)
                                                {
                                                    sol::optional<double> v { arr.get<sol::optional<double>> (i + 1) };

                                                    if (v)
                                                    {
                                                        const auto& spec { schema.at (*paddingKeys[i]) };
                                                        const double clamped { juce::jlimit (spec.minValue, spec.maxValue, *v) };
                                                        values.insert_or_assign (*paddingKeys[i], clamped);
                                                    }
                                                }

                                                return;
                                            }

                                            if (values.find (dotKey) == values.end())
                                            {
                                                warnings.add ("unknown key '" + dotKey + "'");
                                            }
                                            else if (schema.find (dotKey) != schema.end())
                                            {
                                                const auto& spec { schema.at (dotKey) };
                                                bool typeOk { false };

                                                if (spec.expectedType == ValueSpec::Type::number)
                                                {
                                                    typeOk = (fieldVal.get_type() == sol::type::number);
                                                }
                                                else if (spec.expectedType == ValueSpec::Type::boolean)
                                                {
                                                    typeOk = (fieldVal.get_type() == sol::type::boolean);
                                                }
                                                else if (spec.expectedType == ValueSpec::Type::string)
                                                {
                                                    typeOk = (fieldVal.get_type() == sol::type::string);
                                                }
                                                if (not typeOk)
                                                {
                                                    static const std::array<const char*, 3> specTypeNames {
                                                        { "string", "number", "boolean" }
                                                    };
                                                    warnings.add (
                                                        "'" + dotKey + "' expected "
                                                        + specTypeNames.at (static_cast<size_t> (spec.expectedType))
                                                        + ", got " + luaTypeName (fieldVal.get_type()));
                                                }
                                                else if (spec.hasRange and spec.expectedType == ValueSpec::Type::number)
                                                {
                                                    const double val { fieldVal.as<double>() };

                                                    if (val < spec.minValue or val > spec.maxValue)
                                                    {
                                                        warnings.add ("'" + dotKey + "' value " + juce::String (val)
                                                                      + " out of range [" + juce::String (spec.minValue)
                                                                      + ", " + juce::String (spec.maxValue) + "]");
                                                    }
                                                    else
                                                    {
                                                        values.insert_or_assign (dotKey, val);
                                                    }
                                                }
                                                else if (fieldVal.get_type() == sol::type::string)
                                                {
                                                    values.insert_or_assign (
                                                        dotKey, juce::String (fieldVal.as<std::string>()));
                                                }
                                                else if (fieldVal.get_type() == sol::type::number)
                                                {
                                                    values.insert_or_assign (dotKey, fieldVal.as<double>());
                                                }
                                                else if (fieldVal.get_type() == sol::type::boolean)
                                                {
                                                    values.insert_or_assign (dotKey, fieldVal.as<bool>());
                                                }
                                            }
                                        }
                                    });
                            }
                        });

                    // Parse popups table (three-level: END.popups.<name>.<field>)
                    sol::object popupsObj { root["popups"] };

                    if (popupsObj.get_type() == sol::type::table)
                    {
                        sol::table popupsTable { popupsObj.as<sol::table>() };

                        popupsTable.for_each (
                            [this, &warnings] (const sol::object& nameKey, const sol::object& entryVal)
                            {
                                if (nameKey.get_type() == sol::type::string and entryVal.get_type() == sol::type::table)
                                {
                                    const juce::String name { nameKey.as<std::string>() };
                                    sol::table entry { entryVal.as<sol::table>() };
                                    PopupEntry popup;

                                    if (auto cmd { entry.get<sol::optional<std::string>> ("command") })
                                        popup.command = juce::String (*cmd);

                                    if (auto a { entry.get<sol::optional<std::string>> ("args") })
                                        popup.args = juce::String (*a);

                                    if (auto c { entry.get<sol::optional<std::string>> ("cwd") })
                                        popup.cwd = juce::String (*c);

                                    if (auto w { entry.get<sol::optional<double>> ("width") })
                                        popup.width = static_cast<float> (*w);

                                    if (auto h { entry.get<sol::optional<double>> ("height") })
                                        popup.height = static_cast<float> (*h);

                                    if (auto m { entry.get<sol::optional<std::string>> ("modal") })
                                        popup.modal = juce::String (*m);

                                    if (auto g { entry.get<sol::optional<std::string>> ("global") })
                                        popup.global = juce::String (*g);

                                    if (popup.command.isEmpty())
                                    {
                                        warnings.add ("popups." + name + ": missing 'command'");
                                    }
                                    else if (popup.modal.isEmpty() and popup.global.isEmpty())
                                    {
                                        warnings.add ("popups." + name + ": needs 'modal' or 'global' key binding");
                                    }
                                    else
                                    {
                                        popups.insert_or_assign (name, std::move (popup));
                                    }
                                }
                            });
                    }
                }

                if (not warnings.isEmpty())
                {
                    errorOut = configErrorPrefix + warnings.joinIntoString ("\n");
                }

                success = true;
            }
            else
            {
                sol::error err = result;
                errorOut = juce::String (configErrorPrefix) + juce::String (err.what());
            }
        }
    }

    return success;
}

/**
 * @brief Resets to defaults and reloads `end.lua`.
 *
 * Window size and zoom are managed by `AppState` and preserved across
 * config reloads.  Called by `Terminal::Component` on Cmd+R.
 *
 * @return The error/warning string from the reload, or empty if clean.
 * @see Terminal::Component::keyPressed
 */
juce::String Config::reload()
{
    initDefaults();
    clearPopups();
    juce::String error;
    load (getConfigFile(), error);

    if (onReload != nullptr)
        onReload();

    return error;
}

/**
 * @brief Returns a config value as a string.
 * @param key  A `Config::Key` constant.
 * @return The stored string value.
 */
juce::String Config::getString (const juce::String& key) const { return values.at (key).toString(); }

/**
 * @brief Returns a config value as an integer.
 * @param key  A `Config::Key` constant.
 * @return The stored value cast to `int`.
 */
int Config::getInt (const juce::String& key) const { return static_cast<int> (values.at (key)); }

/**
 * @brief Returns a config value as a float.
 * @param key  A `Config::Key` constant.
 * @return The stored value cast to `float`.
 */
float Config::getFloat (const juce::String& key) const { return static_cast<float> (values.at (key)); }

/**
 * @brief Returns a config value as a boolean.
 * @param key  A `Config::Key` constant.
 * @return The stored boolean value.
 */
bool Config::getBool (const juce::String& key) const { return static_cast<bool> (values.at (key)); }

/**
 * @brief Returns a config value parsed as a JUCE Colour.
 * @param key  A `Config::Key` constant whose value is a colour string.
 * @return The parsed `juce::Colour`.
 * @see parseColour
 */
juce::Colour Config::getColour (const juce::String& key) const { return parseColour (values.at (key).toString()); }

/**
 * @brief Builds a fully resolved Theme from the current config values.
 *
 * Reads all 16 ANSI colour keys plus the default foreground, background, and
 * selection colour, and assembles them into a `Theme` struct.
 *
 * @return A `Theme` ready to pass to `Screen::setTheme()`.
 * @see Theme
 * @see Screen::setTheme
 */
Config::Theme Config::buildTheme() const
{
    Theme theme;
    theme.defaultForeground = getColour (Key::coloursForeground);
    theme.defaultBackground = getColour (Key::coloursBackground);
    theme.selectionColour = getColour (Key::coloursSelection);
    theme.ansi = {
        {
         getColour (Key::coloursBlack),
         getColour (Key::coloursRed),
         getColour (Key::coloursGreen),
         getColour (Key::coloursYellow),
         getColour (Key::coloursBlue),
         getColour (Key::coloursMagenta),
         getColour (Key::coloursCyan),
         getColour (Key::coloursWhite),
         getColour (Key::coloursBrightBlack),
         getColour (Key::coloursBrightRed),
         getColour (Key::coloursBrightGreen),
         getColour (Key::coloursBrightYellow),
         getColour (Key::coloursBrightBlue),
         getColour (Key::coloursBrightMagenta),
         getColour (Key::coloursBrightCyan),
         getColour (Key::coloursBrightWhite),
         }
    };
    return theme;
}

/**
 * @brief Parses a colour string in several supported formats.
 *
 * Supported formats:
 * | Format          | Example               | Notes                        |
 * |-----------------|-----------------------|------------------------------|
 * | `#RGB`          | `#F0A`                | Each nibble expanded × 17    |
 * | `#RRGGBB`       | `#FF00AA`             | Full opaque RGB              |
 * | `#AARRGGBB`     | `#CCB3F9F5`           | Alpha in high byte           |
 * | `rgba(r,g,b,a)` | `rgba(255,0,128,0.5)` | Alpha is float [0, 1]        |
 *
 * @param input  The colour string to parse (leading/trailing whitespace trimmed).
 * @return The parsed `juce::Colour`, or `juce::Colours::magenta` on failure.
 * @note Triggers `jassertfalse` on unrecognised format in debug builds.
 */
juce::Colour Config::parseColour (const juce::String& input)
{
    const juce::String trimmed { input.trim() };
    juce::Colour result { juce::Colours::magenta };
    bool parsed { false };

    if (trimmed.startsWithIgnoreCase ("rgba"))
    {
        const int open { trimmed.indexOfChar ('(') };
        const int close { trimmed.indexOfChar (')') };

        if (open >= 0 and close > open)
        {
            juce::StringArray parts;
            parts.addTokens (trimmed.substring (open + 1, close), ",", "");
            parts.trim();

            if (parts.size() == 4)
            {
                const uint8_t r { static_cast<uint8_t> (juce::jlimit (0, 255, parts[0].getIntValue())) };
                const uint8_t g { static_cast<uint8_t> (juce::jlimit (0, 255, parts[1].getIntValue())) };
                const uint8_t b { static_cast<uint8_t> (juce::jlimit (0, 255, parts[2].getIntValue())) };
                const uint8_t a { static_cast<uint8_t> (
                    juce::jlimit (0, 255, juce::roundToInt (parts[3].getFloatValue() * 255.0f))) };
                result = juce::Colour (r, g, b, a);
                parsed = true;
            }
        }

        if (not parsed)
        {
            jassertfalse;
        }
    }
    else if (trimmed.startsWithChar ('#'))
    {
        const juce::String hex { trimmed.substring (1) };

        if (hex.length() == 3)
        {
            const uint8_t r { static_cast<uint8_t> (hex.substring (0, 1).getHexValue32() * 17) };
            const uint8_t g { static_cast<uint8_t> (hex.substring (1, 2).getHexValue32() * 17) };
            const uint8_t b { static_cast<uint8_t> (hex.substring (2, 3).getHexValue32() * 17) };
            result = juce::Colour (r, g, b);
            parsed = true;
        }
        else if (hex.length() == 6)
        {
            const uint8_t r { static_cast<uint8_t> (hex.substring (0, 2).getHexValue32()) };
            const uint8_t g { static_cast<uint8_t> (hex.substring (2, 4).getHexValue32()) };
            const uint8_t b { static_cast<uint8_t> (hex.substring (4, 6).getHexValue32()) };
            result = juce::Colour (r, g, b);
            parsed = true;
        }
        else if (hex.length() == 8)
        {
            const uint8_t a { static_cast<uint8_t> (hex.substring (0, 2).getHexValue32()) };
            const uint8_t r { static_cast<uint8_t> (hex.substring (2, 4).getHexValue32()) };
            const uint8_t g { static_cast<uint8_t> (hex.substring (4, 6).getHexValue32()) };
            const uint8_t b { static_cast<uint8_t> (hex.substring (6, 8).getHexValue32()) };
            result = juce::Colour (r, g, b, a);
            parsed = true;
        }
    }

    if (not parsed)
    {
        jassertfalse;
    }

    return result;
}

const std::unordered_map<juce::String, Config::PopupEntry>& Config::getPopups() const noexcept { return popups; }

void Config::clearPopups() { popups.clear(); }

