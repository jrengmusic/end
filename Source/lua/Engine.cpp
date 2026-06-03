/**
 * @file Engine.cpp
 * @brief Core lifecycle for lua::Engine.
 *
 * Contains: constructor, destructor, load(), setDisplayCallbacks(),
 * setPopupCallbacks(), registerApiTable(), registerActions(), buildKeyMap(),
 * getSelectionKeys(), getLoadError(), isKeyFileRemappable(), getPrefixString().
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
    writeDefaults();
}

Engine::~Engine()
{
    // action.entries holds jam::lua::Function refs into the Lua state.
    // Members destroy in reverse declaration order — action is declared after lua,
    // so action outlives lua. Clear here to ensure Function refs are released
    // while the Lua state is still alive.
    action.entries.clear();
}

//==============================================================================
void Engine::load (jam::Model& newModel)
{
    model = &newModel;

    // Reset internal parsed state — CONFIG properties are handled by parse methods.
    keys           = Keys {};
    popup          = Popup {};
    action         = Action {};
    handlers.clear();
    extensions.clear();
    loadError.clear();
    keyFileRemappable = true;

    lua = jam::lua::State {};

    lua.openLibraries (
        jam::lua::Lib::base, jam::lua::Lib::string, jam::lua::Lib::table, jam::lua::Lib::math, jam::lua::Lib::package);

    registerApiTable();

    const juce::File configDir { getConfigPath() };
    const auto packagePath { configDir.getFullPathName().replace ("\\", "/") + "/?.lua" };
    lua["package"].setField ("path", packagePath);

    const auto endFile { configDir.getChildFile (app::id::endLua) };

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

            const auto keysFile { configDir.getChildFile (app::id::keysLua) };

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
                             const bool isVertical { direction == Map::Direction::getContext()->get (Map::Direction::vertical) };
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
                                                         cell (jam::lua::Stack::get<int> (L, 5)),
                                                         cell (jam::lua::Stack::get<int> (L, 6)));
                             return 0;
                         });
    }
}

void Engine::registerActions (::action::Registry& registry)
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

void Engine::buildKeyMap (::action::Registry& registry)
{
    std::vector<::action::Registry::Binding> bindings;
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

    const juce::String prefixStr { model->getValue<juce::String> (app::id::KEYS_LUA, app::id::prefix) };
    const int timeout { model->getValue<int> (app::id::KEYS_LUA, app::id::prefixTimeout) };
    registry.buildKeyMap (prefixStr, timeout, bindings);
}

//==============================================================================
const Engine::SelectionKeys& Engine::getSelectionKeys() const noexcept { return keys.selection; }

const juce::String& Engine::getLoadError() const noexcept { return loadError; }

bool Engine::isKeyFileRemappable() const noexcept { return keyFileRemappable; }

//==============================================================================
juce::String Engine::getPrefixString() const noexcept
{
    jassert (model != nullptr);
    return model->getValue<juce::String> (app::id::KEYS_LUA, app::id::prefix);
}

/**______________________________END OF NAMESPACE______________________________*/
} // namespace lua
