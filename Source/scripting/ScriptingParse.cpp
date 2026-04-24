/**
 * @file ScriptingParse.cpp
 * @brief Lua table parse methods for the scripting engine.
 *
 * Contains: Engine::parseKeys(), Engine::parsePopups(),
 * Engine::parseActions(), Engine::parseSelectionKeys().
 *
 * @see Scripting::Engine
 */

#include "Scripting.h"

namespace Scripting
{

//==============================================================================
/**
 * @brief Writes the default action.lua template with platform-appropriate keybindings.
 *
 * Replaces %%placeholder%% tokens in the BinaryData template with platform-conditional
 * default values. Same pattern as Config::writeDefaults for end.lua.
 *
 * @param file  The file to create; parent directory must already exist.
 */
void Engine::writeDefaults (const juce::File& file)
{
    juce::String content { BinaryData::getString ("default_action.lua") };

    if (content.isNotEmpty())
    {
        // Platform-conditional keybindings — macOS uses Cmd, others use Ctrl.
        // Modal keys and selection keys are platform-independent.
#if JUCE_MAC
        content = jam::String::replaceholder (content, "copy",       "cmd+c");
        content = jam::String::replaceholder (content, "paste",      "cmd+v");
        content = jam::String::replaceholder (content, "quit",       "cmd+q");
        content = jam::String::replaceholder (content, "close_tab",  "cmd+w");
        content = jam::String::replaceholder (content, "reload",     "cmd+r");
        content = jam::String::replaceholder (content, "zoom_in",    "cmd+=");
        content = jam::String::replaceholder (content, "zoom_out",   "cmd+-");
        content = jam::String::replaceholder (content, "zoom_reset", "cmd+0");
        content = jam::String::replaceholder (content, "new_window", "cmd+n");
        content = jam::String::replaceholder (content, "new_tab",    "cmd+t");
        content = jam::String::replaceholder (content, "prev_tab",   "cmd+[");
        content = jam::String::replaceholder (content, "next_tab",   "cmd+]");
#else
        content = jam::String::replaceholder (content, "copy",       "ctrl+c");
        content = jam::String::replaceholder (content, "paste",      "ctrl+v");
        content = jam::String::replaceholder (content, "quit",       "ctrl+q");
        content = jam::String::replaceholder (content, "close_tab",  "ctrl+w");
        content = jam::String::replaceholder (content, "reload",     "ctrl+/");
        content = jam::String::replaceholder (content, "zoom_in",    "ctrl+=");
        content = jam::String::replaceholder (content, "zoom_out",   "ctrl+-");
        content = jam::String::replaceholder (content, "zoom_reset", "ctrl+0");
        content = jam::String::replaceholder (content, "new_window", "ctrl+n");
        content = jam::String::replaceholder (content, "new_tab",    "ctrl+t");
        content = jam::String::replaceholder (content, "prev_tab",   "ctrl+[");
        content = jam::String::replaceholder (content, "next_tab",   "ctrl+]");
#endif

        file.replaceWithText (content);
    }
}

//==============================================================================
void Engine::parseKeys()
{
    jam::lua::optional<jam::lua::table> keysOpt { lua["keys"].get<jam::lua::optional<jam::lua::table>>() };

    if (keysOpt.has_value())
    {
        auto& keysTable { keysOpt.value() };
        const auto& mappings { keyMappings };
        const int count { static_cast<int> (mappings.size()) };

        for (int i { 0 }; i < count; ++i)
        {
            jam::lua::optional<std::string> val { keysTable[mappings.at (i).luaKey].get<jam::lua::optional<std::string>>() };

            if (val.has_value())
            {
                KeyBinding binding;
                binding.actionId       = juce::String (mappings.at (i).actionId);
                binding.shortcutString = juce::String (val.value());
                binding.isModal        = mappings.at (i).isModal;
                keyBindings.push_back (std::move (binding));
            }
        }

        jam::lua::optional<std::string> prefix { keysTable["prefix"].get<jam::lua::optional<std::string>>() };

        if (prefix.has_value())
            prefixString = juce::String (prefix.value());

        jam::lua::optional<int> timeout { keysTable["prefix_timeout"].get<jam::lua::optional<int>>() };

        if (timeout.has_value())
            prefixTimeoutMs = timeout.value();
    }
}

//==============================================================================
void Engine::parsePopups()
{
    jam::lua::optional<jam::lua::table> popupsOpt { lua["popups"].get<jam::lua::optional<jam::lua::table>>() };

    if (popupsOpt.has_value())
    {
        auto& popupsTable { popupsOpt.value() };

        for (auto& [key, value] : popupsTable)
        {
            if (value.get_type() == jam::lua::type::table)
            {
                jam::lua::table entry { value };
                PopupEntry popup;
                popup.name = juce::String (key.as<std::string>());

                jam::lua::optional<std::string> command { entry["command"].get<jam::lua::optional<std::string>>() };

                if (command.has_value())
                {
                    popup.command = juce::String (command.value());

                    jam::lua::optional<std::string> args { entry["args"].get<jam::lua::optional<std::string>>() };
                    if (args.has_value()) popup.args = juce::String (args.value());

                    jam::lua::optional<std::string> cwd { entry["cwd"].get<jam::lua::optional<std::string>>() };
                    if (cwd.has_value()) popup.cwd = juce::String (cwd.value());

                    jam::lua::optional<int> cols { entry["cols"].get<jam::lua::optional<int>>() };
                    if (cols.has_value()) popup.cols = cols.value();

                    jam::lua::optional<int> rows { entry["rows"].get<jam::lua::optional<int>>() };
                    if (rows.has_value()) popup.rows = rows.value();

                    jam::lua::optional<std::string> modal { entry["modal"].get<jam::lua::optional<std::string>>() };
                    if (modal.has_value()) popup.modal = juce::String (modal.value());

                    jam::lua::optional<std::string> global { entry["global"].get<jam::lua::optional<std::string>>() };
                    if (global.has_value()) popup.global = juce::String (global.value());

                    popupEntries.push_back (std::move (popup));
                }
            }
        }
    }
}

//==============================================================================
void Engine::parseActions()
{
    jam::lua::optional<jam::lua::table> actionsOpt { lua["actions"].get<jam::lua::optional<jam::lua::table>>() };

    if (actionsOpt.has_value())
    {
        auto& actionsTable { actionsOpt.value() };

        for (auto& [key, value] : actionsTable)
        {
            if (value.get_type() == jam::lua::type::table)
            {
                jam::lua::table entry { value };
                CustomAction action;
                action.id = "lua:" + juce::String (key.as<std::string>());

                jam::lua::optional<std::string> name { entry["name"].get<jam::lua::optional<std::string>>() };
                if (name.has_value()) action.name = juce::String (name.value());

                jam::lua::optional<std::string> desc { entry["description"].get<jam::lua::optional<std::string>>() };
                if (desc.has_value()) action.description = juce::String (desc.value());

                jam::lua::optional<std::string> modal  { entry["modal"].get<jam::lua::optional<std::string>>() };
                jam::lua::optional<std::string> global { entry["global"].get<jam::lua::optional<std::string>>() };

                if (modal.has_value())
                {
                    action.shortcut = juce::String (modal.value());
                    action.isModal  = true;
                }
                else if (global.has_value())
                {
                    action.shortcut = juce::String (global.value());
                    action.isModal  = false;
                }

                jam::lua::optional<jam::lua::protected_function> exec { entry["execute"].get<jam::lua::optional<jam::lua::protected_function>>() };

                if (exec.has_value())
                {
                    action.execute = exec.value();
                    customActions.push_back (std::move (action));
                }
            }
        }
    }
}

//==============================================================================
void Engine::parseSelectionKeys()
{
    jam::lua::optional<jam::lua::table> keysOpt { lua["keys"].get<jam::lua::optional<jam::lua::table>>() };

    if (keysOpt.has_value())
    {
        auto& keysTable { keysOpt.value() };

        auto parse = [] (jam::lua::table& t, const char* field) -> juce::KeyPress
        {
            jam::lua::optional<std::string> val { t[field].get<jam::lua::optional<std::string>>() };
            juce::KeyPress result {};

            if (val.has_value())
                result = Action::Registry::parseShortcut (juce::String (val.value()));

            return result;
        };

        selectionKeys.up               = parse (keysTable, "selection_up");
        selectionKeys.down             = parse (keysTable, "selection_down");
        selectionKeys.left             = parse (keysTable, "selection_left");
        selectionKeys.right            = parse (keysTable, "selection_right");
        selectionKeys.visual           = parse (keysTable, "selection_visual");
        selectionKeys.visualLine       = parse (keysTable, "selection_visual_line");
        selectionKeys.copy             = parse (keysTable, "selection_copy");
        selectionKeys.top              = parse (keysTable, "selection_top");
        selectionKeys.bottom           = parse (keysTable, "selection_bottom");
        selectionKeys.lineStart        = parse (keysTable, "selection_line_start");
        selectionKeys.lineEnd          = parse (keysTable, "selection_line_end");
        selectionKeys.exit             = parse (keysTable, "selection_exit");
        selectionKeys.openFileNextPage = parse (keysTable, "open_file_next_page");
        selectionKeys.globalCopy       = parse (keysTable, "copy");

        // Visual block: special handling for Ctrl on macOS.
        // parseShortcut maps "ctrl" to Cmd on macOS. For visual block we need
        // real ctrlModifier so Ctrl+V doesn't conflict with paste.
        {
            jam::lua::optional<std::string> raw { keysTable["selection_visual_block"].get<jam::lua::optional<std::string>>() };

            if (raw.has_value())
            {
                const juce::String rawStr { raw.value() };
                const bool hasCtrl { rawStr.containsIgnoreCase ("ctrl")
                                     and not rawStr.containsIgnoreCase ("cmd") };

                if (hasCtrl)
                    selectionKeys.visualBlock = juce::KeyPress ('v', juce::ModifierKeys::ctrlModifier, 0);
                else
                    selectionKeys.visualBlock = Action::Registry::parseShortcut (rawStr);
            }
        }
    }
}

} // namespace Scripting
