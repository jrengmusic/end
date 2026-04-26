/**
 * @file EngineParse.cpp
 * @brief Lua table parse methods for lua::Engine keys, popups, and actions.
 *
 * Contains: Engine::parseKeys(), Engine::parseSelectionKeys(),
 * Engine::parsePopups(), Engine::parseActions().
 *
 * @see lua::Engine
 */

#include <jam_lua/jam_lua.h>

#include "Engine.h"
#include "../action/Action.h"

namespace lua
{

//==============================================================================
void Engine::parseKeys()
{
    jam::lua::Value keysTable { lua["END"]["keys"] };

    if (keysTable.isTable())
    {
        const auto& mappings { keyMappings };
        const int count { static_cast<int> (mappings.size()) };

        for (int i { 0 }; i < count; ++i)
        {
            auto val { keysTable[mappings.at (i).luaKey].optional<juce::String>() };

            if (val.has_value())
            {
                Keys::Binding binding;
                binding.actionId       = juce::String (mappings.at (i).actionId);
                binding.shortcutString = val.value();
                binding.isModal        = mappings.at (i).isModal;
                keys.bindings.push_back (std::move (binding));
            }
        }

        auto prefix { keysTable["prefix"].optional<juce::String>() };
        if (prefix.has_value()) keys.prefix = prefix.value();

        auto timeout { keysTable["prefix_timeout"].optional<int>() };
        if (timeout.has_value()) keys.prefixTimeout = timeout.value();
    }
}

//==============================================================================
void Engine::parseSelectionKeys()
{
    jam::lua::Value keysTable { lua["END"]["keys"] };

    if (keysTable.isTable())
    {
        auto parse = [] (jam::lua::Value& t, const char* field) -> juce::KeyPress
        {
            auto val { t[field].optional<juce::String>() };
            juce::KeyPress result {};

            if (val.has_value())
                result = ::Action::Registry::parseShortcut (val.value());

            return result;
        };

        keys.selection.up               = parse (keysTable, "selection_up");
        keys.selection.down             = parse (keysTable, "selection_down");
        keys.selection.left             = parse (keysTable, "selection_left");
        keys.selection.right            = parse (keysTable, "selection_right");
        keys.selection.visual           = parse (keysTable, "selection_visual");
        keys.selection.visualLine       = parse (keysTable, "selection_visual_line");
        keys.selection.copy             = parse (keysTable, "selection_copy");
        keys.selection.top              = parse (keysTable, "selection_top");
        keys.selection.bottom           = parse (keysTable, "selection_bottom");
        keys.selection.lineStart        = parse (keysTable, "selection_line_start");
        keys.selection.lineEnd          = parse (keysTable, "selection_line_end");
        keys.selection.exit             = parse (keysTable, "selection_exit");
        keys.selection.openFileNextPage = parse (keysTable, "open_file_next_page");
        keys.selection.globalCopy       = parse (keysTable, "copy");

        // Visual block: special handling for Ctrl on macOS.
        // parseShortcut maps "ctrl" to Cmd on macOS. For visual block we need
        // real ctrlModifier so Ctrl+V doesn't conflict with paste.
        {
            auto raw { keysTable["selection_visual_block"].optional<juce::String>() };

            if (raw.has_value())
            {
                const juce::String rawStr { raw.value() };
                const bool hasCtrl { rawStr.containsIgnoreCase ("ctrl")
                                     and not rawStr.containsIgnoreCase ("cmd") };

                if (hasCtrl)
                    keys.selection.visualBlock = juce::KeyPress ('v', juce::ModifierKeys::ctrlModifier, 0);
                else
                    keys.selection.visualBlock = ::Action::Registry::parseShortcut (rawStr);
            }
        }
    }
}

//==============================================================================
void Engine::parsePopups()
{
    jam::lua::Value popupsTable { lua["END"]["popups"] };

    if (popupsTable.isTable())
    {
        // Read defaults sub-table first
        jam::lua::Value defaultsTable { popupsTable["defaults"] };

        if (defaultsTable.isTable())
        {
            auto defaultCols { defaultsTable["cols"].optional<int>() };
            if (defaultCols.has_value()) popup.defaultCols = defaultCols.value();

            auto defaultRows { defaultsTable["rows"].optional<int>() };
            if (defaultRows.has_value()) popup.defaultRows = defaultRows.value();

            auto defaultPosition { defaultsTable["position"].optional<juce::String>() };
            if (defaultPosition.has_value()) popup.defaultPosition = defaultPosition.value();
        }

        // Iterate remaining entries (skip "defaults" key)
        popupsTable.forEach ([this] (const jam::lua::Value& key, const jam::lua::Value& value)
        {
            if (value.getType() == jam::lua::Type::table
                and key.getType() == jam::lua::Type::string
                and key.to<juce::String>() != "defaults")
            {
                Popup::Entry popupEntry;
                popupEntry.name = key.to<juce::String>();

                auto command { value["command"].optional<juce::String>() };

                if (command.has_value())
                {
                    popupEntry.command = command.value();

                    auto args { value["args"].optional<juce::String>() };
                    if (args.has_value()) popupEntry.args = args.value();

                    auto cwd { value["cwd"].optional<juce::String>() };
                    if (cwd.has_value()) popupEntry.cwd = cwd.value();

                    auto cols { value["cols"].optional<int>() };
                    if (cols.has_value()) popupEntry.cols = cols.value();

                    auto rows { value["rows"].optional<int>() };
                    if (rows.has_value()) popupEntry.rows = rows.value();

                    auto modal { value["modal"].optional<juce::String>() };
                    if (modal.has_value()) popupEntry.modal = modal.value();

                    auto global { value["global"].optional<juce::String>() };
                    if (global.has_value()) popupEntry.global = global.value();

                    popup.entries.push_back (std::move (popupEntry));
                }
            }
        });
    }
}

//==============================================================================
void Engine::parseActions()
{
    jam::lua::Value actionsTable { lua["END"]["actions"] };

    if (actionsTable.isTable())
    {
        actionsTable.forEach ([this] (const jam::lua::Value& key, const jam::lua::Value& value)
        {
            if (value.getType() == jam::lua::Type::table)
            {
                Action::Entry actionEntry;
                actionEntry.id = "lua:" + key.to<juce::String>();

                auto name { value["name"].optional<juce::String>() };
                if (name.has_value()) actionEntry.name = name.value();

                auto desc { value["description"].optional<juce::String>() };
                if (desc.has_value()) actionEntry.description = desc.value();

                auto modal  { value["modal"].optional<juce::String>() };
                auto global { value["global"].optional<juce::String>() };

                if (modal.has_value())
                {
                    actionEntry.shortcut = modal.value();
                    actionEntry.isModal  = true;
                }
                else if (global.has_value())
                {
                    actionEntry.shortcut = global.value();
                    actionEntry.isModal  = false;
                }

                jam::lua::Value execVal { value["execute"] };

                if (execVal.isFunction())
                {
                    actionEntry.execute = jam::lua::Function (std::move (execVal));
                    action.entries.push_back (std::move (actionEntry));
                }
            }
        });
    }
}

} // namespace lua
