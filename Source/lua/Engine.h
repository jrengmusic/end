/**
 * @file Engine.h
 * @brief Lua config parser — writes CONFIG tree in place, owns action/key lifecycle.
 *
 * lua::Engine is a pure parser. It receives a CONFIG juce::ValueTree from AppModel,
 * runs end.lua and all module files through a persistent Lua state, and writes
 * parsed properties directly into the CONFIG subtree. It does NOT own a file watcher,
 * does NOT expose config struct members as public state, and is NOT a Context.
 *
 * Key/action lifecycle data (bindings, popup entries, custom actions) is retained
 * internally and exposed only via the public API (registerActions, buildKeyMap,
 * getSelectionKeys, etc.).
 *
 * @note All public methods are called on the MESSAGE THREAD.
 *
 * @see AppModel
 * @see action::Registry
 */

#pragma once
#include <JuceHeader.h>
#include "../AppIdentifier.h"
#include "../Map.h"
#include "../terminal/action/Action.h"
// Forward-declare jam::Model to avoid circular includes — AppModel.h includes Engine.h,
// so we cannot include the full jam_model.h header here.
namespace jam { class Model; }

namespace lua
{
/*____________________________________________________________________________*/

/**
 * @class lua::Engine
 * @brief Receives a CONFIG ValueTree and writes parsed Lua config properties in place.
 *
 * Engine is a pure parser: it owns the persistent Lua state so that custom action
 * `execute` functions remain callable at keypress time, and it maintains internal
 * vectors for key bindings, popup entries, and custom actions. All config scalars
 * (display, nexus, whelmed, keys prefix/timeout, popup defaults) are written into
 * the CONFIG ValueTree received via load().
 *
 * @par Thread context
 * All public methods are MESSAGE THREAD only.
 */
class Engine
{
public:
    //==========================================================================
    /**
     * @struct SelectionKeys
     * @brief Parsed selection-mode key bindings.
     *
     * Consumed by terminal::Input and whelmed::InputHandler via getSelectionKeys().
     */
    struct SelectionKeys
    {
        /** @brief Move selection cursor up. */
        juce::KeyPress up;

        /** @brief Move selection cursor down. */
        juce::KeyPress down;

        /** @brief Move selection cursor left. */
        juce::KeyPress left;

        /** @brief Move selection cursor right. */
        juce::KeyPress right;

        /** @brief Enter character-wise visual selection mode. */
        juce::KeyPress visual;

        /** @brief Enter line-wise visual selection mode. */
        juce::KeyPress visualLine;

        /** @brief Enter block visual selection mode. */
        juce::KeyPress visualBlock;

        /** @brief Copy current selection. */
        juce::KeyPress copy;

        /** @brief Copy current selection to system clipboard. */
        juce::KeyPress globalCopy;

        /** @brief Move selection cursor to top of buffer. */
        juce::KeyPress top;

        /** @brief Move selection cursor to bottom of buffer. */
        juce::KeyPress bottom;

        /** @brief Move selection cursor to start of line. */
        juce::KeyPress lineStart;

        /** @brief Move selection cursor to end of line. */
        juce::KeyPress lineEnd;

        /** @brief Exit selection mode. */
        juce::KeyPress exit;

        /** @brief Open file under cursor on next page. */
        juce::KeyPress openFileNextPage;
    };

    //==========================================================================
    /**
     * @struct DisplayCallbacks
     * @brief Callbacks for display/pane operations exposed to Lua.
     *
     * Wired by MainComponent to Tabs methods. Engine invokes these
     * from Lua custom action functions.
     */
    struct DisplayCallbacks
    {
        /** @brief Split the focused pane horizontally. */
        std::function<void()> splitHorizontal;

        /** @brief Split the focused pane vertically. */
        std::function<void()> splitVertical;

        /** @brief Split with an explicit ratio and orientation. */
        std::function<void (const juce::String&, bool, double)> splitWithRatio;

        /** @brief Open a new tab. */
        std::function<void()> newTab;

        /** @brief Close the current tab. */
        std::function<void()> closeTab;

        /** @brief Switch to the next tab. */
        std::function<void()> nextTab;

        /** @brief Switch to the previous tab. */
        std::function<void()> prevTab;

        /** @brief Focus a specific pane by (col, row) index. */
        std::function<void (int, int)> focusPane;

        /** @brief Close the focused pane. */
        std::function<void()> closePane;

        /** @brief Rename the active tab. Empty string clears user override. */
        std::function<void (const juce::String&)> renameTab;
    };

    //==========================================================================
    /**
     * @struct PopupCallbacks
     * @brief Callbacks for launching popup terminals from Lua.
     *
     * Wired by MainComponent. Engine invokes this when a popup action executes.
     */
    struct PopupCallbacks
    {
        /** @brief Launch a named popup terminal with given parameters. */
        std::function<void (const juce::String& name,
                            const juce::String& command,
                            const juce::String& args,
                            const juce::String& cwd,
                            cell cols,
                            cell rows)>
            launchPopup;
    };

    //==========================================================================
    /**
     * @struct Theme
     * @brief Resolved colour set built from the current config values.
     *
     * Constructed by buildTheme() and passed to Screen::setTheme().
     * Decouples the renderer from Engine so the renderer never calls
     * Engine::getContext() directly.
     *
     * @see Engine::buildTheme
     */
    struct Theme
    {
        /** @brief Default text foreground colour (ANSI colour index -1). */
        juce::Colour defaultForeground { 0xffa1d6e5 };// skyFall

        /** @brief Default cell background colour (ANSI colour index -1). */
        juce::Colour defaultBackground { 0x00000000 };// transparent

        /** @brief Selection highlight colour (typically semi-transparent). */
        juce::Colour selectionColour { 0x2000ddee };// fishBoy semi-transparent

        /** @brief Cursor colour (from display.colours.cursor). */
        juce::Colour cursorColour { 0xff4e8c93 };// paradiso

        /** @brief Selection-mode cursor colour (from display.colours.selection_cursor). */
        juce::Colour selectionCursorColour { 0xff00ddee };// fishBoy

        /** @brief Hint label background colour used in Open File mode. */
        juce::Colour hintLabelBg { 0xff00ffff };// cyan

        /** @brief Hint label foreground colour used in Open File mode. */
        juce::Colour hintLabelFg { 0xff111111 };// near-black

        /** @brief Unicode codepoint for the user cursor glyph. */
        char32_t cursorCodepoint { 0x2588u };

        /** @brief When true, always use the user cursor glyph regardless of DECSCUSR. */
        bool cursorForce { false };

        /**
         * @brief The 16 standard ANSI palette entries.
         *
         * Indices 0-7 are normal colours; indices 8-15 are bright variants.
         */
        std::array<juce::Colour, 16> ansi {};
    };

    //==========================================================================
    /** @brief Minimum zoom multiplier (1x = no zoom). */
    static constexpr float zoomMin { 1.0f };

    /** @brief Maximum zoom multiplier (4x = quadruple size). */
    static constexpr float zoomMax { 4.0f };

    /** @brief Zoom increment/decrement step size. */
    static constexpr float zoomStep { 0.25f };

    //==========================================================================
    /** @brief Constructs the engine and initialises all config to default values. */
    Engine();

    /** @brief Destructor. */
    ~Engine();

    /**
     * @brief Loads end.lua and all module files, writing parsed values into the model.
     *
     * Creates a fresh Lua state, registers the API tables, runs end.lua and all
     * module files, and writes all parsed scalar config properties via model.setValue().
     * On first launch, writes default config files from BinaryData if they do not exist.
     *
     * Each call replaces all parsed state. model is stored as a pointer for use
     * by buildTheme(), dpiCorrectedFontSize(), buildKeyMap(), and getPrefixString().
     *
     * @param model  The AppModel instance. Engine writes config via model.setValue().
     * @note MESSAGE THREAD.
     */
    void load (jam::Model& model);

    /**
     * @brief Sets the display/pane operation callbacks.
     *
     * Must be called before load() if Lua scripts use display functions.
     *
     * @param callbacks  Display operation callbacks wired to Tabs/MainComponent.
     */
    void setDisplayCallbacks (DisplayCallbacks callbacks);

    /**
     * @brief Sets the popup launch callback.
     *
     * Must be called before load() if popup actions are defined in popups.lua.
     *
     * @param callbacks  Popup launch callback wired to MainComponent.
     */
    void setPopupCallbacks (PopupCallbacks callbacks);

    /**
     * @brief Registers the `display` and `end` API tables in the Lua state.
     *
     * Called internally by load() after the Lua state is created.
     *
     * @note MESSAGE THREAD.
     */
    void registerApiTable();

    /**
     * @brief Registers popup actions and custom Lua actions in the registry.
     *
     * Does NOT register built-in actions — those are registered by MainComponent.
     * Call after MainComponent has registered all built-in actions.
     *
     * @param registry  The action registry to populate.
     * @note MESSAGE THREAD.
     */
    void registerActions (::action::Registry& registry);

    /**
     * @brief Populates the registry's key maps from all parsed bindings.
     *
     * Handles built-in key bindings, popup bindings, custom action bindings,
     * prefix key, and prefix timeout. Prefix string and timeout are read from the
     * CONFIG/KEYS node. Must be called after all actions are registered.
     *
     * @param registry  The action registry whose key maps to populate.
     * @note MESSAGE THREAD.
     */
    void buildKeyMap (::action::Registry& registry);

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
     * @brief Returns whether the currently loaded key file supports remapping.
     * @return True if key bindings can be patched on disk.
     */
    bool isKeyFileRemappable() const noexcept;

    /**
     * @brief Patches a key binding value in keys.lua on disk.
     *
     * Used by the action list shortcut remap feature. Finds the table.leaf in
     * keys.lua and replaces the value in-place.
     *
     * @param key    Dot-notation key (e.g. "keys.copy").
     * @param value  New shortcut string value.
     * @note MESSAGE THREAD.
     */
    void patchKey (const juce::String& key, const juce::String& value);

    /**
     * @brief Returns the keys.lua key for a given action ID, or empty if none.
     *
     * Built-in actions map to "keys.<lua_key>" (e.g. "copy" -> "keys.copy").
     * Popup and custom Lua actions return empty (no patchable key).
     *
     * @param actionId  The action ID (e.g. "copy", "popup:tit", "lua:my_action").
     * @return The keys.lua key string, or empty if not remappable.
     */
    juce::String getActionLuaKey (const juce::String& actionId) const;

    /**
     * @brief Returns the parsed prefix shortcut string from the CONFIG/KEYS node.
     * @return Prefix string value.
     */
    juce::String getPrefixString() const noexcept;

    /**
     * @brief Returns the shortcut string for a given action.lua key, or empty if not found.
     * @param actionLuaKey  Dot-notation key (e.g. "keys.copy").
     * @return The shortcut string, or empty if not found.
     */
    juce::String getShortcutString (const juce::String& actionLuaKey) const;

    /**
     * @brief Constructs a Theme from the current CONFIG/DISPLAY colour properties.
     * @return A fully populated Theme struct.
     */
    Theme buildTheme() const;

    /**
     * @brief Returns the font size from CONFIG/DISPLAY corrected for the current DPI scale.
     * @return DPI-adjusted font size in points.
     */
    float dpiCorrectedFontSize() const noexcept;

    /**
     * @brief Returns the handler command for a given file extension, or empty if none.
     * @param extension  The file extension to look up (e.g. ".md").
     * @return Handler command string, or empty if the extension has no handler.
     */
    juce::String getHandler (const juce::String& extension) const noexcept;

    /**
     * @brief Returns whether a file extension is treated as a clickable hyperlink.
     * @param extension  The file extension to check.
     * @return True if the extension is in the clickable set.
     */
    bool isClickableExtension (const juce::String& extension) const noexcept;

    /**
     * @brief Parses a colour string in hex, rgb(), rgba(), or named-colour format.
     *
     * Accepts: "#RRGGBB", "#RRGGBBAA", "rgb(r,g,b)", "rgba(r,g,b,a)", and
     * any string parseable by juce::Colour::fromString.
     *
     * @param input  The colour string to parse.
     * @return The parsed colour, or juce::Colours::magenta on failure.
     */
    static juce::Colour parseColour (const juce::String& input);

    /** @brief Returns the config directory (~/.config/end/). SSOT for all config path resolution.
        @note Thread-safe — returns a fixed path.
    */
    static juce::File getConfigPath();

private:
    //==========================================================================
    /**
     * @struct KeyMapping
     * @brief A single entry in the built-in key mapping table.
     *
     * Maps a Lua key name to an action ID and records whether the action is modal.
     */
    struct KeyMapping
    {
        /** @brief Key name as it appears in the keys.lua `keys` table. */
        const char* luaKey;

        /** @brief Corresponding action ID in the action::Registry. */
        const char* actionId;

        /** @brief Whether this action requires the prefix key. */
        bool isModal;
    };

    /** @brief Number of entries in the built-in key mapping table. */
    static constexpr int keyMappingCount { 23 };

    // clang-format off
    /** @brief Built-in key mapping table: maps Lua key names to action IDs. */
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
        { "rename_tab",       "rename_tab",       true  },
    }};
    // clang-format on

    //==========================================================================
    /**
     * @struct Keys
     * @brief Parsed key binding data retained between parse and buildKeyMap.
     */
    struct Keys
    {
        /**
         * @struct Binding
         * @brief A single parsed key binding entry.
         */
        struct Binding
        {
            /** @brief The action ID this binding triggers. */
            juce::String actionId;

            /** @brief The shortcut string as written in keys.lua. */
            juce::String shortcutString;

            /** @brief Whether this action requires the prefix key. */
            bool isModal { false };
        };

        /** @brief All parsed key bindings. */
        std::vector<Binding> bindings;

        /** @brief Selection-mode key bindings. */
        SelectionKeys selection;
    };

    //==========================================================================
    /**
     * @struct Popup
     * @brief Parsed popup terminal entries retained between parse and registerActions.
     */
    struct Popup
    {
        /**
         * @struct Entry
         * @brief A single popup terminal definition.
         */
        struct Entry
        {
            /** @brief Display name for the popup (used as action name). */
            juce::String name;

            /** @brief Shell command to run inside the popup. */
            juce::String command;

            /** @brief Arguments for the popup command. */
            juce::String args;

            /** @brief Working directory for the popup. */
            juce::String cwd;

            /** @brief Popup terminal column count (0 = use default). */
            cell cols { 0 };

            /** @brief Popup terminal row count (0 = use default). */
            cell rows { 0 };

            /** @brief Modal binding key string for this popup. */
            juce::String modal;

            /** @brief Global binding key string for this popup. */
            juce::String global;
        };

        /** @brief All parsed popup entries. */
        std::vector<Entry> entries;
    };

    //==========================================================================
    /**
     * @struct Action
     * @brief Parsed custom Lua action definitions retained between parse and registerActions.
     */
    struct Action
    {
        /**
         * @struct Entry
         * @brief A single custom Lua action definition.
         */
        struct Entry
        {
            /** @brief Unique machine-readable identifier (e.g. "lua:my_action"). */
            juce::String id;

            /** @brief Human-readable display name. */
            juce::String name;

            /** @brief One-line description shown in the command palette. */
            juce::String description;

            /** @brief Shortcut string for this action. */
            juce::String shortcut;

            /** @brief Whether this action requires the prefix key. */
            bool isModal { false };

            /** @brief The Lua function invoked when the action fires. */
            jam::lua::Function execute;
        };

        /** @brief All parsed custom action entries. */
        std::vector<Entry> entries;
    };

    //==========================================================================
    /** @brief Writes default config files to ~/.config/end/ if they do not exist. */
    void writeDefaults();

    /** @brief Writes default_end.lua to configDir if end.lua is absent. */
    void writeEndDefaults (const juce::File& configDir);

    /** @brief Writes default_nexus.lua to configDir if nexus.lua is absent. */
    void writeNexusDefaults (const juce::File& configDir);

    /** @brief Writes default_display.lua to configDir if display.lua is absent. */
    void writeDisplayDefaults (const juce::File& configDir);

    /** @brief Writes default_keys.lua to configDir if keys.lua is absent. */
    void writeKeysDefaults (const juce::File& configDir);

    /** @brief Writes default_popups.lua to configDir if popups.lua is absent. */
    void writePopupsDefaults (const juce::File& configDir);

    /** @brief Writes default_actions.lua to configDir if actions.lua is absent. */
    void writeActionsDefaults (const juce::File& configDir);

    /** @brief Writes default_whelmed.lua to configDir if whelmed.lua is absent. */
    void writeWhelmedDefaults (const juce::File& configDir);

    /** @brief Parses the nexus table from the loaded Lua state into the CONFIG/NEXUS node. */
    void parseNexus();

    /** @brief Parses the display table from the loaded Lua state into the CONFIG/DISPLAY node. */
    void parseDisplay();

    /** @brief Parses the whelmed table from the loaded Lua state into the CONFIG/WHELMED node. */
    void parseWhelmed();

    /** @brief Parses the keys table from keys.lua — bindings into keys member, prefix/timeout into CONFIG/KEYS. */
    void parseKeys();

    /** @brief Parses the popups table from popups.lua — entries into popup member, defaults into CONFIG/POPUPS. */
    void parsePopups();

    /** @brief Parses the actions table from actions.lua into the action member. */
    void parseActions();

    /** @brief Parses the selection key bindings from keys.lua into keys.selection. */
    void parseSelectionKeys();

    //==========================================================================
    /** @brief Pointer to the AppModel instance. Set by load(). Never null after first load(). */
    jam::Model* model { nullptr };

    /** @brief The persistent Lua state. */
    jam::lua::State lua;

    /** @brief Pane/tab operation callbacks wired by MainComponent. */
    DisplayCallbacks displayCallbacks;

    /** @brief Popup launch callback wired by MainComponent. */
    PopupCallbacks popupCallbacks;

    /** @brief Error message from the last load() call, or empty on success. */
    juce::String loadError;

    /** @brief Whether the current key file supports on-disk patching. */
    bool keyFileRemappable { true };

    /** @brief Parsed key bindings — rebuilt on each load(). */
    Keys keys;

    /** @brief Parsed popup entries — rebuilt on each load(). */
    Popup popup;

    /** @brief Parsed custom Lua actions — rebuilt on each load(). */
    Action action;

    /** @brief Hyperlink handler map — rebuilt on each load(). */
    std::unordered_map<juce::String, juce::String> handlers;

    /** @brief Clickable extension set — rebuilt on each load(). */
    std::unordered_set<juce::String> extensions;

    //==========================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Engine)
};

/**______________________________END OF NAMESPACE______________________________*/
} // namespace lua
