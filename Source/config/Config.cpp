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
 *   loadState()             ← overlay state.lua (width, height, zoom)
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
 * Defaults are applied first; `end.lua` values overlay them, and `state.lua`
 * overlays those.
 *
 * @note Called at construction and at the start of `reload()`.
 */
void Config::initDefaults()
{
    values[Key::fontFamily] = "Display Mono";
    values[Key::fontSize] = 14.0f;
    values[Key::fontLigatures] = true;
    values[Key::fontEmbolden] = true;

    values[Key::cursorChar] = juce::String::charToString (static_cast<juce::juce_wchar> (0x2588));
    values[Key::cursorBlink] = true;
    values[Key::cursorBlinkInterval] = 500;

    values[Key::coloursForeground] = "#FFB3F9F5";// frostbite
    // values[Key::coloursForeground] = "#FF4E8C93"; // paradiso
    values[Key::coloursBackground] = "#E0090D12";// bunker
    values[Key::coloursCursor] = "#CCB3F9F5";// frostbite
    values[Key::coloursSelection] = "#8000C8D8";// blueBikini

    values[Key::coloursBlack] = "#FF090D12";// bunker
    values[Key::coloursRed] = "#FFFC704C";// preciousPersimmon
    values[Key::coloursGreen] = "#FFC5F0E9";// gentleCold
    values[Key::coloursYellow] = "#FFF3F5C5";// silkStar
    values[Key::coloursBlue] = "#FF8CC9D9";// dolphin
    values[Key::coloursMagenta] = "#FF519299";// lagoon
    values[Key::coloursCyan] = "#FF699DAA";// tranquiliTeal
    values[Key::coloursWhite] = "#FFB3F9F5";// frostbite

    values[Key::coloursBrightBlack] = "#FF33535B";// mediterranea
    values[Key::coloursBrightRed] = "#FFFC704C";// preciousPersimmon
    values[Key::coloursBrightGreen] = "#FFBAFFFD";// paleSky
    values[Key::coloursBrightYellow] = "#FFFEFFD2";// mattWhite
    values[Key::coloursBrightBlue] = "#FF67DFEF";// poseidonJr
    values[Key::coloursBrightMagenta] = "#FF01C2D2";// caribbeanBlue
    values[Key::coloursBrightCyan] = "#FF00C8D8";// blueBikini
    values[Key::coloursBrightWhite] = "#FFBAFFFD";// paleSky

    values[Key::windowTitle] = ProjectInfo::projectName;
    values[Key::windowWidth] = 640;
    values[Key::windowHeight] = 480;
    values[Key::windowColour] = "#090D12";
    values[Key::windowOpacity] = 0.75f;
    values[Key::windowBlurRadius] = 32.0f;
    values[Key::windowAlwaysOnTop] = true;
    values[Key::windowButtons] = false;
    values[Key::windowZoom] = 1.0f;

    values[Key::overlayFamily] = "Display Mono";
    values[Key::overlaySize] = 28.0f;
    values[Key::overlayColour] = "#4E8C93";

    values[Key::scrollbackNumLines] = 10000;
    values[Key::scrollbackStep] = 5;

    values[Key::keysCopy]      = "cmd+c";
    values[Key::keysPaste]     = "cmd+v";
    values[Key::keysQuit]      = "cmd+q";
    values[Key::keysCloseTab]  = "cmd+w";
    values[Key::keysReload]    = "cmd+r";
    values[Key::keysZoomIn]    = "cmd+=";
    values[Key::keysZoomOut]   = "cmd+-";
    values[Key::keysZoomReset] = "cmd+0";
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

    schema[Key::fontFamily]          = { T::string };
    schema[Key::fontSize]            = { T::number, 1.0,   200.0,   true };
    schema[Key::fontLigatures]       = { T::boolean };
    schema[Key::fontEmbolden]        = { T::boolean };

    schema[Key::cursorChar]          = { T::string };
    schema[Key::cursorBlink]         = { T::boolean };
    schema[Key::cursorBlinkInterval] = { T::number, 100.0, 5000.0,  true };

    schema[Key::coloursForeground]   = { T::string };
    schema[Key::coloursBackground]   = { T::string };
    schema[Key::coloursCursor]       = { T::string };
    schema[Key::coloursSelection]    = { T::string };
    schema[Key::coloursBlack]        = { T::string };
    schema[Key::coloursRed]          = { T::string };
    schema[Key::coloursGreen]        = { T::string };
    schema[Key::coloursYellow]       = { T::string };
    schema[Key::coloursBlue]         = { T::string };
    schema[Key::coloursMagenta]      = { T::string };
    schema[Key::coloursCyan]         = { T::string };
    schema[Key::coloursWhite]        = { T::string };
    schema[Key::coloursBrightBlack]  = { T::string };
    schema[Key::coloursBrightRed]    = { T::string };
    schema[Key::coloursBrightGreen]  = { T::string };
    schema[Key::coloursBrightYellow] = { T::string };
    schema[Key::coloursBrightBlue]   = { T::string };
    schema[Key::coloursBrightMagenta]= { T::string };
    schema[Key::coloursBrightCyan]   = { T::string };
    schema[Key::coloursBrightWhite]  = { T::string };

    schema[Key::windowTitle]         = { T::string };
    schema[Key::windowColour]        = { T::string };
    schema[Key::windowOpacity]       = { T::number, 0.0,   1.0,     true };
    schema[Key::windowBlurRadius]    = { T::number, 0.0,   100.0,   true };
    schema[Key::windowAlwaysOnTop]   = { T::boolean };
    schema[Key::windowButtons]       = { T::boolean };

    schema[Key::overlayFamily]       = { T::string };
    schema[Key::overlaySize]         = { T::number, 1.0,   200.0,   true };
    schema[Key::overlayColour]       = { T::string };

    schema[Key::scrollbackNumLines]  = { T::number, 100.0, 1000000.0, true };
    schema[Key::scrollbackStep]      = { T::number, 1.0,   100.0,   true };

    schema[Key::keysCopy]            = { T::string };
    schema[Key::keysPaste]           = { T::string };
    schema[Key::keysQuit]            = { T::string };
    schema[Key::keysCloseTab]        = { T::string };
    schema[Key::keysReload]          = { T::string };
    schema[Key::keysZoomIn]          = { T::string };
    schema[Key::keysZoomOut]         = { T::string };
    schema[Key::keysZoomReset]       = { T::string };
}

/**
 * @brief Constructs Config: loads defaults, schema, end.lua, then state.lua.
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

    loadState();
}

/**
 * @brief Returns the path to `~/.config/end/end.lua`, creating it if absent.
 *
 * Creates `~/.config/end/` if the directory does not exist, then writes a
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
 * @brief Writes a minimal `END = {}` skeleton to @p file.
 *
 * Gives the user a valid starting point without any overrides so that all
 * defaults apply on first launch.
 *
 * @param file  The file to create; parent directory must already exist.
 */
void Config::writeDefaults (const juce::File& file) const
{
    juce::String content;
    content << "END = {\n";
    content << "}\n";
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
        case sol::type::lua_nil:       return "nil";
        case sol::type::boolean:       return "boolean";
        case sol::type::number:        return "number";
        case sol::type::string:        return "string";
        case sol::type::table:         return "table";
        case sol::type::function:      return "function";
        case sol::type::userdata:      return "userdata";
        case sol::type::lightuserdata: return "lightuserdata";
        case sol::type::thread:        return "thread";
        default:                       return "unknown";
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
bool Config::load (const juce::File& file)
{
    return load (file, loadError);
}

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
        lua.open_libraries (sol::lib::base, sol::lib::string, sol::lib::table,
                            sol::lib::os, sol::lib::debug);

        auto setupResult { lua.safe_script (validationScript, sol::script_pass_on_error) };

        if (setupResult.valid())
        {
            auto result { lua.safe_script_file (file.getFullPathName().toStdString(),
                                                sol::script_pass_on_error) };

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
                                warnings.add ("line " + juce::String (line)
                                              + ": undefined variable '" + name + "'");
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
                            if (groupKey.get_type() == sol::type::string
                                and groupVal.get_type() == sol::type::table)
                            {
                                const juce::String groupName { groupKey.as<std::string>() };
                                sol::table group { groupVal.as<sol::table>() };

                                group.for_each (
                                    [this, &groupName, &warnings] (const sol::object& fieldKey,
                                                                   const sol::object& fieldVal)
                                    {
                                        if (fieldKey.get_type() == sol::type::string)
                                        {
                                            const juce::String fieldName { fieldKey.as<std::string>() };
                                            const juce::String dotKey { groupName + "." + fieldName };

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
                                                    static const std::array<const char*, 3> specTypeNames
                                                        {{ "string", "number", "boolean" }};
                                                    warnings.add ("'" + dotKey + "' expected "
                                                        + specTypeNames.at (static_cast<size_t> (spec.expectedType))
                                                        + ", got "
                                                        + luaTypeName (fieldVal.get_type()));
                                                }
                                                else if (spec.hasRange
                                                         and spec.expectedType == ValueSpec::Type::number)
                                                {
                                                    const double val { fieldVal.as<double>() };

                                                    if (val < spec.minValue or val > spec.maxValue)
                                                    {
                                                        warnings.add ("'" + dotKey + "' value "
                                                            + juce::String (val) + " out of range ["
                                                            + juce::String (spec.minValue) + ", "
                                                            + juce::String (spec.maxValue) + "]");
                                                    }
                                                    else
                                                    {
                                                        values.insert_or_assign (dotKey, val);
                                                    }
                                                }
                                                else if (fieldVal.get_type() == sol::type::string)
                                                {
                                                    values.insert_or_assign (dotKey,
                                                        juce::String (fieldVal.as<std::string>()));
                                                }
                                                else if (fieldVal.get_type() == sol::type::number)
                                                {
                                                    values.insert_or_assign (dotKey,
                                                        fieldVal.as<double>());
                                                }
                                                else if (fieldVal.get_type() == sol::type::boolean)
                                                {
                                                    values.insert_or_assign (dotKey,
                                                        fieldVal.as<bool>());
                                                }
                                            }
                                        }
                                    });
                            }
                        });
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
                errorOut = juce::String (configErrorPrefix)
                           + juce::String (err.what());
            }
        }
    }

    return success;
}

/**
 * @brief Resets to defaults and reloads `end.lua`.
 *
 * Does NOT reload `state.lua`; window size and zoom are preserved across
 * config reloads.  Called by `Terminal::Component` on Cmd+R.
 *
 * @return The error/warning string from the reload, or empty if clean.
 * @see Terminal::Component::keyPressed
 */
juce::String Config::reload()
{
    initDefaults();
    juce::String error;
    load (getConfigFile(), error);
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
    theme.ansi = {{
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
    }};
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
                return juce::Colour (r, g, b, a);
            }
        }

        jassertfalse;
        return juce::Colours::magenta;
    }

    if (trimmed.startsWithChar ('#'))
    {
        const juce::String hex { trimmed.substring (1) };

        if (hex.length() == 3)
        {
            const uint8_t r { static_cast<uint8_t> (hex.substring (0, 1).getHexValue32() * 17) };
            const uint8_t g { static_cast<uint8_t> (hex.substring (1, 2).getHexValue32() * 17) };
            const uint8_t b { static_cast<uint8_t> (hex.substring (2, 3).getHexValue32() * 17) };
            return juce::Colour (r, g, b);
        }

        if (hex.length() == 6)
        {
            const uint8_t r { static_cast<uint8_t> (hex.substring (0, 2).getHexValue32()) };
            const uint8_t g { static_cast<uint8_t> (hex.substring (2, 4).getHexValue32()) };
            const uint8_t b { static_cast<uint8_t> (hex.substring (4, 6).getHexValue32()) };
            return juce::Colour (r, g, b);
        }

        if (hex.length() == 8)
        {
            const uint8_t a { static_cast<uint8_t> (hex.substring (0, 2).getHexValue32()) };
            const uint8_t r { static_cast<uint8_t> (hex.substring (2, 4).getHexValue32()) };
            const uint8_t g { static_cast<uint8_t> (hex.substring (4, 6).getHexValue32()) };
            const uint8_t b { static_cast<uint8_t> (hex.substring (6, 8).getHexValue32()) };
            return juce::Colour (r, g, b, a);
        }
    }

    jassertfalse;
    return juce::Colours::magenta;
}

/**
 * @brief Returns the path to `~/.config/end/state.lua`.
 * @return The state file path (may not exist yet).
 */
juce::File Config::getStateFile() const
{
    return getConfigFile().getParentDirectory().getChildFile ("state.lua");
}

/**
 * @brief Loads `state.lua` and overlays window width, height, and zoom.
 *
 * State values take precedence over both built-in defaults and `end.lua`.
 * Missing or invalid fields in `state.lua` are silently ignored so that a
 * partial or corrupt state file does not prevent startup.
 *
 * @note Called at the end of the constructor, after `end.lua` has been loaded.
 */
void Config::loadState()
{
    auto stateFile { getStateFile() };

    if (stateFile.existsAsFile())
    {
        sol::state lua;
        lua.open_libraries (sol::lib::base);

        auto result { lua.safe_script_file (stateFile.getFullPathName().toStdString(),
                                            sol::script_pass_on_error) };

        if (result.valid())
        {
            sol::table state = lua["state"];

            if (state.valid())
            {
                sol::object w { state["width"] };
                sol::object h { state["height"] };
                sol::object z { state["zoom"] };

                if (w.get_type() == sol::type::number)
                {
                    values.insert_or_assign (Key::windowWidth, w.as<double>());
                }

                if (h.get_type() == sol::type::number)
                {
                    values.insert_or_assign (Key::windowHeight, h.as<double>());
                }

                if (z.get_type() == sol::type::number)
                {
                    values.insert_or_assign (Key::windowZoom, z.as<double>());
                }
            }
        }
    }
}

/**
 * @brief Writes the state table to @p file.
 *
 * File-scope helper used by both `saveWindowSize()` and `saveZoom()`.
 * Writes a Lua table with `width`, `height`, and `zoom` fields.
 *
 * @param file    The state file to overwrite.
 * @param width   Window width in pixels.
 * @param height  Window height in pixels.
 * @param zoom    Zoom multiplier (already clamped by the caller).
 */
static void writeState (const juce::File& file, int width, int height, float zoom)
{
    juce::String content;
    content << "state = {\n";
    content << "\twidth = " << width << ",\n";
    content << "\theight = " << height << ",\n";
    content << "\tzoom = " << juce::String (zoom, 2) << ",\n";
    content << "}\n";
    file.replaceWithText (content);
}

/**
 * @brief Persists the current window dimensions to `state.lua`.
 *
 * Updates the in-memory `windowWidth` / `windowHeight` values and writes
 * `state.lua` so the next launch restores the same size.
 *
 * @param width   Current window width in pixels.
 * @param height  Current window height in pixels.
 * @see saveZoom
 */
void Config::saveWindowSize (int width, int height)
{
    values.insert_or_assign (Key::windowWidth, width);
    values.insert_or_assign (Key::windowHeight, height);
    writeState (getStateFile(), width, height,
                static_cast<float> (values.at (Key::windowZoom)));
}

/**
 * @brief Persists the zoom multiplier to `state.lua`.
 *
 * Clamps @p zoom to `[zoomMin, zoomMax]` before storing.  Reads the current
 * window dimensions from the values map so `writeState()` always writes a
 * consistent triple.
 *
 * @param zoom  Desired zoom multiplier (will be clamped to [zoomMin, zoomMax]).
 * @see saveWindowSize
 */
void Config::saveZoom (float zoom)
{
    const float clamped { juce::jlimit (zoomMin, zoomMax, zoom) };
    values.insert_or_assign (Key::windowZoom, clamped);
    writeState (getStateFile(),
                static_cast<int> (values.at (Key::windowWidth)),
                static_cast<int> (values.at (Key::windowHeight)),
                clamped);
}
