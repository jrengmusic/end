/**
 * @file Engine.cpp
 * @brief Core lifecycle for lua::Engine.
 *
 * Contains: constructor, destructor, load(), reload(), setDisplayCallbacks(),
 * setPopupCallbacks(), registerApiTable(), registerActions(), buildKeyMap(),
 * getSelectionKeys(), getLoadError(), isKeyFileRemappable(), getPrefixString(),
 * fileChanged().
 *
 * Defaults are implemented in EngineDefaults.cpp.
 * Domain utilities (parseColour, buildTheme, etc.) are in EngineConfig.cpp.
 * Parse methods are implemented in EngineParse.cpp.
 * Patch and query methods are implemented in EnginePatch.cpp.
 *
 * @see lua::Engine
 */

#include "Engine.h"

namespace lua
{
/*____________________________________________________________________________*/

juce::File Engine::getConfigPath()
{
    return juce::File::getSpecialLocation (juce::File::userHomeDirectory).getChildFile (".config/end");
}

//==============================================================================
Engine::Engine()
{
    initDefaults();
    writeDefaults();

    const juce::File configDir { getConfigPath() };
    watcher.addFolder (configDir);
    watcher.coalesceEvents (300);
    watcher.addListener (this);

    load();
}

Engine::~Engine()
{
    watcher.removeListener (this);

    // action.entries holds jam::lua::Function refs into the Lua state.
    // Members destroy in reverse declaration order — action (public) is declared
    // before lua (private), so action would outlive lua.  Clear here to ensure
    // Function refs are released while the Lua state is still alive.
    action.entries.clear();
}

//==============================================================================
void Engine::load()
{
    // Reset all parsed state
    initDefaults();
    nexus.hyperlinks.handlers.clear();
    nexus.hyperlinks.extensions.clear();
    keys.bindings.clear();
    popup.entries.clear();
    action.entries.clear();
    keys.selection = {};
    loadError.clear();
    keyFileRemappable = true;

    lua = jam::lua::State {};

    lua.openLibraries (
        jam::lua::Lib::base, jam::lua::Lib::string, jam::lua::Lib::table, jam::lua::Lib::math, jam::lua::Lib::package);

    registerApiTable();

    const juce::File configDir { getConfigPath() };
    const auto packagePath { configDir.getFullPathName().replace ("\\", "/") + "/?.lua" };
    lua["package"].setField ("path", packagePath);

    const auto endFile { configDir.getChildFile ("end.lua") };

    if (endFile.existsAsFile())
    {
        auto result { lua.script (endFile) };

        if (result.wasOk())
        {
            jam::lua::Value endObj { lua["END"] };

            if (endObj.getType() == jam::lua::Type::table)
            {
                juce::StringArray unexpected;

                static const juce::StringArray expectedKeys {
                    "nexus", "display", "whelmed", "keys", "popups", "actions"
                };

                endObj.forEach (
                    [&unexpected] (const jam::lua::Value& key, const jam::lua::Value&)
                    {
                        if (key.getType() == jam::lua::Type::string)
                        {
                            const auto name { key.to<juce::String>() };

                            if (not expectedKeys.contains (name))
                                unexpected.add (name);
                        }
                    });

                if (not unexpected.isEmpty())
                    loadError = "end.lua: unrecognised keys: " + unexpected.joinIntoString (", ")
                                + "\nExpected: nexus, display, whelmed, keys, popups, actions"
                                + "\nDelete ~/.config/end/ and restart to generate fresh config.";
            }

            parseNexus();
            parseDisplay();
            parseWhelmed();
            parseKeys();
            parseSelectionKeys();
            parsePopups();
            parseActions();

            const auto keysFile { configDir.getChildFile ("keys.lua") };

            if (keysFile.existsAsFile())
            {
                const auto keysContent { keysFile.loadFileAsString() };
                keyFileRemappable = keysContent.startsWith ("-- END-GENERATED v1");
            }
        }
        else
        {
            loadError = result.getErrorMessage();
        }
    }
}

void Engine::reload()
{
    load();

    if (onReload != nullptr)
        onReload();
}

void Engine::setDisplayCallbacks (DisplayCallbacks callbacks) { displayCallbacks = std::move (callbacks); }

void Engine::setPopupCallbacks (PopupCallbacks callbacks) { popupCallbacks = std::move (callbacks); }

void Engine::registerApiTable()
{
    if (displayCallbacks.splitHorizontal != nullptr)
    {
        lua.setFunction ("api",
                         "split_horizontal",
                         [this]
                         {
                             displayCallbacks.splitHorizontal();
                         });
        lua.setFunction ("api",
                         "split_vertical",
                         [this]
                         {
                             displayCallbacks.splitVertical();
                         });
        lua.setFunction ("api",
                         "split_with_ratio",
                         [this] (lua_State* L) -> int
                         {
                             auto direction { jam::lua::Stack::get<juce::String> (L, 1) };
                             auto ratio { jam::lua::Stack::get<double> (L, 2) };
                             const bool isVertical { direction == "vertical" };
                             displayCallbacks.splitWithRatio (direction, isVertical, ratio);
                             return 0;
                         });
        lua.setFunction ("api",
                         "new_tab",
                         [this]
                         {
                             displayCallbacks.newTab();
                         });
        lua.setFunction ("api",
                         "close_tab",
                         [this]
                         {
                             displayCallbacks.closeTab();
                         });
        lua.setFunction ("api",
                         "next_tab",
                         [this]
                         {
                             displayCallbacks.nextTab();
                         });
        lua.setFunction ("api",
                         "prev_tab",
                         [this]
                         {
                             displayCallbacks.prevTab();
                         });
        lua.setFunction ("api",
                         "focus_pane",
                         [this] (lua_State* L) -> int
                         {
                             displayCallbacks.focusPane (
                                 jam::lua::Stack::get<int> (L, 1), jam::lua::Stack::get<int> (L, 2));
                             return 0;
                         });
        lua.setFunction ("api",
                         "close_pane",
                         [this]
                         {
                             displayCallbacks.closePane();
                         });
        lua.setFunction ("api",
                         "rename_tab",
                         [this] (lua_State* L) -> int
                         {
                             displayCallbacks.renameTab (jam::lua::Stack::get<juce::String> (L, 1));
                             return 0;
                         });
    }

    if (popupCallbacks.launchPopup != nullptr)
    {
        lua.setFunction ("api",
                         "launch_popup",
                         [this] (lua_State* L) -> int
                         {
                             popupCallbacks.launchPopup (jam::lua::Stack::get<juce::String> (L, 1),
                                                         jam::lua::Stack::get<juce::String> (L, 2),
                                                         jam::lua::Stack::get<juce::String> (L, 3),
                                                         jam::lua::Stack::get<juce::String> (L, 4),
                                                         jam::lua::Stack::get<int> (L, 5),
                                                         jam::lua::Stack::get<int> (L, 6));
                             return 0;
                         });
    }
}

void Engine::registerActions (::Action::Registry& registry)
{
    for (const auto& popupEntry : popup.entries)
    {
        auto launchFn { [this, popupEntry]() -> bool
                        {
                            popupCallbacks.launchPopup (popupEntry.name,
                                                        popupEntry.command,
                                                        popupEntry.args,
                                                        popupEntry.cwd,
                                                        popupEntry.cols,
                                                        popupEntry.rows);
                            return true;
                        } };

        if (popupEntry.modal.isNotEmpty())
        {
            registry.registerAction ("popup:" + popupEntry.name,
                                     "Popup: " + popupEntry.name,
                                     "Open " + popupEntry.name + " popup",
                                     "Popups",
                                     true,
                                     launchFn);
        }

        if (popupEntry.global.isNotEmpty())
        {
            registry.registerAction ("popup_global:" + popupEntry.name,
                                     "Popup: " + popupEntry.name,
                                     "Open " + popupEntry.name + " popup",
                                     "Popups",
                                     false,
                                     launchFn);
        }
    }

    for (auto& actionEntry : action.entries)
    {
        auto executeFn { [&actionEntry]() -> bool
                         {
                             auto result { actionEntry.execute.call() };

                             if (result.failed())
                             {
                                 DBG ("Lua action error (" + actionEntry.id + "): " + result.getErrorMessage());
                             }

                             return true;
                         } };

        registry.registerAction (
            actionEntry.id, actionEntry.name, actionEntry.description, "Custom", actionEntry.isModal, executeFn);
    }
}

void Engine::buildKeyMap (::Action::Registry& registry)
{
    std::vector<::Action::Registry::Binding> bindings;
    bindings.reserve (keys.bindings.size() + popup.entries.size() * 2 + action.entries.size());

    // Built-in key bindings from the keys table.
    for (const auto& kb : keys.bindings)
        bindings.push_back ({ kb.actionId, kb.shortcutString, kb.isModal });

    // Popup bindings.
    for (const auto& popupEntry : popup.entries)
    {
        if (popupEntry.modal.isNotEmpty())
            bindings.push_back ({ "popup:" + popupEntry.name, popupEntry.modal, true });

        if (popupEntry.global.isNotEmpty())
            bindings.push_back ({ "popup_global:" + popupEntry.name, popupEntry.global, false });
    }

    // Custom Lua action bindings.
    for (const auto& actionEntry : action.entries)
    {
        if (actionEntry.shortcut.isNotEmpty())
            bindings.push_back ({ actionEntry.id, actionEntry.shortcut, actionEntry.isModal });
    }

    registry.buildKeyMap (keys.prefix, keys.prefixTimeout, bindings);
}

//==============================================================================
const Engine::SelectionKeys& Engine::getSelectionKeys() const noexcept { return keys.selection; }

const juce::String& Engine::getLoadError() const noexcept { return loadError; }

bool Engine::isKeyFileRemappable() const noexcept { return keyFileRemappable; }

//==============================================================================
const juce::String& Engine::getPrefixString() const noexcept { return keys.prefix; }

//==============================================================================
void Engine::fileChanged (const juce::File& /*file*/, jam::File::Watcher::Event event)
{
    if (event == jam::File::Watcher::Event::fileUpdated and nexus.autoReload)
        reload();
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace lua
