/**
 * @file Config.cpp
 * @brief Implementation of the Lua-driven configuration loader for Whelmed.
 *
 * Uses sol2 (`sol::state`) to execute Lua scripts in a sandboxed environment.
 *
 * ### Load pipeline
 * @code
 * Config()
 *   initKeys()               <- populate values + schema from single key table
 *   load(whelmed.lua)
 *     lua.safe_script_file(whelmed.lua)   <- execute user config
 *     iterate WHELMED.* -> validate + store
 * @endcode
 *
 * @see Whelmed::Config
 * @see Whelmed::Config::Key
 */

#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>
#include "WhelmedConfig.h"

//==============================================================================
/**
 * @brief Registers a single config key with its default value and schema spec.
 *
 * Inserts into both `values` and `schema` in one call.
 *
 * @param key          The flat key string (e.g. `"font_size"`).
 * @param defaultVal   Default value stored in the values map.
 * @param spec         Type and range constraints for validation.
 */
void Whelmed::Config::addKey (const juce::String& key, const juce::var& defaultVal, Whelmed::Config::Value spec)
{
    values.insert_or_assign (key, defaultVal);
    schema.insert_or_assign (key, std::move (spec));
}

/**
 * @brief Populates both `values` and `schema` from a single unified key table.
 *
 * Called at construction and at the start of `reload()`.
 */
void Whelmed::Config::initKeys()
{
    using T = Whelmed::Config::Value::Type;

    addKey (Key::fontFamily, "Display", { T::string });
    addKey (Key::fontStyle, "Medium", { T::string });
    addKey (Key::fontSize, 16.0, { T::number, 8.0, 72.0, true });
    addKey (Key::codeFamily, "Display Mono", { T::string });
    addKey (Key::codeSize, 12.0, { T::number, 8.0, 72.0, true });
    addKey (Key::codeStyle, "Medium", { T::string });
    addKey (Key::h1Size, 28.0, { T::number, 8.0, 72.0, true });
    addKey (Key::h2Size, 28.0, { T::number, 8.0, 72.0, true });
    addKey (Key::h3Size, 24.0, { T::number, 8.0, 72.0, true });
    addKey (Key::h4Size, 20.0, { T::number, 8.0, 72.0, true });
    addKey (Key::h5Size, 18.0, { T::number, 8.0, 72.0, true });
    addKey (Key::h6Size, 16.0, { T::number, 8.0, 72.0, true });
    addKey (Key::lineHeight, 1.5, { T::number, 0.8, 3.0, true });

    addKey (Key::background, juce::String ("0D141CFF"), { T::string });
    addKey (Key::bodyColour, juce::String ("B3F9F5FF"), { T::string });
    addKey (Key::codeColour, juce::String ("00D0FFFF"), { T::string });
    addKey (Key::linkColour, juce::String ("01C2D2FF"), { T::string });
    addKey (Key::h1Colour, juce::String ("D4C8A0FF"), { T::string });
    addKey (Key::h2Colour, juce::String ("D4C8A0FF"), { T::string });
    addKey (Key::h3Colour, juce::String ("D4C8A0FF"), { T::string });
    addKey (Key::h4Colour, juce::String ("D4C8A0FF"), { T::string });
    addKey (Key::h5Colour, juce::String ("D4C8A0FF"), { T::string });
    addKey (Key::h6Colour, juce::String ("D4C8A0FF"), { T::string });
    addKey (Key::codeFenceBackground, juce::String ("090D12FF"), { T::string });
    addKey (Key::progressBackground, juce::String ("1A1A1AFF"), { T::string });
    addKey (Key::progressForeground, juce::String ("4488CCFF"), { T::string });
    addKey (Key::progressTextColour, juce::String ("CCCCCCFF"), { T::string });
    addKey (Key::progressSpinnerColour, juce::String ("4488CCFF"), { T::string });

    addKey (Key::paddingTop, 10.0, { T::number, 0.0, 200.0, true });
    addKey (Key::paddingRight, 10.0, { T::number, 0.0, 200.0, true });
    addKey (Key::paddingBottom, 10.0, { T::number, 0.0, 200.0, true });
    addKey (Key::paddingLeft, 10.0, { T::number, 0.0, 200.0, true });

    addKey (Key::tokenError, juce::String ("F74A4AFF"), { T::string });
    addKey (Key::tokenComment, juce::String ("6080C0FF"), { T::string });
    addKey (Key::tokenKeyword, juce::String ("1919FFFF"), { T::string });
    addKey (Key::tokenOperator, juce::String ("B0B0B0FF"), { T::string });
    addKey (Key::tokenIdentifier, juce::String ("00C6FFFF"), { T::string });
    addKey (Key::tokenInteger, juce::String ("00FF00FF"), { T::string });
    addKey (Key::tokenFloat, juce::String ("00FF00FF"), { T::string });
    addKey (Key::tokenString, juce::String ("FFC0C0FF"), { T::string });
    addKey (Key::tokenBracket, juce::String ("80FFFFFF"), { T::string });
    addKey (Key::tokenPunctuation, juce::String ("FF9080FF"), { T::string });
    addKey (Key::tokenPreprocessor, juce::String ("9AFF00FF"), { T::string });

    addKey (Key::tableBackground, juce::String ("090D12FF"), { T::string });
    addKey (Key::tableHeaderBackground, juce::String ("112130FF"), { T::string });
    addKey (Key::tableRowAlt, juce::String ("0D141CFF"), { T::string });
    addKey (Key::tableBorderColour, juce::String ("2C4144FF"), { T::string });
    addKey (Key::tableHeaderText, juce::String ("BAFFFDFF"), { T::string });
    addKey (Key::tableCellText, juce::String ("B3F9F5FF"), { T::string });

    addKey (Key::scrollbarThumb, juce::String ("2C4144FF"), { T::string });// littleMermaid
    addKey (Key::scrollbarTrack, juce::String ("0D141CFF"), { T::string });// corbeau
    addKey (Key::scrollbarBackground, juce::String ("0D141CFF"), { T::string });// corbeau

    addKey (Key::scrollDown, juce::String ("j"), { T::string });
    addKey (Key::scrollUp, juce::String ("k"), { T::string });
    addKey (Key::scrollTop, juce::String ("gg"), { T::string });
    addKey (Key::scrollBottom, juce::String ("G"), { T::string });
    addKey (Key::scrollStep, 50.0, { T::number, 1.0, 2000.0, true });

    addKey (Key::selectionColour, juce::String ("00C8D880"), { T::string });
}

//==============================================================================
/**
 * @brief Constructs Config: initialises key table then loads whelmed.lua.
 *
 * If `whelmed.lua` does not exist it is created with defaults via
 * `writeDefaults()`.  Any load errors are stored in `loadError`.
 *
 * @note MESSAGE THREAD — called once from ENDApplication member initialisation.
 */
Whelmed::Config::Config()
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
 * - All platforms: `~/.config/end/whelmed.lua`
 *
 * Creates the directory if it does not exist, then writes defaults if
 * `whelmed.lua` is missing.
 *
 * @return The config file; guaranteed to exist after this call.
 */
juce::File Whelmed::Config::getConfigFile() const
{
    auto configDir { juce::File::getSpecialLocation (juce::File::userHomeDirectory).getChildFile (".config/end") };

    if (not configDir.exists())
        configDir.createDirectory();

    auto configFile { configDir.getChildFile ("whelmed.lua") };

    if (not configFile.existsAsFile())
        writeDefaults (configFile);

    return configFile;
}

/**
 * @brief Writes a default `whelmed.lua` from the embedded template.
 *
 * Reads the `default_whelmed.lua` template from BinaryData, replaces every
 * `%%key%%` placeholder with the corresponding value from `initKeys()`,
 * and writes the result to @p file.
 *
 * @param file  The file to create; parent directory must already exist.
 */
void Whelmed::Config::writeDefaults (const juce::File& file) const
{
    juce::String content { BinaryData::getString ("default_whelmed.lua") };

    for (const auto& [key, value] : values)
    {
        auto str { value.toString() };
        str = str.replace ("\\", "\\\\");
        content = jreng::String::replaceholder (content, key, str);
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
bool Whelmed::Config::load (const juce::File& file) { return load (file, loadError); }

/**
 * @brief Explodes a 4-element Lua padding array into the four flat scalar keys.
 *
 * Expects @p arr to be a Lua array `{ top, right, bottom, left }` (1-indexed).
 * Each element is validated against the schema range and stored if valid.
 *
 * @param arr     The Lua table containing the padding values.
 * @param values  The runtime value store to write into.
 * @param schema  The type/range schema to validate against.
 */
static void loadPadding (const sol::table& arr,
                         std::unordered_map<juce::String, juce::var>& values,
                         const std::unordered_map<juce::String, Whelmed::Config::Value>& schema)
{
    static const std::array<const juce::String*, 4> paddingKeys { &Whelmed::Config::Key::paddingTop,
                                                                  &Whelmed::Config::Key::paddingRight,
                                                                  &Whelmed::Config::Key::paddingBottom,
                                                                  &Whelmed::Config::Key::paddingLeft };

    for (int i { 0 }; i < 4; ++i)
    {
        sol::optional<double> v { arr.get<sol::optional<double>> (i + 1) };

        if (v)
        {
            const auto& spec { schema.at (*paddingKeys.at (i)) };
            const bool inRange { not spec.hasRange or (*v >= spec.minValue and *v <= spec.maxValue) };

            if (inRange)
                values.insert_or_assign (*paddingKeys.at (i), *v);
        }
    }
}

/**
 * @brief Validates a single key/value pair from Lua and stores it if valid.
 *
 * File-scope static — keeps sol2 types out of the header (matches END's pattern).
 *
 * @param key       The flat key string (e.g. `"font_size"`).
 * @param value     The Lua value to validate.
 * @param values    The runtime value store to write into.
 * @param schema    The type/range schema to validate against.
 * @param errorOut  Warning string; entries are appended on failure.
 */
static void validateAndStore (const juce::String& key,
                              const sol::object& value,
                              std::unordered_map<juce::String, juce::var>& values,
                              const std::unordered_map<juce::String, Whelmed::Config::Value>& schema,
                              juce::String& errorOut)
{
    static constexpr std::array<const char*, 2> specTypeNames {
        { "string", "number" }
    };

    const bool keyKnown { values.find (key) != values.end() };
    const bool hasSchema { keyKnown and schema.find (key) != schema.end() };

    if (not keyKnown)
    {
        if (not errorOut.isEmpty())
            errorOut += "\n";

        errorOut += "unknown key '" + key + "'";
    }
    else if (hasSchema)
    {
        const auto& spec { schema.at (key) };
        bool typeOk { false };

        switch (spec.expectedType)
        {
            case Whelmed::Config::Value::Type::number:
                typeOk = (value.get_type() == sol::type::number);
                break;
            case Whelmed::Config::Value::Type::string:
                typeOk = (value.get_type() == sol::type::string);
                break;
        }

        if (not typeOk)
        {
            if (not errorOut.isEmpty())
                errorOut += "\n";

            errorOut += "'" + key + "' expected " + specTypeNames.at (static_cast<size_t> (spec.expectedType))
                        + ", got " + luaTypeName (value.get_type());
        }
        else
        {
            switch (spec.expectedType)
            {
                case Whelmed::Config::Value::Type::number:
                {
                    const double val { value.as<double>() };

                    if (spec.hasRange and (val < spec.minValue or val > spec.maxValue))
                    {
                        if (not errorOut.isEmpty())
                            errorOut += "\n";

                        errorOut += "'" + key + "' value " + juce::String (val) + " out of range ["
                                    + juce::String (spec.minValue) + ", " + juce::String (spec.maxValue) + "]";
                    }
                    else
                    {
                        values.insert_or_assign (key, val);
                    }

                    break;
                }
                case Whelmed::Config::Value::Type::string:
                    values.insert_or_assign (key, juce::String (value.as<std::string>()));
                    break;
            }
        }
    }
}

/**
 * @brief Loads and validates a Lua config file.
 *
 * Executes the user file with `safe_script_file`, then iterates the flat
 * `WHELMED` table, dispatching each field to `validateAndStore()`.
 *
 * Non-fatal warnings are appended to @p errorOut but do not prevent a
 * @c true return.  A fatal Lua parse/runtime error causes @c false.
 *
 * @param file      The Lua config file to load.
 * @param errorOut  Receives the combined error/warning string on return.
 * @return @c true if the file was parsed without a fatal Lua error.
 */
bool Whelmed::Config::load (const juce::File& file, juce::String& errorOut)
{
    errorOut = {};
    bool success { false };

    if (file.existsAsFile())
    {
        sol::state lua;
        lua.open_libraries (
            sol::lib::base, sol::lib::string, sol::lib::table, sol::lib::os, sol::lib::debug, sol::lib::package);

        auto result { lua.safe_script_file (file.getFullPathName().toStdString(), sol::script_pass_on_error) };

        if (result.valid())
        {
            sol::table whelmedTable = lua["WHELMED"];

            if (whelmedTable.valid())
            {
                for (const auto& [fieldKey, fieldVal] : whelmedTable)
                {
                    if (fieldKey.is<std::string>())
                    {
                        const auto key { juce::String (fieldKey.as<std::string>()) };

                        if (key == "padding" and fieldVal.get_type() == sol::type::table)
                        {
                            loadPadding (fieldVal.as<sol::table>(), values, schema);
                        }
                        else
                        {
                            validateAndStore (key, fieldVal, values, schema, errorOut);
                        }
                    }
                }
            }

            success = true;
        }
        else
        {
            sol::error err = result;
            errorOut = juce::String (err.what());
        }
    }

    return success;
}

//==============================================================================
/**
 * @brief Resets to defaults and reloads `whelmed.lua`.
 *
 * Called on config reload.  Fires `onReload` after re-parsing the file.
 *
 * @return The error/warning string from the reload, or empty if clean.
 */
juce::String Whelmed::Config::reload()
{
    initKeys();
    juce::String error;
    load (getConfigFile(), error);

    if (onReload != nullptr)
        onReload();

    return error;
}

//==============================================================================
/**
 * @brief Returns a config value as a string.
 * @param key  A `Whelmed::Config::Key` constant.
 * @return The stored string value.
 */
juce::String Whelmed::Config::getString (const juce::String& key) const { return values.at (key).toString(); }

/**
 * @brief Returns a config value as a float.
 * @param key  A `Whelmed::Config::Key` constant.
 * @return The stored value cast to `float`.
 */
float Whelmed::Config::getFloat (const juce::String& key) const { return static_cast<float> (values.at (key)); }

/**
 * @brief Returns a config value as an int.
 * @param key  A `Whelmed::Config::Key` constant.
 * @return The stored value cast to `int`.
 */
int Whelmed::Config::getInt (const juce::String& key) const { return static_cast<int> (values.at (key)); }

/**
 * @brief Returns a config value as a juce::Colour.
 *
 * Config stores colours as RRGGBBAA hex strings (8 chars).  JUCE parses
 * colour strings in AARRGGBB order, so the alpha pair is moved to the front.
 *
 * @param key  A `Whelmed::Config::Key` constant for a colour field.
 * @return The parsed colour, or opaque black on malformed input.
 */
juce::Colour Whelmed::Config::getColour (const juce::String& key) const
{
    const juce::String rrggbbaa { values.at (key).toString() };
    jassert (rrggbbaa.length() == 8);
    const juce::String aarrggbb { rrggbbaa.substring (6) + rrggbbaa.substring (0, 6) };
    return juce::Colour::fromString (aarrggbb);
}

/**
 * @brief Returns the last load error/warning string.
 * @return Reference to the internal error string.
 */
const juce::String& Whelmed::Config::getLoadError() const noexcept { return loadError; }

