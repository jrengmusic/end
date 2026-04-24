/**
 * @file Scripting.h
 * @brief Lua scripting engine for user-defined actions and keybinding management.
 *
 * Owns a persistent Lua state that loads ~/.config/end/action.lua — the single
 * source of truth for all keybindings (built-in, popup, and custom user actions).
 * Provides parsed key bindings to Action::Registry and selection key bindings to
 * terminal Input and Whelmed InputHandler.
 *
 * @see Action::Registry
 * @see Terminal::Input
 */

#pragma once

#include <array>
#include <JuceHeader.h>
#include "../action/Action.h"
#include "../config/Config.h"

namespace Scripting
{

/**
 * @class Scripting::Engine
 * @brief Loads action.lua, registers actions, and provides key bindings.
 *
 * Lifecycle: constructed after Config and Action::Registry. Destroyed with MainComponent.
 * The persistent Lua state lives for the duration of the Engine so that custom action
 * `execute` functions remain callable at keypress time.
 *
 * @par Thread context
 * All public methods are MESSAGE THREAD only.
 */
class Engine
    : public jam::Context<Engine>
    , public jam::File::Watcher::Listener
{
public:
    /**
     * @brief Callbacks for display/pane operations exposed to Lua.
     *
     * Wired by MainComponent to Tabs methods. Engine invokes these
     * from Lua custom action functions.
     */
    struct DisplayCallbacks
    {
        std::function<void()> splitHorizontal;
        std::function<void()> splitVertical;
        std::function<void (const juce::String&, bool, double)> splitWithRatio;
        std::function<void()> newTab;
        std::function<void()> closeTab;
        std::function<void()> nextTab;
        std::function<void()> prevTab;
        std::function<void (int, int)> focusPane;
        std::function<void()> closePane;
    };

    /**
     * @brief Callback for launching a popup terminal.
     *
     * Wired by MainComponent. Engine invokes this when a popup action executes.
     */
    struct PopupCallbacks
    {
        std::function<void (const juce::String& name,
                            const juce::String& command,
                            const juce::String& args,
                            const juce::String& cwd,
                            int cols,
                            int rows)>
            launchPopup;
    };

    /**
     * @brief Parsed selection-mode key bindings.
     *
     * Consumed by Terminal::Input and Whelmed::InputHandler via getSelectionKeys().
     */
    struct SelectionKeys
    {
        juce::KeyPress up;
        juce::KeyPress down;
        juce::KeyPress left;
        juce::KeyPress right;
        juce::KeyPress visual;
        juce::KeyPress visualLine;
        juce::KeyPress visualBlock;
        juce::KeyPress copy;
        juce::KeyPress globalCopy;
        juce::KeyPress top;
        juce::KeyPress bottom;
        juce::KeyPress lineStart;
        juce::KeyPress lineEnd;
        juce::KeyPress exit;
        juce::KeyPress openFileNextPage;
    };

    /**
     * @brief Constructs the scripting engine.
     * @param displayCallbacks  Pane/tab operation callbacks.
     * @param popupCallbacks    Popup launch callback.
     */
    Engine (DisplayCallbacks displayCallbacks, PopupCallbacks popupCallbacks);

    ~Engine() override;

    /**
     * @brief Loads (or reloads) action.lua from ~/.config/end/.
     *
     * Creates a fresh Lua state, exposes the `display` API table, runs action.lua,
     * and parses the keys, popups, and actions tables. On first launch, writes the
     * default action.lua from BinaryData if the file does not exist.
     *
     * Safe to call multiple times (hot-reload). Each call replaces all parsed state.
     *
     * @note MESSAGE THREAD.
     */
    void load();

    /**
     * @brief Registers popup actions and custom Lua actions in the registry.
     *
     * Does NOT register built-in actions — those are registered by MainComponent.
     * Call after MainComponent has registered all built-in actions.
     *
     * @param registry  The action registry to populate.
     * @note MESSAGE THREAD.
     */
    void registerActions (Action::Registry& registry);

    /**
     * @brief Populates the registry's key maps from all parsed bindings.
     *
     * Handles built-in key bindings (keys table), popup bindings, custom action
     * bindings, prefix key, and prefix timeout. Must be called after all actions
     * (built-in + popup + custom) are registered.
     *
     * @param registry  The action registry whose key maps to populate.
     * @note MESSAGE THREAD.
     */
    void buildKeyMap (Action::Registry& registry);

    /**
     * @brief Returns parsed selection-mode key bindings.
     * @return Const reference to the cached selection keys.
     */
    const SelectionKeys& getSelectionKeys() const noexcept;

    /**
     * @brief Returns the error message from the last load(), or empty on success.
     * @return Error string (empty = no error).
     */
    const juce::String& getLoadError() const noexcept;

    /**
     * @brief Patches a key binding value in action.lua on disk.
     *
     * Used by the action list (command palette) shortcut remap feature.
     * Finds the table.leaf in action.lua and replaces the value in-place.
     *
     * @param key    Dot-notation key (e.g. "keys.copy").
     * @param value  New shortcut string value.
     * @note MESSAGE THREAD.
     */
    void patchKey (const juce::String& key, const juce::String& value);

    /**
     * @brief Returns the action.lua key for a given action ID, or empty if none.
     *
     * Built-in actions map to "keys.<lua_key>" (e.g. "copy" -> "keys.copy").
     * Popup actions have no patchable key (return empty).
     * Custom Lua actions have no patchable key (return empty).
     *
     * @param actionId  The action ID (e.g. "copy", "popup:tit", "lua:split_thirds_h").
     * @return The action.lua key string, or empty if not remappable.
     */
    juce::String getActionLuaKey (const juce::String& actionId) const;

    /**
     * @brief Returns the parsed prefix shortcut string.
     */
    const juce::String& getPrefixString() const noexcept;

    /**
     * @brief Returns the shortcut string for a given action.lua key, or empty if not found.
     * @param actionLuaKey  Dot-notation key (e.g. "keys.copy").
     */
    juce::String getShortcutString (const juce::String& actionLuaKey) const;

    /** @brief Called when action.lua changes on disk. Wired by MainComponent. */
    std::function<void()> onActionReload;

    /** @brief Called when end.lua changes on disk. Wired by MainComponent. */
    std::function<void()> onConfigReload;

private:
    /**
     * @brief A single parsed entry from the `popups` table in action.lua.
     */
    struct PopupEntry
    {
        juce::String name;
        juce::String command;
        juce::String args;
        juce::String cwd;
        int cols { 0 };
        int rows { 0 };
        juce::String modal;
        juce::String global;
    };

    /**
     * @brief A single parsed entry from the `actions` table in action.lua.
     */
    struct CustomAction
    {
        juce::String id;
        juce::String name;
        juce::String description;
        juce::String shortcut;
        bool isModal { false };
        jam::lua::protected_function execute;
    };

    /**
     * @brief A parsed key binding from the `keys` table in action.lua.
     */
    struct KeyBinding
    {
        juce::String actionId;
        juce::String shortcutString;
        bool isModal { false };
    };

    /**
     * @brief A single entry in the built-in key mapping table.
     *
     * Maps a Lua key name to an action ID and records whether the action is modal.
     */
    struct KeyMapping
    {
        const char* luaKey;
        const char* actionId;
        bool isModal;
    };

    static constexpr int keyMappingCount { 22 };

    // clang-format off
    static constexpr std::array<KeyMapping, keyMappingCount> keyMappings
    {{
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
    }};
    // clang-format on

    static juce::File getActionFile();

    static void writeDefaults (const juce::File& file);

    void fileChanged (const juce::File& file, jam::File::Watcher::Event event) override;

    void parseKeys();
    void parsePopups();
    void parseActions();
    void parseSelectionKeys();

    jam::File::Watcher watcher;

    DisplayCallbacks displayCallbacks;
    PopupCallbacks popupCallbacks;

    jam::lua::state lua;
    juce::String loadError;

    std::vector<KeyBinding> keyBindings;
    std::vector<PopupEntry> popupEntries;
    std::vector<CustomAction> customActions;
    SelectionKeys selectionKeys;

    juce::String prefixString;
    int prefixTimeoutMs { 1000 };

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Engine)
};

} // namespace Scripting

