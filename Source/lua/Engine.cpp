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

#include <jam_lua/jam_lua.h>
#include <unordered_set>

#include "Engine.h"

namespace lua
{

//==============================================================================
Engine::Engine()
{
    initDefaults();
    writeDefaults();

    const juce::File configDir { juce::File::getSpecialLocation (juce::File::userHomeDirectory)
                                     .getChildFile (".config/end") };
    watcher.addFolder (configDir);
    watcher.coalesceEvents (300);
    watcher.addListener (this);

    load();
}

Engine::~Engine()
{
    watcher.removeListener (this);

    // action.entries holds jam::lua::protected_function refs into the Lua state.
    // Members destroy in reverse declaration order — action (public) is declared
    // before lua (private), so action would outlive lua.  Clear here to ensure
    // protected_function refs are released while the Lua state is still alive.
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

    lua = jam::lua::state {};

    try
    {
        lua.open_libraries (jam::lua::lib::base, jam::lua::lib::string, jam::lua::lib::table, jam::lua::lib::math, jam::lua::lib::package);

        registerApiTable();

        const juce::File configDir { juce::File::getSpecialLocation (juce::File::userHomeDirectory)
                                         .getChildFile (".config/end") };
        const auto packagePath { configDir.getFullPathName().replace ("\\", "/") + "/?.lua" };
        lua["package"]["path"] = packagePath.toStdString();

        const auto endFile { configDir.getChildFile ("end.lua") };

        if (endFile.existsAsFile())
        {
            auto result { lua.safe_script_file (endFile.getFullPathName().toStdString(), jam::lua::script_pass_on_error) };

            if (result.valid())
            {
                jam::lua::object endObj { lua["END"] };

                if (endObj.get_type() == jam::lua::type::table)
                {
                    jam::lua::table endTable { endObj.as<jam::lua::table>() };
                    juce::StringArray unexpected;

                    static const std::unordered_set<std::string> expectedKeys {
                        "nexus", "display", "whelmed", "keys", "popups", "actions"
                    };

                    endTable.for_each ([&unexpected] (const jam::lua::object& key, const jam::lua::object&)
                    {
                        if (key.get_type() == jam::lua::type::string)
                        {
                            const std::string name { key.as<std::string>() };

                            if (expectedKeys.count (name) == 0)
                                unexpected.add (juce::String (name));
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
                jam::lua::error err { result.get<jam::lua::error>() };
                loadError = juce::String (err.what());
            }
        }
    }
    catch (const jam::lua::error& e)
    {
        loadError = juce::String (e.what());
    }
}

void Engine::reload()
{
    load();

    if (onReload != nullptr)
        onReload();
}

void Engine::setDisplayCallbacks (DisplayCallbacks callbacks)
{
    displayCallbacks = std::move (callbacks);
}

void Engine::setPopupCallbacks (PopupCallbacks callbacks)
{
    popupCallbacks = std::move (callbacks);
}

void Engine::registerApiTable()
{
    auto apiTable { lua.create_named_table ("api") };

    if (displayCallbacks.splitHorizontal != nullptr)
    {
        apiTable.set_function ("split_horizontal", [this] { displayCallbacks.splitHorizontal(); });
        apiTable.set_function ("split_vertical",   [this] { displayCallbacks.splitVertical(); });
        apiTable.set_function ("split_with_ratio", [this] (const std::string& direction, double ratio)
        {
            const bool isVertical { direction == "vertical" };
            displayCallbacks.splitWithRatio (juce::String (direction), isVertical, ratio);
        });
        apiTable.set_function ("new_tab",    [this] { displayCallbacks.newTab(); });
        apiTable.set_function ("close_tab",  [this] { displayCallbacks.closeTab(); });
        apiTable.set_function ("next_tab",   [this] { displayCallbacks.nextTab(); });
        apiTable.set_function ("prev_tab",   [this] { displayCallbacks.prevTab(); });
        apiTable.set_function ("focus_pane", [this] (int dx, int dy) { displayCallbacks.focusPane (dx, dy); });
        apiTable.set_function ("close_pane", [this] { displayCallbacks.closePane(); });
    }

    if (popupCallbacks.launchPopup != nullptr)
    {
        apiTable.set_function ("launch_popup",
            [this] (const std::string& name, const std::string& command,
                    const std::string& args, const std::string& cwd,
                    int cols, int rows)
            {
                popupCallbacks.launchPopup (juce::String (name), juce::String (command),
                                            juce::String (args), juce::String (cwd), cols, rows);
            });
    }
}

void Engine::registerActions (::Action::Registry& registry)
{
    for (const auto& popupEntry : popup.entries)
    {
        auto launchFn { [this, popupEntry]() -> bool
                        {
                            popupCallbacks.launchPopup (
                                popupEntry.name, popupEntry.command,
                                popupEntry.args, popupEntry.cwd,
                                popupEntry.cols, popupEntry.rows);
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
                             auto result { actionEntry.execute() };

                             if (not result.valid())
                             {
                                 jam::lua::error err { result.get<jam::lua::error>() };
                                 DBG ("Lua action error (" + actionEntry.id + "): " + juce::String (err.what()));
                             }

                             return true;
                         } };

        registry.registerAction (actionEntry.id, actionEntry.name, actionEntry.description,
                                  "Custom", actionEntry.isModal, executeFn);
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
const Engine::SelectionKeys& Engine::getSelectionKeys() const noexcept
{
    return keys.selection;
}

const juce::String& Engine::getLoadError() const noexcept
{
    return loadError;
}

bool Engine::isKeyFileRemappable() const noexcept
{
    return keyFileRemappable;
}

//==============================================================================
const juce::String& Engine::getPrefixString() const noexcept
{
    return keys.prefix;
}

//==============================================================================
void Engine::fileChanged (const juce::File& /*file*/, jam::File::Watcher::Event event)
{
    if (event == jam::File::Watcher::Event::fileUpdated and nexus.autoReload)
        reload();
}

} // namespace lua
