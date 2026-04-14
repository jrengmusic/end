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
 *   initKeys()               ← populate values + schema from single key table
 *   load(end.lua)
 *     lua.safe_script(validationScript)   ← install _undefined tracker
 *     lua.safe_script_file(end.lua)       ← execute user config
 *     iterate _undefined → warnings
 *     iterate END.* → validate + store
 *   (AppState loads end.state and nexus/<uuid>.nexus separately)
 * @endcode
 *
 * ### Colour parsing
 * `parseColour()` handles `#RGB`, `#RGBA`, `#RRGGBB`, `#RRGGBBAA`, and
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

//==============================================================================
/**
 * @brief Registers a single config key with its default value and schema spec.
 *
 * Inserts into both `values` and `schema` in one call, eliminating the
 * parallel initDefaults / initSchema pattern.
 *
 * @param key          The dot-notation key string (e.g. `"font.size"`).
 * @param defaultVal   Default value stored in the values map.
 * @param spec         Type and range constraints for validation.
 */
void Config::addKey (const juce::String& key, const juce::var& defaultVal, Value spec)
{
    values.insert_or_assign (key, defaultVal);
    schema.insert_or_assign (key, std::move (spec));
}

/**
 * @brief Populates both `values` and `schema` from a single unified key table.
 *
 * Replaces the former parallel `initDefaults()` + `initSchema()` pair.  Every
 * key is declared once — default value and validation spec together — making it
 * impossible for the two maps to diverge.
 *
 * Called at construction and at the start of `reload()`.
 */
void Config::initKeys()
{
    using T = Value::Type;

    addKey (Key::fontFamily, "Display Mono", { T::string });
    addKey (Key::fontSize, 12.0, { T::number, 1.0, 200.0, true });
    addKey (Key::fontLigatures, "true", { T::string });
    addKey (Key::fontEmbolden, "true", { T::string });
    addKey (Key::fontLineHeight, 1.0, { T::number, 0.5, 3.0, true });
    addKey (Key::fontCellWidth, 1.0, { T::number, 0.5, 3.0, true });
    addKey (Key::fontDesktopScale, "false", { T::string });

    addKey (Key::cursorChar, juce::String::charToString (static_cast<juce::juce_wchar> (0x2588)), { T::string });
    addKey (Key::cursorBlink, "true", { T::string });
    addKey (Key::cursorBlinkInterval, 500.0, { T::number, 100.0, 5000.0, true });
    addKey (Key::cursorForce, "false", { T::string });
    addKey (Key::coloursForeground, "#A1D6E5", { T::string });///<  skyFall
    addKey (Key::coloursBackground, "#090D12FF", { T::string });///< bunker
    addKey (Key::coloursCursor, "#4E8C93", { T::string });///< paradiso
    addKey (Key::coloursSelection, "#00DDEE20", { T::string });///< fishBoy semi-transparent
    addKey (Key::coloursSelectionCursor, "#00DDEE", { T::string });///< fishBoy

    // ---- ANSI palette (indices 0–15) ----------------------------------------
    // Normal colours
    addKey (Key::coloursBlack, "#090D12", { T::string });///< bunker
    addKey (Key::coloursRed, "#FC704C", { T::string });///< preciousPersimmon
    addKey (Key::coloursGreen, "#C5F0E9", { T::string });///< gentleCold
    addKey (Key::coloursYellow, "#F3F5C5", { T::string });///< silkStar
    addKey (Key::coloursBlue, "#8CC9D9", { T::string });///< dolphin
    addKey (Key::coloursMagenta, "#519299", { T::string });///< lagoon
    addKey (Key::coloursCyan, "#699DAA", { T::string });///< tranquiliTeal
    addKey (Key::coloursWhite, "#DDDDDD", { T::string });///< frostbite
    // Bright colours
    addKey (Key::coloursBrightBlack, "#33535B", { T::string });///< mediterranea
    addKey (Key::coloursBrightRed, "#FC704C", { T::string });///< preciousPersimmon
    addKey (Key::coloursBrightGreen, "#BAFFFD", { T::string });///< paleSky
    addKey (Key::coloursBrightYellow, "#FEFFD2", { T::string });///< mattWhite
    addKey (Key::coloursBrightBlue, "#67DFEF", { T::string });///< poseidonJr
    addKey (Key::coloursBrightMagenta, "#01C2D2", { T::string });///< caribbeanBlue
    addKey (Key::coloursBrightCyan, "#00C8D8", { T::string });///< blueBikini
    addKey (Key::coloursBrightWhite, "#BAFFFD", { T::string });///< paleSky

    addKey (Key::windowTitle, ProjectInfo::projectName, { T::string });
    addKey (Key::windowWidth, 640.0, { T::number, 0.0, 0.0, false });
    addKey (Key::windowHeight, 480.0, { T::number, 0.0, 0.0, false });
    addKey (Key::windowColour, "#090D12", { T::string });///< bunker
    addKey (Key::windowOpacity, 0.75, { T::number, 0.0, 1.0, true });
    addKey (Key::windowBlurRadius, 32.0, { T::number, 0.0, 100.0, true });
    addKey (Key::windowAlwaysOnTop, "false", { T::string });
    addKey (Key::windowButtons, "false", { T::string });
    addKey (Key::windowForceDwm, "true", { T::string });
    addKey (Key::gpuAcceleration, "auto", { T::string });
    addKey (Key::daemon, "false", { T::string });

    addKey (Key::tabFamily, "Display Mono", { T::string });
    addKey (Key::tabSize, 12.0, { T::number, 1.0, 200.0, true });
    addKey (Key::tabForeground, "#00C8D8", { T::string });///< blueBikini
    addKey (Key::tabInactive, "#33535B", { T::string });///< mediterranea
    addKey (Key::tabPosition, "left", { T::string });
    addKey (Key::tabLine, "#2C4144", { T::string });///< littleMermaid
    addKey (Key::tabActive, "#002B35", { T::string });///< midnightDreams
    addKey (Key::tabIndicator, "#01C2D2", { T::string });///< caribbeanBlue
    addKey (Key::menuOpacity, 0.65, { T::number, 0.0, 1.0, true });

    addKey (Key::overlayFamily, "Display Mono", { T::string });
    addKey (Key::overlaySize, 14.0, { T::number, 1.0, 200.0, true });
    addKey (Key::overlayColour, "#4E8C93", { T::string });///< paradiso

#if JUCE_MAC
    addKey (Key::shellProgram, "zsh", { T::string });
    addKey (Key::shellArgs, "-l", { T::string });
#elif JUCE_LINUX
    addKey (Key::shellProgram, "bash", { T::string });
    addKey (Key::shellArgs, "-l", { T::string });
#elif JUCE_WINDOWS
    {
        const auto [shell, args] { findDefaultWindowsShell() };
        addKey (Key::shellProgram, shell, { T::string });
        addKey (Key::shellArgs, args, { T::string });
    }
#endif

    addKey (Key::shellIntegration, "true", { T::string });

    addKey (Key::terminalScrollbackLines, 10000.0, { T::number, 100.0, 1000000.0, true });
    addKey (Key::terminalScrollStep, 5.0, { T::number, 1.0, 100.0, true });
    addKey (Key::terminalPaddingTop, 10.0, { T::number, 0.0, 50.0, true });
    addKey (Key::terminalPaddingRight, 10.0, { T::number, 0.0, 50.0, true });
    addKey (Key::terminalPaddingBottom, 10.0, { T::number, 0.0, 50.0, true });
    addKey (Key::terminalPaddingLeft, 10.0, { T::number, 0.0, 50.0, true });
    addKey (Key::terminalDropMultifiles, "space", { T::string });
    addKey (Key::terminalDropQuoted, "true", { T::string });

#if JUCE_MAC
    addKey (Key::keysCopy, "cmd+c", { T::string });
    addKey (Key::keysPaste, "cmd+v", { T::string });
    addKey (Key::keysQuit, "cmd+q", { T::string });
    addKey (Key::keysCloseTab, "cmd+w", { T::string });
    addKey (Key::keysReload, "cmd+r", { T::string });
    addKey (Key::keysZoomIn, "cmd+=", { T::string });
    addKey (Key::keysZoomOut, "cmd+-", { T::string });
    addKey (Key::keysZoomReset, "cmd+0", { T::string });
    addKey (Key::keysNewWindow, "cmd+n", { T::string });
    addKey (Key::keysNewTab, "cmd+t", { T::string });
    addKey (Key::keysPrevTab, "cmd+[", { T::string });
    addKey (Key::keysNextTab, "cmd+]", { T::string });
#else
    addKey (Key::keysCopy, "ctrl+c", { T::string });
    addKey (Key::keysPaste, "ctrl+v", { T::string });
    addKey (Key::keysQuit, "ctrl+q", { T::string });
    addKey (Key::keysCloseTab, "ctrl+w", { T::string });
    addKey (Key::keysReload, "ctrl+/", { T::string });
    addKey (Key::keysZoomIn, "ctrl+=", { T::string });
    addKey (Key::keysZoomOut, "ctrl+-", { T::string });
    addKey (Key::keysZoomReset, "ctrl+0", { T::string });
    addKey (Key::keysNewWindow, "ctrl+n", { T::string });
    addKey (Key::keysNewTab, "ctrl+t", { T::string });
    addKey (Key::keysPrevTab, "ctrl+[", { T::string });
    addKey (Key::keysNextTab, "ctrl+]", { T::string });
#endif
    addKey (Key::keysSplitHorizontal, juce::String::charToString (static_cast<juce::juce_wchar> ('\\')), { T::string });
    addKey (Key::keysSplitVertical, "-", { T::string });
    addKey (Key::keysPrefix, "`", { T::string });
    addKey (Key::keysPrefixTimeout, 1000.0, { T::number, 100.0, 5000.0, true });
    addKey (Key::keysPaneLeft, "h", { T::string });
    addKey (Key::keysPaneDown, "j", { T::string });
    addKey (Key::keysPaneUp, "k", { T::string });
    addKey (Key::keysPaneRight, "l", { T::string });
    addKey (Key::keysNewline, "shift+return", { T::string });
    addKey (Key::keysActionList, "?", { T::string });
    addKey (Key::keysActionListPosition, "top", { T::string });
    addKey (Key::keysEnterSelection, "[", { T::string });
    addKey (Key::keysEnterOpenFile, "o", { T::string });
    addKey (Key::keysOpenFileNextPage, "space", { T::string });

    addKey (Key::keysSelectionUp, "k", { T::string });
    addKey (Key::keysSelectionDown, "j", { T::string });
    addKey (Key::keysSelectionLeft, "h", { T::string });
    addKey (Key::keysSelectionRight, "l", { T::string });
    addKey (Key::keysSelectionVisual, "v", { T::string });
    addKey (Key::keysSelectionVisualLine, "shift+v", { T::string });
    addKey (Key::keysSelectionVisualBlock, "ctrl+v", { T::string });
    addKey (Key::keysSelectionCopy, "y", { T::string });
    addKey (Key::keysSelectionTop, "g", { T::string });
    addKey (Key::keysSelectionBottom, "shift+g", { T::string });
    addKey (Key::keysSelectionLineStart, "0", { T::string });
    addKey (Key::keysSelectionLineEnd, "$", { T::string });
    addKey (Key::keysSelectionExit, "escape", { T::string });

    addKey (Key::popupCols, 70, { T::number, 1.0, 640.0, true });
    addKey (Key::popupRows, 20, { T::number, 1.0, 480.0, true });
    addKey (Key::popupPosition, "center", { T::string });
    addKey (Key::popupBorderColour, "#4E8C93", { T::string });// paradiso (same as foreground)
    addKey (Key::popupBorderWidth, 1.0, { T::number, 0.0, 10.0, true });

    // ---- Action list (command palette) --------------------------------------
    addKey (Key::keysActionListCloseOnRun, "true", { T::string });
    addKey (Key::actionListWidth, 0.3, { T::number, 0.1, 1.0, true });
    addKey (Key::actionListHeight, 0.3, { T::number, 0.1, 1.0, true });
    addKey (Key::actionListNameFamily, "Display", { T::string });
    addKey (Key::actionListNameSize, 13.0, { T::number, 6.0, 72.0, true });
    addKey (Key::actionListShortcutFamily, "Display Mono Bold", { T::string });
    addKey (Key::actionListShortcutSize, 12.0, { T::number, 6.0, 72.0, true });
    addKey (Key::actionListPaddingTop, 10.0, { T::number, 0.0, 50.0, true });
    addKey (Key::actionListPaddingRight, 10.0, { T::number, 0.0, 50.0, true });
    addKey (Key::actionListPaddingBottom, 10.0, { T::number, 0.0, 50.0, true });
    addKey (Key::actionListPaddingLeft, 10.0, { T::number, 0.0, 50.0, true });
    addKey (Key::actionListNameColour, "#A1D6E5", { T::string });///< skyFall
    addKey (Key::actionListShortcutColour, "#00C8D8", { T::string });///< blueBikini
    addKey (Key::actionListHighlightColour, "#00DDEE20", { T::string });///< fishBoy semi-transparent

    addKey (Key::paneBarColour, "#1B2A31", { T::string });///< dark
    addKey (Key::paneBarHighlight, "#4E8C93", { T::string });///< paradiso

    addKey (Key::coloursStatusBar, "#090D12", { T::string });///< bunker
    addKey (Key::coloursStatusBarLabelBg, "#112130", { T::string });///< trappedDarkness
    addKey (Key::coloursStatusBarLabelFg, "#4E8C93", { T::string });///< paradiso
    addKey (Key::keysStatusBarPosition, "bottom", { T::string });
    addKey (Key::statusBarFontFamily, juce::String ("Display Mono"), { T::string });
    addKey (Key::statusBarFontSize, 12.0, { T::number, 6.0, 48.0, true });
    addKey (Key::statusBarFontStyle, juce::String ("Bold"), { T::string });
    addKey (Key::statusBarSpinnerColour, "#00C8D8", { T::string });///< blueBikini

    addKey (Key::coloursHintLabelBg, "#00FFFF", { T::string });///< cyan
    addKey (Key::coloursHintLabelFg, "#111111", { T::string });///< near-black

    addKey (Key::hyperlinksEditor, "nvim", { T::string });
}

//==============================================================================
/**
 * @brief Constructs Config: initialises key table then loads end.lua.
 *
 * If `end.lua` does not exist it is created with an empty `END = {}` table via
 * `writeDefaults()`.  Any load errors are stored in `loadError` and surfaced by
 * `Terminal::Display` as a startup `MessageOverlay`.
 *
 * @note MESSAGE THREAD — called once from ENDApplication member initialisation.
 */
Config::Config()
{
    initKeys();

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
 * `%%key%%` placeholder with the corresponding value from `initKeys()`,
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

        // Escape backslashes for valid Lua string syntax
        auto str { value.toString() };
        str = str.replace ("\\", "\\\\");
        content = jreng::String::replaceholder (content, placeholder, str);
    }

    file.replaceWithText (content);
}

//==============================================================================
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

//==============================================================================
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
 * @brief Validates a single key/value pair from Lua and stores it if valid.
 *
 * Checks the value type against the schema entry for @p dotKey, emits a
 * warning on mismatch or out-of-range number, and stores the value on success.
 *
 * Free function; operates on the caller's maps directly to avoid exposing
 * sol2 types in Config.h.
 *
 * @param dotKey    The fully-qualified dot-notation key (e.g. `"font.size"`).
 * @param fieldVal  The Lua value to validate.
 * @param values    The config values map to store into.
 * @param schema    The schema map to look up constraints from.
 * @param warnings  Warning list; entries are appended on failure.
 */
static void validateAndStore (const juce::String& dotKey,
                              const sol::object& fieldVal,
                              std::unordered_map<juce::String, juce::var>& values,
                              const std::unordered_map<juce::String, Config::Value>& schema,
                              juce::StringArray& warnings)
{
    static constexpr std::array<const char*, 2> specTypeNames {
        { "string", "number" }
    };

    const bool keyKnown { values.find (dotKey) != values.end() };
    const bool hasSchema { keyKnown and schema.find (dotKey) != schema.end() };

    if (not keyKnown)
    {
        warnings.add ("unknown key '" + dotKey + "'");
    }
    else if (hasSchema)
    {
        const auto& spec { schema.at (dotKey) };

        bool typeOk { false };

        switch (spec.expectedType)
        {
            case Config::Value::Type::number:
                typeOk = (fieldVal.get_type() == sol::type::number);
                break;
            case Config::Value::Type::string:
                typeOk = (fieldVal.get_type() == sol::type::string);
                break;
        }

        if (not typeOk)
        {
            warnings.add ("'" + dotKey + "' expected " + specTypeNames.at (static_cast<size_t> (spec.expectedType))
                          + ", got " + luaTypeName (fieldVal.get_type()));
        }
        else
        {
            switch (spec.expectedType)
            {
                case Config::Value::Type::number:
                {
                    const double val { fieldVal.as<double>() };

                    if (spec.hasRange and (val < spec.minValue or val > spec.maxValue))
                    {
                        warnings.add ("'" + dotKey + "' value " + juce::String (val) + " out of range ["
                                      + juce::String (spec.minValue) + ", " + juce::String (spec.maxValue) + "]");
                    }
                    else
                    {
                        values.insert_or_assign (dotKey, val);
                    }

                    break;
                }
                case Config::Value::Type::string:
                    values.insert_or_assign (dotKey, juce::String (fieldVal.as<std::string>()));
                    break;
            }
        }
    }
}

/**
 * @brief Parses a four-element Lua padding array into flat keys.
 *
 * The Lua config exposes padding as `{ top, right, bottom, left }`.  Reads
 * that array and stores each value under its individual flat key.
 *
 * Free function; operates on the caller's maps directly to avoid exposing
 * sol2 types in Config.h.
 *
 * @param arr     The Lua array table (1-indexed, four numeric elements).
 * @param keys    The four key pointers: top, right, bottom, left.
 * @param values  The config values map to store into.
 * @param schema  The schema map used for clamping range.
 */
static void loadPadding (const sol::table& arr,
                         const std::array<const juce::String*, 4>& keys,
                         std::unordered_map<juce::String, juce::var>& values,
                         const std::unordered_map<juce::String, Config::Value>& schema)
{
    for (int i { 0 }; i < 4; ++i)
    {
        sol::optional<double> v { arr.get<sol::optional<double>> (i + 1) };

        if (v)
        {
            const auto& spec { schema.at (*keys[i]) };
            const double clamped { juce::jlimit (spec.minValue, spec.maxValue, *v) };
            values.insert_or_assign (*keys[i], clamped);
        }
    }
}

/**
 * @brief Parses the `popups` three-level Lua table into `PopupEntry` records.
 *
 * Each named entry under `END.popups` becomes a `PopupEntry`.  Entries missing
 * a `command` field or lacking any key binding are rejected with a warning.
 *
 * Free function; operates on the caller's maps directly to avoid exposing
 * sol2 types in Config.h.
 *
 * @param popupsTable  The `END.popups` Lua table.
 * @param popups       The popup entries map to insert into.
 * @param warnings     Warning list; entries are appended on validation failure.
 */
static void loadPopups (const sol::table& popupsTable,
                        std::unordered_map<juce::String, Config::PopupEntry>& popups,
                        juce::StringArray& warnings)
{
    popupsTable.for_each (
        [&popups, &warnings] (const sol::object& nameKey, const sol::object& entryVal)
        {
            if (nameKey.get_type() == sol::type::string and entryVal.get_type() == sol::type::table)
            {
                const juce::String name { nameKey.as<std::string>() };
                sol::table entry { entryVal.as<sol::table>() };
                Config::PopupEntry popup;

                if (auto cmd { entry.get<sol::optional<std::string>> ("command") })
                    popup.command = juce::String (*cmd);

                if (auto a { entry.get<sol::optional<std::string>> ("args") })
                    popup.args = juce::String (*a);

                if (auto c { entry.get<sol::optional<std::string>> ("cwd") })
                    popup.cwd = juce::String (*c);

                if (auto c { entry.get<sol::optional<int>> ("cols") })
                    popup.cols = *c;

                if (auto r { entry.get<sol::optional<int>> ("rows") })
                    popup.rows = *r;

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

/**
 * @brief Parses the `hyperlinks.handlers` and `hyperlinks.extensions` sub-tables.
 *
 * `handlers` is a string-keyed table mapping extensions to shell commands.
 * `extensions` is an array of extension strings.  Both are optional; missing
 * sub-tables are silently skipped.
 *
 * Free function; operates on the caller's maps directly to avoid exposing
 * sol2 types in Config.h.
 *
 * @param hyperlinksTable  The `END.hyperlinks` Lua table.
 * @param handlers         Map to populate with `{ lowercase extension → command }` entries.
 * @param extensions       Set to populate with lowercase extension strings.
 */
static void loadHyperlinks (const sol::table& hyperlinksTable,
                            std::unordered_map<juce::String, juce::String>& handlers,
                            std::unordered_set<juce::String>& extensions)
{
    hyperlinksTable.for_each (
        [&handlers, &extensions] (const sol::object& fieldKey, const sol::object& fieldVal)
        {
            if (fieldKey.get_type() == sol::type::string)
            {
                const juce::String fieldName { fieldKey.as<std::string>() };

                if (fieldName == "handlers" and fieldVal.get_type() == sol::type::table)
                {
                    fieldVal.as<sol::table>().for_each (
                        [&handlers] (const sol::object& extKey, const sol::object& cmdVal)
                        {
                            if (extKey.get_type() == sol::type::string and cmdVal.get_type() == sol::type::string)
                            {
                                const juce::String ext { juce::String (extKey.as<std::string>()).toLowerCase() };
                                handlers.insert_or_assign (ext, juce::String (cmdVal.as<std::string>()));
                            }
                        });
                }
                else if (fieldName == "extensions" and fieldVal.get_type() == sol::type::table)
                {
                    fieldVal.as<sol::table>().for_each (
                        [&extensions] (const sol::object&, const sol::object& extVal)
                        {
                            if (extVal.get_type() == sol::type::string)
                            {
                                extensions.insert (juce::String (extVal.as<std::string>()).toLowerCase());
                            }
                        });
                }
            }
        });
}

/**
 * @brief Loads and validates a Lua config file.
 *
 * ### Validation steps
 * 1. Injects `validationScript` to track undefined global accesses.
 * 2. Executes the user file with `safe_script_file`.
 * 3. Iterates `_undefined` to collect undefined-variable warnings.
 * 4. Iterates `END.*.*` — dispatches `terminal.padding` to `loadPadding()`,
 *    `popups` to `loadPopups()`, and all other keys to `validateAndStore()`.
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
 * @see Value
 */
bool Config::load (const juce::File& file, juce::String& errorOut)
{
    errorOut = {};
    colourCache.clear();
    hyperlinkHandlers.clear();
    hyperlinkExtensions.clear();
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
                            if (groupKey.get_type() == sol::type::string)
                            {
                                const bool isTableVal { groupVal.get_type() == sol::type::table };

                                if (not isTableVal)
                                {
                                    // Flat scalar at root level (e.g. nexus = true).
                                    const juce::String flatName { groupKey.as<std::string>() };
                                    validateAndStore (flatName, groupVal, values, schema, warnings);
                                }

                                if (isTableVal)
                                {
                                    const juce::String groupName { groupKey.as<std::string>() };
                                    const bool isSpecialGroup { groupName == "popups" or groupName == "hyperlinks" };

                                    if (not isSpecialGroup)
                                    {
                                        sol::table group { groupVal.as<sol::table>() };

                                        group.for_each (
                                            [this, &groupName, &warnings] (
                                                const sol::object& fieldKey, const sol::object& fieldVal)
                                            {
                                                if (fieldKey.get_type() == sol::type::string)
                                                {
                                                    const juce::String fieldName { fieldKey.as<std::string>() };

                                                    // Padding tables are 4-element arrays { top, right, bottom, left }.
                                                    // Dispatched to loadPadding() rather than treated as a scalar.
                                                    const bool isPadding { fieldName == "padding"
                                                                           and fieldVal.get_type()
                                                                                   == sol::type::table };

                                                    if (isPadding)
                                                    {
                                                        static const std::array<const juce::String*, 4>
                                                            terminalPaddingKeys { &Config::Key::terminalPaddingTop,
                                                                                  &Config::Key::terminalPaddingRight,
                                                                                  &Config::Key::terminalPaddingBottom,
                                                                                  &Config::Key::terminalPaddingLeft };

                                                        static const std::array<const juce::String*, 4>
                                                            actionListPaddingKeys {
                                                                &Config::Key::actionListPaddingTop,
                                                                &Config::Key::actionListPaddingRight,
                                                                &Config::Key::actionListPaddingBottom,
                                                                &Config::Key::actionListPaddingLeft
                                                            };

                                                        if (groupName == "terminal")
                                                            loadPadding (fieldVal.as<sol::table>(),
                                                                         terminalPaddingKeys,
                                                                         values,
                                                                         schema);

                                                        if (groupName == "action_list")
                                                            loadPadding (fieldVal.as<sol::table>(),
                                                                         actionListPaddingKeys,
                                                                         values,
                                                                         schema);
                                                    }

                                                    if (not isPadding)
                                                    {
                                                        validateAndStore (groupName + "." + fieldName,
                                                                          fieldVal,
                                                                          values,
                                                                          schema,
                                                                          warnings);
                                                    }
                                                }
                                            });
                                    }
                                }
                            }
                        });

                    sol::object popupsObj { root["popups"] };

                    if (popupsObj.get_type() == sol::type::table)
                        loadPopups (popupsObj.as<sol::table>(), popups, warnings);

                    sol::object hyperlinksObj { root["hyperlinks"] };

                    if (hyperlinksObj.get_type() == sol::type::table)
                    {
                        sol::table hyperlinksTable { hyperlinksObj.as<sol::table>() };

                        // Scalar keys (e.g. editor) go through normal validation.
                        hyperlinksTable.for_each (
                            [this, &warnings] (const sol::object& fieldKey, const sol::object& fieldVal)
                            {
                                if (fieldKey.get_type() == sol::type::string
                                    and fieldVal.get_type() != sol::type::table)
                                {
                                    const juce::String fieldName { fieldKey.as<std::string>() };
                                    validateAndStore ("hyperlinks." + fieldName, fieldVal, values, schema, warnings);
                                }
                            });

                        // Default handler: .md opens in whelmed viewer (user can override).
                        hyperlinkHandlers[".md"] = "whelmed";

                        // Sub-table keys (handlers, extensions) go to loadHyperlinks.
                        loadHyperlinks (hyperlinksTable, hyperlinkHandlers, hyperlinkExtensions);
                    }
                }

                if (not warnings.isEmpty())
                    errorOut = configErrorPrefix + warnings.joinIntoString ("\n");

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

//==============================================================================
/**
 * @brief Resets to defaults and reloads `end.lua`.
 *
 * Window size and zoom are managed by `AppState` and preserved across
 * config reloads.  Called by `Terminal::Display` on Cmd+R.
 *
 * @return The error/warning string from the reload, or empty if clean.
 * @see Terminal::Display::keyPressed
 */
juce::String Config::reload()
{
    colourCache.clear();
    initKeys();
    clearPopups();
    juce::String error;
    load (getConfigFile(), error);

    if (onReload != nullptr)
        onReload();

    return error;
}

//==============================================================================
/**
 * @brief Patches a single key-value pair in end.lua without reloading.
 *
 * Splits the dot-notation key into table name and leaf name, locates the
 * table block in the file, then either replaces the existing value or
 * appends a new line.  String-typed values are quoted; boolean and number
 * values are written unquoted.
 *
 * Does NOT call `onReload`.  Caller handles post-patch actions.
 */
void Config::patchKey (const juce::String& key, const juce::String& value)
{
    const int dotIndex { key.indexOfChar ('.') };
    jassert (dotIndex > 0);

    const juce::String tableName { key.substring (0, dotIndex) };
    const juce::String leafName { key.substring (dotIndex + 1) };

    auto configFile { getConfigFile() };
    juce::String content { configFile.loadFileAsString() };

    // Determine whether the value needs quoting by consulting the schema.
    const auto schemaIt { schema.find (key) };
    const bool isNumberType { schemaIt != schema.end() and schemaIt->second.expectedType == Value::Type::number };
    const juce::String formattedValue { isNumberType ? value : "\"" + value + "\"" };

    // Locate the table block: find a line matching `tableName = {`
    // (with optional leading whitespace/tabs).
    const juce::String tablePattern { tableName + " = {" };
    const int tableStart { content.indexOf (tablePattern) };

    if (tableStart >= 0)
    {
        // Find the closing brace for this table block.
        // Walk from tableStart to find the matching `},` or `}` line.
        const int openBrace { content.indexOf (tableStart, "{") };
        int braceDepth { 1 };
        int pos { openBrace + 1 };

        while (pos < content.length() and braceDepth > 0)
        {
            const juce::juce_wchar ch { content[pos] };

            if (ch == '{')
                ++braceDepth;
            else if (ch == '}')
                --braceDepth;

            if (braceDepth > 0)
                ++pos;
        }

        const int tableEnd { pos };

        // Search for existing leaf within [tableStart, tableEnd).
        const juce::String leafPattern { leafName + " = " };
        const int leafSearch { content.indexOf (tableStart, leafPattern) };
        const int leafPos { (leafSearch >= 0 and leafSearch < tableEnd) ? leafSearch : -1 };

        if (leafPos >= 0)
        {
            // Replace the value token on this line.
            const int valueStart { leafPos + leafPattern.length() };
            int valueEnd { content.indexOf (valueStart, "\n") };

            if (valueEnd < 0)
                valueEnd = content.length();

            // Trim trailing comma and whitespace from the value region.
            juce::String lineRemainder { content.substring (valueStart, valueEnd) };
            const bool hadComma { lineRemainder.trimEnd().endsWithChar (',') };

            // Find where the comment starts (if any) to preserve it.
            // Look for ` --` (Lua comment) outside of quotes.
            int commentStart { -1 };
            bool inQuote { false };

            for (int i { 0 }; i < lineRemainder.length(); ++i)
            {
                if (lineRemainder[i] == '"')
                    inQuote = not inQuote;

                if (not inQuote and i + 1 < lineRemainder.length() and lineRemainder[i] == '-'
                    and lineRemainder[i + 1] == '-')
                {
                    commentStart = i;
                    break;
                }
            }

            juce::String trailingComment;

            if (commentStart >= 0)
                trailingComment = lineRemainder.substring (commentStart);

            juce::String replacement { formattedValue };

            if (hadComma)
                replacement += ",";

            if (trailingComment.isNotEmpty())
                replacement += " " + trailingComment;

            content = content.substring (0, valueStart) + replacement + content.substring (valueEnd);
        }
        else
        {
            // Key not present — append before the closing brace.
            // Use the same indentation as existing lines in this block.
            const juce::String indent { "\t\t" };
            const juce::String newLine { indent + leafName + " = " + formattedValue + ",\n" };

            content = content.substring (0, tableEnd) + newLine + content.substring (tableEnd);
        }

        configFile.replaceWithText (content);
    }
}

//==============================================================================
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
bool Config::getBool (const juce::String& key) const { return values.at (key).toString() == "true"; }

/**
 * @brief Returns the configured font size with the Windows desktop-scale correction
 *        applied per `Key::fontDesktopScale`. On macOS/Linux returns the raw size.
 */
float Config::dpiCorrectedFontSize() const noexcept
{
    const float configured { getFloat (Key::fontSize) };
    float corrected { configured };

#if JUCE_WINDOWS
    const bool honourDesktopScale { getString (Key::fontDesktopScale) == "true" };

    if (not honourDesktopScale)
    {
        const float scale { jreng::Typeface::getDisplayScale() };

        if (scale > 0.0f)
            corrected = configured / scale;
    }
#endif

    return corrected;
}

/**
 * @brief Returns a config value parsed as a JUCE Colour.
 *
 * Results are cached in `colourCache` so repeated calls for the same key
 * pay only a hash-map lookup, not a full string parse.
 *
 * @param key  A `Config::Key` constant whose value is a colour string.
 * @return The parsed `juce::Colour`.
 * @see parseColour
 */
juce::Colour Config::getColour (const juce::String& key) const
{
    auto it { colourCache.find (key) };

    if (it == colourCache.end())
    {
        colourCache.insert_or_assign (key, parseColour (values.at (key).toString()));
        it = colourCache.find (key);
    }

    return it->second;
}

//==============================================================================
/**
 * @brief Builds a fully resolved Theme from the current config values.
 *
 * Reads all 16 ANSI colour keys plus the default foreground, background, and
 * selection colour, and assembles them into a `Theme` struct.
 *
 * The 16 ANSI entries are read via a loop over `ansiKeys` — the same ordering
 * used everywhere the palette is iterated — rather than 16 individual calls.
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
    theme.selectionCursorColour = getColour (Key::coloursSelectionCursor);
    theme.cursorColour = getColour (Key::coloursCursor);

    const juce::String cursorCharStr { getString (Key::cursorChar) };
    theme.cursorCodepoint = cursorCharStr.isNotEmpty() ? static_cast<uint32_t> (cursorCharStr[0]) : 0x2588u;
    theme.cursorForce = getBool (Key::cursorForce);

    theme.hintLabelBg = getColour (Key::coloursHintLabelBg);
    theme.hintLabelFg = getColour (Key::coloursHintLabelFg);

    static const std::array<const juce::String*, 16> ansiKeys {
        {
         &Key::coloursBlack,
         &Key::coloursRed,
         &Key::coloursGreen,
         &Key::coloursYellow,
         &Key::coloursBlue,
         &Key::coloursMagenta,
         &Key::coloursCyan,
         &Key::coloursWhite,
         &Key::coloursBrightBlack,
         &Key::coloursBrightRed,
         &Key::coloursBrightGreen,
         &Key::coloursBrightYellow,
         &Key::coloursBrightBlue,
         &Key::coloursBrightMagenta,
         &Key::coloursBrightCyan,
         &Key::coloursBrightWhite,
         }
    };

    for (int i { 0 }; i < 16; ++i)
        theme.ansi.at (static_cast<size_t> (i)) = getColour (*ansiKeys.at (static_cast<size_t> (i)));

    return theme;
}

//==============================================================================
/**
 * @brief Parses a colour string in several supported formats.
 *
 * Supported formats:
 * | Format          | Example               | Notes                           |
 * |-----------------|-----------------------|---------------------------------|
 * | `#RGB`          | `#F0A`                | Each nibble expanded × 17       |
 * | `#RGBA`         | `#F0A8`               | Each nibble expanded × 17       |
 * | `#RRGGBB`       | `#FF00AA`             | Fully opaque RGB                |
 * | `#RRGGBBAA`     | `#FF00AA80`           | Alpha in low byte               |
 * | `rgba(r,g,b,a)` | `rgba(255,0,128,0.5)` | Alpha is float [0, 1]           |
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
            // #RGB — each nibble expanded to two digits (× 17)
            const uint8_t r { static_cast<uint8_t> (hex.substring (0, 1).getHexValue32() * 17) };
            const uint8_t g { static_cast<uint8_t> (hex.substring (1, 2).getHexValue32() * 17) };
            const uint8_t b { static_cast<uint8_t> (hex.substring (2, 3).getHexValue32() * 17) };
            result = juce::Colour (r, g, b);
            parsed = true;
        }
        else if (hex.length() == 4)
        {
            // #RGBA — each nibble expanded to two digits (× 17)
            const uint8_t r { static_cast<uint8_t> (hex.substring (0, 1).getHexValue32() * 17) };
            const uint8_t g { static_cast<uint8_t> (hex.substring (1, 2).getHexValue32() * 17) };
            const uint8_t b { static_cast<uint8_t> (hex.substring (2, 3).getHexValue32() * 17) };
            const uint8_t a { static_cast<uint8_t> (hex.substring (3, 4).getHexValue32() * 17) };
            result = juce::Colour (r, g, b, a);
            parsed = true;
        }
        else if (hex.length() == 6)
        {
            // #RRGGBB — fully opaque
            const uint8_t r { static_cast<uint8_t> (hex.substring (0, 2).getHexValue32()) };
            const uint8_t g { static_cast<uint8_t> (hex.substring (2, 4).getHexValue32()) };
            const uint8_t b { static_cast<uint8_t> (hex.substring (4, 6).getHexValue32()) };
            result = juce::Colour (r, g, b);
            parsed = true;
        }
        else if (hex.length() == 8)
        {
            // #RRGGBBAA — alpha in low byte
            const uint8_t r { static_cast<uint8_t> (hex.substring (0, 2).getHexValue32()) };
            const uint8_t g { static_cast<uint8_t> (hex.substring (2, 4).getHexValue32()) };
            const uint8_t b { static_cast<uint8_t> (hex.substring (4, 6).getHexValue32()) };
            const uint8_t a { static_cast<uint8_t> (hex.substring (6, 8).getHexValue32()) };
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

//==============================================================================
const std::unordered_map<juce::String, Config::PopupEntry>& Config::getPopups() const noexcept { return popups; }

void Config::clearPopups() { popups.clear(); }

//==============================================================================
/**
 * @brief Returns the handler command for the given file extension.
 *
 * @param extension  Lowercase extension with leading dot (e.g. `".pdf"`).
 * @return The command string, or empty if none is configured.
 */
juce::String Config::getHandler (const juce::String& extension) const noexcept
{
    juce::String result;
    const auto it { hyperlinkHandlers.find (extension) };

    if (it != hyperlinkHandlers.end())
        result = it->second;

    return result;
}

/**
 * @brief Returns `true` if @p extension is user-configured (extensions array or handlers keys).
 *
 * @param extension  Lowercase extension with leading dot (e.g. `".vue"`).
 * @return `true` if the extension is in either user-configured set.
 */
bool Config::isClickableExtension (const juce::String& extension) const noexcept
{
    return hyperlinkExtensions.count (extension) > 0 or hyperlinkHandlers.count (extension) > 0;
}

