/**
 * @file ScriptingPatch.cpp
 * @brief Key patch and shortcut query methods for the scripting engine.
 *
 * Contains: Engine::patchKey(), Engine::getActionLuaKey(),
 * Engine::getShortcutString().
 *
 * @see Scripting::Engine
 */

#include "Scripting.h"

namespace Scripting
{

//==============================================================================
void Engine::patchKey (const juce::String& key, const juce::String& value)
{
    const int dotIndex { key.indexOfChar ('.') };
    jassert (dotIndex > 0);

    const juce::String tableName { key.substring (0, dotIndex) };
    const juce::String leafName  { key.substring (dotIndex + 1) };

    const juce::File actionFile { Engine::getActionFile() };
    juce::String content { actionFile.loadFileAsString() };

    // All action.lua values are strings (shortcuts), so always quote.
    const juce::String formattedValue { "\"" + value + "\"" };

    const juce::String tablePattern { tableName + " = {" };
    const int tableStart { content.indexOf (tablePattern) };

    if (tableStart >= 0)
    {
        // Find closing brace for this table block.
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

        actionFile.replaceWithText (content);
    }
}

//==============================================================================
juce::String Engine::getActionLuaKey (const juce::String& actionId) const
{
    const auto* mappings { getKeyMappings() };
    const int count { getKeyMappingCount() };

    juce::String result;

    for (int i { 0 }; i < count and result.isEmpty(); ++i)
    {
        if (juce::String (mappings[i].actionId) == actionId)
            result = "keys." + juce::String (mappings[i].luaKey);
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

    if (leafName == "prefix")
        return prefixString;

    // Find the actionId for this leaf name.
    const auto* mappings { getKeyMappings() };
    const int count { getKeyMappingCount() };
    juce::String targetActionId;

    for (int i { 0 }; i < count and targetActionId.isEmpty(); ++i)
    {
        if (juce::String (mappings[i].luaKey) == leafName)
            targetActionId = juce::String (mappings[i].actionId);
    }

    // Find the binding for this action.
    juce::String result;

    for (const auto& kb : keyBindings)
    {
        if (kb.actionId == targetActionId and result.isEmpty())
            result = kb.shortcutString;
    }

    return result;
}

} // namespace Scripting
