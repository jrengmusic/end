/**
 * @file Scripting.cpp
 * @brief Core lifecycle of the Lua scripting engine.
 *
 * Contains: file helpers, static key mapping table, Engine constructor/destructor,
 * Engine::load(), Engine::registerActions(), Engine::buildKeyMap(),
 * accessors, and the file watcher callback.
 *
 * Parse methods are in ScriptingParse.cpp.
 * Patch and shortcut query methods are in ScriptingPatch.cpp.
 *
 * @see Scripting::Engine
 */

#include "Scripting.h"
#include "../config/Config.h"

namespace Scripting
{

//==============================================================================
juce::File Engine::getActionFile()
{
    return juce::File::getSpecialLocation (juce::File::userHomeDirectory)
               .getChildFile (".config/end/action.lua");
}

// clang-format off
static const Engine::KeyMapping keyMappingsData[]
{
    { "copy",             "copy",             false },
    { "paste",            "paste",            false },
    { "quit",             "quit",             false },
    { "close_tab",        "close_tab",        false },
    { "reload",           "reload_config",    false },
    { "zoom_in",          "zoom_in",          false },
    { "zoom_out",         "zoom_out",         false },
    { "zoom_reset",       "zoom_reset",       false },
    { "new_window",       "new_window",       false },
    { "new_tab",          "new_tab",          false },
    { "prev_tab",         "prev_tab",         false },
    { "next_tab",         "next_tab",         false },
    { "split_horizontal", "split_horizontal", true  },
    { "split_vertical",   "split_vertical",   true  },
    { "pane_left",        "pane_left",        true  },
    { "pane_down",        "pane_down",        true  },
    { "pane_up",          "pane_up",          true  },
    { "pane_right",       "pane_right",       true  },
    { "newline",          "newline",          false },
    { "action_list",      "action_list",      true  },
    { "enter_selection",  "enter_selection",  true  },
    { "enter_open_file",  "enter_open_file",  true  },
};
// clang-format on

//==============================================================================
const Engine::KeyMapping* Engine::getKeyMappings() noexcept
{
    return keyMappingsData;
}

int Engine::getKeyMappingCount() noexcept
{
    return static_cast<int> (std::size (keyMappingsData));
}

//==============================================================================
Engine::Engine (DisplayCallbacks inDisplayCallbacks, PopupCallbacks inPopupCallbacks)
    : displayCallbacks (std::move (inDisplayCallbacks))
    , popupCallbacks (std::move (inPopupCallbacks))
{
    const juce::File configDir {
        juce::File::getSpecialLocation (juce::File::userHomeDirectory)
            .getChildFile (".config/end") };

    watcher.addFolder (configDir);
    watcher.coalesceEvents (300);
    watcher.addListener (this);
}

//==============================================================================
Engine::~Engine()
{
    watcher.removeListener (this);
}

//==============================================================================
void Engine::load()
{
    keyBindings.clear();
    popupEntries.clear();
    customActions.clear();
    selectionKeys    = {};
    loadError.clear();
    prefixString.clear();
    prefixTimeoutMs  = 1000;

    lua = jam::lua::state {};
    lua.open_libraries (jam::lua::lib::base,
                        jam::lua::lib::string,
                        jam::lua::lib::table,
                        jam::lua::lib::math);

    auto displayTable { lua.create_named_table ("display") };

    displayTable.set_function ("split_horizontal",
        [this] { displayCallbacks.splitHorizontal(); });

    displayTable.set_function ("split_vertical",
        [this] { displayCallbacks.splitVertical(); });

    displayTable.set_function ("split_with_ratio",
        [this] (const std::string& direction, double ratio)
        {
            const bool isVertical { direction == "vertical" };
            displayCallbacks.splitWithRatio (juce::String (direction), isVertical, ratio);
        });

    displayTable.set_function ("new_tab",
        [this] { displayCallbacks.newTab(); });

    displayTable.set_function ("close_tab",
        [this] { displayCallbacks.closeTab(); });

    displayTable.set_function ("next_tab",
        [this] { displayCallbacks.nextTab(); });

    displayTable.set_function ("prev_tab",
        [this] { displayCallbacks.prevTab(); });

    displayTable.set_function ("focus_pane",
        [this] (int dx, int dy) { displayCallbacks.focusPane (dx, dy); });

    displayTable.set_function ("close_pane",
        [this] { displayCallbacks.closePane(); });

    const juce::File actionFile { getActionFile() };

    if (not actionFile.existsAsFile())
        writeDefaults (actionFile);

    if (actionFile.existsAsFile())
    {
        auto result { lua.safe_script_file (actionFile.getFullPathName().toStdString(),
                                            jam::lua::script_pass_on_error) };

        if (not result.valid())
        {
            jam::lua::error err { result };
            loadError = juce::String (err.what());
        }
        else
        {
            parseKeys();
            parsePopups();
            parseActions();
            parseSelectionKeys();
        }
    }
}

//==============================================================================
void Engine::registerActions (Action::Registry& registry)
{
    for (const auto& popup : popupEntries)
    {
        auto launchFn
        {
            [this, popup]() -> bool
            {
                popupCallbacks.launchPopup (popup.name, popup.command, popup.args,
                                            popup.cwd, popup.cols, popup.rows);
                return true;
            }
        };

        if (popup.modal.isNotEmpty())
        {
            registry.registerAction (
                "popup:" + popup.name,
                "Popup: " + popup.name,
                "Open " + popup.name + " popup",
                "Popups",
                true,
                launchFn);
        }

        if (popup.global.isNotEmpty())
        {
            registry.registerAction (
                "popup_global:" + popup.name,
                "Popup: " + popup.name,
                "Open " + popup.name + " popup",
                "Popups",
                false,
                launchFn);
        }
    }

    for (auto& action : customActions)
    {
        auto executeFn
        {
            [&action]() -> bool
            {
                auto result { action.execute() };

                if (not result.valid())
                {
                    jam::lua::error err { result };
                    DBG ("Lua action error (" + action.id + "): " + juce::String (err.what()));
                }

                return true;
            }
        };

        registry.registerAction (
            action.id,
            action.name,
            action.description,
            "Custom",
            action.isModal,
            executeFn);
    }
}

//==============================================================================
void Engine::buildKeyMap (Action::Registry& registry)
{
    std::vector<Action::Registry::Binding> bindings;
    bindings.reserve (keyBindings.size() + popupEntries.size() * 2 + customActions.size());

    // Built-in key bindings from the keys table.
    for (const auto& kb : keyBindings)
        bindings.push_back ({ kb.actionId, kb.shortcutString, kb.isModal });

    // Popup bindings.
    for (const auto& popup : popupEntries)
    {
        if (popup.modal.isNotEmpty())
            bindings.push_back ({ "popup:" + popup.name, popup.modal, true });

        if (popup.global.isNotEmpty())
            bindings.push_back ({ "popup_global:" + popup.name, popup.global, false });
    }

    // Custom Lua action bindings.
    for (const auto& action : customActions)
    {
        if (action.shortcut.isNotEmpty())
            bindings.push_back ({ action.id, action.shortcut, action.isModal });
    }

    registry.buildKeyMap (prefixString, prefixTimeoutMs, bindings);
}

//==============================================================================
const Engine::SelectionKeys& Engine::getSelectionKeys() const noexcept
{
    return selectionKeys;
}

const juce::String& Engine::getLoadError() const noexcept
{
    return loadError;
}

//==============================================================================
const juce::String& Engine::getPrefixString() const noexcept
{
    return prefixString;
}

//==============================================================================
void Engine::fileChanged (const juce::File& file, jam::File::Watcher::Event event)
{
    if (event == jam::File::Watcher::Event::fileUpdated)
    {
        auto* cfg { Config::getContext() };

        if (cfg != nullptr and cfg->getBool (Config::Key::autoReload))
        {
            if (file.getFileName() == "action.lua" and onActionReload != nullptr)
                onActionReload();

            if (file.getFileName() == "end.lua" and onConfigReload != nullptr)
                onConfigReload();
        }
    }
}

} // namespace Scripting
