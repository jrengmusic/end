/**
 * @file EnginePatch.cpp
 * @brief Key patch and shortcut query methods for lua::Engine.
 *
 * Contains: Engine::patchKey(), Engine::getActionLuaKey(),
 * Engine::getShortcutString().
 *
 * @see lua::Engine
 */

#include <jam_lua/jam_lua.h>

#include "Engine.h"

namespace lua
{

//==============================================================================
void Engine::patchKey (const juce::String& key, const juce::String& value)
{
    const int dotIndex { key.indexOfChar ('.') };
    jassert (dotIndex > 0);

    const juce::String leafName { key.substring (dotIndex + 1) };

    const juce::File configDir { getConfigPath() };
    const juce::File keysFile { configDir.getChildFile ("keys.lua") };
    juce::String content { keysFile.loadFileAsString() };

    // All keys.lua values are strings (shortcuts), so always quote.
    const juce::String formattedValue { "\"" + value + "\"" };

    // keys.lua uses `return {` as the table opener — no named table prefix.
    const juce::String tablePattern { "return {" };
    const int tableStart { content.indexOf (tablePattern) };

    if (tableStart >= 0)
    {
        // Find closing brace for the return block.
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
            // Replace the value on this line.
            const int valueStart { leafPos + leafPattern.length() };
            int valueEnd { content.indexOf (valueStart, "\n") };

            if (valueEnd < 0)
                valueEnd = content.length();

            juce::String lineRemainder { content.substring (valueStart, valueEnd) };
            const bool hadComma { lineRemainder.trimEnd().endsWithChar (',') };

            // Preserve trailing Lua comment if present.
            int commentStart { -1 };
            bool inQuote { false };

            for (int i { 0 }; i < lineRemainder.length(); ++i)
            {
                if (lineRemainder[i] == '"')
                    inQuote = not inQuote;

                if (not inQuote and i + 1 < lineRemainder.length()
                    and lineRemainder[i] == '-' and lineRemainder[i + 1] == '-')
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
            const juce::String indent { "\t" };
            const juce::String newLine { indent + leafName + " = " + formattedValue + ",\n" };

            content = content.substring (0, tableEnd) + newLine + content.substring (tableEnd);
        }

        keysFile.replaceWithText (content);
    }
}

//==============================================================================
juce::String Engine::getActionLuaKey (const juce::String& actionId) const
{
    const auto& mappings { keyMappings };
    const int count { static_cast<int> (mappings.size()) };

    juce::String result;

    for (int i { 0 }; i < count and result.isEmpty(); ++i)
    {
        if (juce::String (mappings.at (i).actionId) == actionId)
            result = "keys." + juce::String (mappings.at (i).luaKey);
    }

    if (actionId == "prefix" and result.isEmpty())
        result = "keys.prefix";

    return result;
}

//==============================================================================
juce::String Engine::getShortcutString (const juce::String& actionLuaKey) const
{
    const int dotIndex { actionLuaKey.indexOfChar ('.') };
    const juce::String leafName { dotIndex >= 0 ? actionLuaKey.substring (dotIndex + 1) : actionLuaKey };

    juce::String result;

    if (leafName == "prefix")
    {
        result = keys.prefix;
    }
    else
    {
        // Find the actionId for this leaf name.
        const auto& mappings { keyMappings };
        const int count { static_cast<int> (mappings.size()) };
        juce::String targetActionId;

        for (int i { 0 }; i < count and targetActionId.isEmpty(); ++i)
        {
            if (juce::String (mappings.at (i).luaKey) == leafName)
                targetActionId = juce::String (mappings.at (i).actionId);
        }

        // Find the binding for this action.
        for (const auto& kb : keys.bindings)
        {
            if (kb.actionId == targetActionId and result.isEmpty())
                result = kb.shortcutString;
        }
    }

    return result;
}

} // namespace lua
