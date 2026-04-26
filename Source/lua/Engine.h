/**
 * @file Engine.h
 * @brief Unified Lua configuration and scripting engine for END.
 *
 * lua::Engine is the single owner of the persistent Lua state that drives all
 * user-tunable settings (previously split across Config and WhelmedConfig) and
 * all keybinding/action definitions (previously owned by Scripting::Engine).
 *
 * It inherits `jam::Context<Engine>` for process-wide singleton access and
 * `jam::File::Watcher::Listener` for hot-reload on config file changes.
 *
 * @note All public methods are called on the MESSAGE THREAD.
 *
 * @see Action::Registry
 */

#pragma once

#include <JuceHeader.h>
#include "../action/Action.h"

namespace lua
{
/*____________________________________________________________________________*/

/**
 * @class lua::Engine
 * @brief Loads end.lua and its require'd modules, owns all parsed config state, and
 *        provides key bindings and action registration.
 *
 * Replaces Config, WhelmedConfig, and Scripting::Engine as the single config
 * surface.  The persistent Lua state lives for the duration of the Engine so
 * that custom action `execute` functions remain callable at keypress time.
 *
 * @par Thread context
 * All public methods are MESSAGE THREAD only.
 */
class Engine
    : public jam::Context<Engine>
    , public jam::File::Watcher::Listener
{
public:
    //==========================================================================
    /**
     * @struct Nexus
     * @brief Configuration for the top-level nexus/shell integration module.
     */
    struct Nexus
    {
        /**
         * @struct Shell
         * @brief Shell program and integration settings.
         */
        struct Shell
        {
            /** @brief Shell executable name. Platform-conditional default set in initDefaults(). */
            juce::String program { "zsh" };

            /** @brief Arguments passed to the shell at startup. */
            juce::String args { "-l" };

            /** @brief Whether shell integration is enabled. */
            bool integration { true };
        };

        /**
         * @struct Terminal
         * @brief Terminal display and input settings.
         */
        struct Terminal
        {
            /** @brief Number of lines to keep in the scrollback buffer. */
            int scrollbackLines { 10000 };

            /** @brief Number of lines to scroll per step. */
            int scrollStep { 5 };

            /** @brief Top padding in pixels. */
            int paddingTop { 10 };

            /** @brief Right padding in pixels. */
            int paddingRight { 10 };

            /** @brief Bottom padding in pixels. */
            int paddingBottom { 10 };

            /** @brief Left padding in pixels. */
            int paddingLeft { 10 };

            /** @brief Separator inserted between multiple dropped files. */
            juce::String dropMultifiles { "space" };

            /** @brief Whether dropped file paths are quoted. */
            bool dropQuoted { true };
        };

        /**
         * @struct Hyperlinks
         * @brief Hyperlink detection and external handler settings.
         */
        struct Hyperlinks
        {
            /** @brief Default editor used for file:// hyperlinks. */
            juce::String editor { "nvim" };

            /** @brief Map from URL scheme/extension to handler command. */
            std::unordered_map<juce::String, juce::String> handlers;

            /** @brief File extensions treated as clickable. */
            std::unordered_set<juce::String> extensions;
        };

        /** @brief GPU acceleration mode ("auto", "opengl", "software"). */
        juce::String gpu { "auto" };

        /** @brief Whether END runs as a background daemon. */
        bool daemon { false };

        /** @brief Whether config files are watched and hot-reloaded. */
        bool autoReload { true };

        /** @brief Shell settings. */
        Shell shell;

        /** @brief Terminal display and input settings. */
        Terminal terminal;

        /** @brief Hyperlink detection settings. */
        Hyperlinks hyperlinks;
    };

    //==========================================================================
    /**
     * @struct Display
     * @brief All visual appearance settings for the terminal window.
     */
    struct Display
    {
        /**
         * @struct Window
         * @brief Native window geometry and decoration settings.
         */
        struct Window
        {
            /** @brief Window title bar text. Default set from ProjectInfo::projectName in initDefaults(). */
            juce::String title;

            /** @brief Initial window width in pixels. */
            int width { 640 };

            /** @brief Initial window height in pixels. */
            int height { 480 };

            /** @brief Window background colour. Default "#090D12" parsed in initDefaults(). */
            juce::Colour colour;

            /** @brief Background opacity (0.0 = fully transparent, 1.0 = fully opaque). */
            float opacity { 0.75f };

            /** @brief Blur radius applied to the window background. */
            float blurRadius { 32.0f };

            /** @brief Whether the window floats above all other windows. */
            bool alwaysOnTop { false };

            /** @brief Whether native title bar buttons are shown. */
            bool buttons { false };

            /** @brief Windows only: whether DWM blur is forced. */
            bool forceDwm { true };

            /** @brief Whether window size is persisted across restarts. */
            bool saveSize { true };

            /** @brief Whether a confirmation dialog is shown on exit. */
            bool confirmationOnExit { true };
        };

        /**
         * @struct Colours
         * @brief Full colour palette for the terminal. All defaults set in initDefaults().
         */
        struct Colours
        {
            /** @brief Default text foreground colour. */
            juce::Colour foreground;

            /** @brief Default cell background colour. */
            juce::Colour background;

            /** @brief Cursor glyph colour. */
            juce::Colour cursor;

            /** @brief Selection highlight colour. */
            juce::Colour selection;

            /** @brief Selection-mode cursor colour. */
            juce::Colour selectionCursor;

            /** @brief The 16 standard ANSI palette entries. */
            std::array<juce::Colour, 16> ansi {};

            /** @brief Status bar background colour. */
            juce::Colour statusBar;

            /** @brief Status bar label background colour. */
            juce::Colour statusBarLabelBg;

            /** @brief Status bar label foreground colour. */
            juce::Colour statusBarLabelFg;

            /** @brief Status bar spinner colour. */
            juce::Colour statusBarSpinner;

            /** @brief Hint label background colour (Open File mode). */
            juce::Colour hintLabelBg;

            /** @brief Hint label foreground colour (Open File mode). */
            juce::Colour hintLabelFg;
        };

        /**
         * @struct Cursor
         * @brief Cursor glyph and blink settings.
         */
        struct Cursor
        {
            /** @brief Unicode codepoint for the cursor glyph. */
            uint32_t codepoint { 0x2588 };

            /** @brief Whether the cursor blinks. */
            bool blink { true };

            /** @brief Blink period in milliseconds (half-cycle). */
            int blinkInterval { 500 };

            /** @brief When true, always use the user cursor, ignoring DECSCUSR and OSC 12. */
            bool force { false };
        };

        /**
         * @struct Font
         * @brief Primary terminal font settings.
         */
        struct Font
        {
            /** @brief Font family name. */
            juce::String family { "Display Mono" };

            /** @brief Base font size in points. */
            float size { 12.0f };

            /** @brief Whether OpenType ligature substitution is enabled. */
            bool ligatures { true };

            /** @brief Whether synthetic bold is applied. */
            bool embolden { true };

            /** @brief Line height multiplier. */
            float lineHeight { 1.0f };

            /** @brief Cell width multiplier. */
            float cellWidth { 1.0f };

            /** @brief Windows only: whether size follows desktop DPI scale. */
            bool desktopScale { false };
        };

        /**
         * @struct Tab
         * @brief Tab bar font and colour settings.
         */
        struct Tab
        {
            /** @brief Font family for tab labels. */
            juce::String family { "Display Mono" };

            /** @brief Font size for tab labels. */
            float size { 12.0f };

            /** @brief Active tab foreground colour. */
            juce::Colour foreground;

            /** @brief Inactive tab foreground colour. */
            juce::Colour inactive;

            /** @brief Tab bar position ("left", "right", "top", "bottom"). */
            juce::String position { "left" };

            /** @brief Tab separator line colour. */
            juce::Colour line;

            /** @brief Active tab background colour. */
            juce::Colour active;

            /** @brief Active tab indicator colour. */
            juce::Colour indicator;

            /** @brief Path to SVG file for custom tab button graphics. Empty = built-in. */
            juce::String buttonSvg;
        };

        /**
         * @struct Pane
         * @brief Pane split bar colours.
         */
        struct Pane
        {
            /** @brief Pane divider bar background colour. */
            juce::Colour barColour;

            /** @brief Pane divider bar highlight colour (focused pane). */
            juce::Colour barHighlight;
        };

        /**
         * @struct Overlay
         * @brief Overlay text font and colour settings.
         */
        struct Overlay
        {
            /** @brief Font family for overlay text. */
            juce::String family { "Display Mono" };

            /** @brief Font size for overlay text. */
            float size { 14.0f };

            /** @brief Overlay text colour. */
            juce::Colour colour;
        };

        /**
         * @struct Menu
         * @brief Context menu appearance settings.
         */
        struct Menu
        {
            /** @brief Context menu background opacity. */
            float opacity { 0.65f };
        };

        /**
         * @struct ActionList
         * @brief Command palette / action list appearance settings.
         */
        struct ActionList
        {
            /** @brief Whether the list closes after running an action. */
            bool closeOnRun { true };

            /** @brief List position on screen ("top", "center", "bottom"). */
            juce::String position { "top" };

            /** @brief Font family for action names. */
            juce::String nameFamily { "Display" };

            /** @brief Font style for action names. */
            juce::String nameStyle { "Bold" };

            /** @brief Font size for action names. */
            float nameSize { 13.0f };

            /** @brief Font family for shortcut labels. */
            juce::String shortcutFamily { "Display Mono" };

            /** @brief Font style for shortcut labels. */
            juce::String shortcutStyle { "Bold" };

            /** @brief Font size for shortcut labels. */
            float shortcutSize { 12.0f };

            /** @brief Top padding in pixels. */
            int paddingTop { 10 };

            /** @brief Right padding in pixels. */
            int paddingRight { 10 };

            /** @brief Bottom padding in pixels. */
            int paddingBottom { 10 };

            /** @brief Left padding in pixels. */
            int paddingLeft { 10 };

            /** @brief Action name text colour. */
            juce::Colour nameColour;

            /** @brief Shortcut label text colour. */
            juce::Colour shortcutColour;

            /** @brief List width as a fraction of the window width. */
            float width { 0.3f };

            /** @brief List height as a fraction of the window height. */
            float height { 0.3f };

            /** @brief Highlighted row background colour. */
            juce::Colour highlightColour;
        };

        /**
         * @struct StatusBar
         * @brief Status bar position and font settings.
         */
        struct StatusBar
        {
            /** @brief Status bar position ("top", "bottom"). */
            juce::String position { "bottom" };

            /** @brief Font family for status bar text. */
            juce::String fontFamily { "Display Mono" };

            /** @brief Font size for status bar text. */
            float fontSize { 12.0f };

            /** @brief Font style for status bar text. */
            juce::String fontStyle { "Bold" };
        };

        /**
         * @struct PopupBorder
         * @brief Popup window border appearance settings.
         */
        struct PopupBorder
        {
            /** @brief Popup border colour. */
            juce::Colour borderColour;

            /** @brief Popup border width in pixels. */
            float borderWidth { 1.0f };
        };

        /** @brief Window geometry and decoration settings. */
        Window window;

        /** @brief Terminal colour palette. */
        Colours colours;

        /** @brief Cursor glyph and blink settings. */
        Cursor cursor;

        /** @brief Primary terminal font. */
        Font font;

        /** @brief Tab bar appearance. */
        Tab tab;

        /** @brief Pane divider appearance. */
        Pane pane;

        /** @brief Overlay text appearance. */
        Overlay overlay;

        /** @brief Context menu appearance. */
        Menu menu;

        /** @brief Command palette appearance. */
        ActionList actionList;

        /** @brief Status bar settings. */
        StatusBar statusBar;

        /** @brief Popup window border settings. */
        PopupBorder popup;
    };

    //==========================================================================
    /**
     * @struct Whelmed
     * @brief All appearance and behaviour settings for the Whelmed markdown viewer.
     */
    struct Whelmed
    {
        /** @brief Body text font family. */
        juce::String fontFamily { "Display" };

        /** @brief Body text font style. */
        juce::String fontStyle { "Medium" };

        /** @brief Body text font size. */
        float fontSize { 16.0f };

        /** @brief Inline code font family. */
        juce::String codeFamily { "Display Mono" };

        /** @brief Inline code font size. */
        float codeSize { 12.0f };

        /** @brief Inline code font style. */
        juce::String codeStyle { "Medium" };

        /** @brief H1 heading font size. */
        float h1Size { 28.0f };

        /** @brief H2 heading font size. */
        float h2Size { 28.0f };

        /** @brief H3 heading font size. */
        float h3Size { 24.0f };

        /** @brief H4 heading font size. */
        float h4Size { 20.0f };

        /** @brief H5 heading font size. */
        float h5Size { 18.0f };

        /** @brief H6 heading font size. */
        float h6Size { 16.0f };

        /** @brief Line height multiplier for body text. */
        float lineHeight { 1.5f };

        /** @brief Document background colour. */
        juce::Colour background;

        /** @brief Body text colour. */
        juce::Colour bodyColour;

        /** @brief Inline code text colour. */
        juce::Colour codeColour;

        /** @brief Hyperlink text colour. */
        juce::Colour linkColour;

        /** @brief H1 heading colour. */
        juce::Colour h1Colour;

        /** @brief H2 heading colour. */
        juce::Colour h2Colour;

        /** @brief H3 heading colour. */
        juce::Colour h3Colour;

        /** @brief H4 heading colour. */
        juce::Colour h4Colour;

        /** @brief H5 heading colour. */
        juce::Colour h5Colour;

        /** @brief H6 heading colour. */
        juce::Colour h6Colour;

        /** @brief Code fence block background colour. */
        juce::Colour codeFenceBackground;

        /** @brief Progress bar track colour. */
        juce::Colour progressBackground;

        /** @brief Progress bar fill colour. */
        juce::Colour progressForeground;

        /** @brief Progress percentage text colour. */
        juce::Colour progressTextColour;

        /** @brief Progress bar spinner colour. */
        juce::Colour progressSpinnerColour;

        /** @brief Top content padding in pixels. */
        int paddingTop { 10 };

        /** @brief Right content padding in pixels. */
        int paddingRight { 10 };

        /** @brief Bottom content padding in pixels. */
        int paddingBottom { 10 };

        /** @brief Left content padding in pixels. */
        int paddingLeft { 10 };

        /** @brief Syntax highlight: error token colour. */
        juce::Colour tokenError;

        /** @brief Syntax highlight: comment colour. */
        juce::Colour tokenComment;

        /** @brief Syntax highlight: keyword colour. */
        juce::Colour tokenKeyword;

        /** @brief Syntax highlight: operator colour. */
        juce::Colour tokenOperator;

        /** @brief Syntax highlight: identifier colour. */
        juce::Colour tokenIdentifier;

        /** @brief Syntax highlight: integer literal colour. */
        juce::Colour tokenInteger;

        /** @brief Syntax highlight: float literal colour. */
        juce::Colour tokenFloat;

        /** @brief Syntax highlight: string literal colour. */
        juce::Colour tokenString;

        /** @brief Syntax highlight: bracket/paren colour. */
        juce::Colour tokenBracket;

        /** @brief Syntax highlight: punctuation colour. */
        juce::Colour tokenPunctuation;

        /** @brief Syntax highlight: preprocessor directive colour. */
        juce::Colour tokenPreprocessor;

        /** @brief Table background colour. */
        juce::Colour tableBackground;

        /** @brief Table header row background colour. */
        juce::Colour tableHeaderBackground;

        /** @brief Alternating table row background colour. */
        juce::Colour tableRowAlt;

        /** @brief Table border colour. */
        juce::Colour tableBorderColour;

        /** @brief Table header text colour. */
        juce::Colour tableHeaderText;

        /** @brief Table cell text colour. */
        juce::Colour tableCellText;

        /** @brief Scrollbar thumb colour. */
        juce::Colour scrollbarThumb;

        /** @brief Scrollbar track colour. */
        juce::Colour scrollbarTrack;

        /** @brief Scrollbar background colour. */
        juce::Colour scrollbarBackground;

        /** @brief Key binding to scroll down. */
        juce::String scrollDown { "j" };

        /** @brief Key binding to scroll up. */
        juce::String scrollUp { "k" };

        /** @brief Key binding to scroll to top. */
        juce::String scrollTop { "gg" };

        /** @brief Key binding to scroll to bottom. */
        juce::String scrollBottom { "G" };

        /** @brief Scroll step in pixels. */
        int scrollStep { 50 };

        /** @brief Text selection highlight colour. */
        juce::Colour selectionColour;
    };

    //==========================================================================
    /**
     * @struct SelectionKeys
     * @brief Parsed selection-mode key bindings.
     *
     * Consumed by Terminal::Input and Whelmed::InputHandler via getSelectionKeys().
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
     * @struct Keys
     * @brief Parsed keybinding settings from keys.lua.
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

        /** @brief The prefix key shortcut string (e.g. "`"). */
        juce::String prefix { "`" };

        /** @brief Milliseconds before the waiting-for-modal-key state times out. */
        int prefixTimeout { 1000 };

        /** @brief All parsed key bindings. */
        std::vector<Binding> bindings;

        /** @brief Selection-mode key bindings. */
        SelectionKeys selection;
    };

    //==========================================================================
    /**
     * @struct Popup
     * @brief Parsed popup terminal configuration from popups.lua.
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
            int cols { 0 };

            /** @brief Popup terminal row count (0 = use default). */
            int rows { 0 };

            /** @brief Modal binding key string for this popup. */
            juce::String modal;

            /** @brief Global binding key string for this popup. */
            juce::String global;
        };

        /** @brief Default popup terminal column count. */
        int defaultCols { 70 };

        /** @brief Default popup terminal row count. */
        int defaultRows { 20 };

        /** @brief Default popup screen position ("center", "top", "bottom"). */
        juce::String defaultPosition { "center" };

        /** @brief All parsed popup entries. */
        std::vector<Entry> entries;
    };

    //==========================================================================
    /**
     * @struct Action
     * @brief Parsed custom Lua action definitions from actions.lua.
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
                            int cols,
                            int rows)>
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
        juce::Colour defaultForeground { juce::Colours::white };

        /** @brief Default cell background colour (ANSI colour index -1). */
        juce::Colour defaultBackground { juce::Colours::black };

        /** @brief Selection highlight colour (typically semi-transparent). */
        juce::Colour selectionColour { 0x8000C8D8 };

        /** @brief Cursor colour (from display.colours.cursor). */
        juce::Colour cursorColour { juce::Colours::white };

        /** @brief Selection-mode cursor colour (from display.colours.selection_cursor). */
        juce::Colour selectionCursorColour { 0xFF00D8FF };

        /** @brief Hint label background colour used in Open File mode. */
        juce::Colour hintLabelBg { 0xFFFFD700 };

        /** @brief Hint label foreground colour used in Open File mode. */
        juce::Colour hintLabelFg { 0xFF111111 };

        /** @brief Unicode codepoint for the user cursor glyph. */
        uint32_t cursorCodepoint { 0x2588 };

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
    ~Engine() override;

    /**
     * @brief Loads (or reloads) end.lua and all module files from ~/.config/end/.
     *
     * Creates a fresh Lua state, registers the API tables, runs end.lua and all
     * module files, and parses all module tables. On first launch, writes default
     * config files from BinaryData if they do not exist.
     *
     * Safe to call multiple times (hot-reload). Each call replaces all parsed state.
     *
     * @note MESSAGE THREAD.
     */
    void load();

    /**
     * @brief Triggers a full reload of all config files.
     *
     * Equivalent to calling load() from the file watcher callback path.
     *
     * @note MESSAGE THREAD.
     */
    void reload();

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
    void registerActions (::Action::Registry& registry);

    /**
     * @brief Populates the registry's key maps from all parsed bindings.
     *
     * Handles built-in key bindings, popup bindings, custom action bindings,
     * prefix key, and prefix timeout. Must be called after all actions are registered.
     *
     * @param registry  The action registry whose key maps to populate.
     * @note MESSAGE THREAD.
     */
    void buildKeyMap (::Action::Registry& registry);

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
     * @brief Returns the parsed prefix shortcut string.
     * @return Const reference to the prefix string.
     */
    const juce::String& getPrefixString() const noexcept;

    /**
     * @brief Returns the shortcut string for a given action.lua key, or empty if not found.
     * @param actionLuaKey  Dot-notation key (e.g. "keys.copy").
     * @return The shortcut string, or empty if not found.
     */
    juce::String getShortcutString (const juce::String& actionLuaKey) const;

    /**
     * @brief Constructs a Theme from the current display.colours config.
     * @return A fully populated Theme struct.
     */
    Theme buildTheme() const;

    /**
     * @brief Returns display.font.size corrected for the current DPI scale.
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

    /** @brief Called after a successful reload. Wired by MainComponent. */
    std::function<void()> onReload;

    //==========================================================================
    /** @brief Top-level nexus and shell configuration. */
    Nexus nexus;

    /** @brief All visual display settings. */
    Display display;

    /** @brief Whelmed markdown viewer settings. */
    Whelmed whelmed;

    /** @brief Key binding and prefix settings. */
    Keys keys;

    /** @brief Popup terminal definitions. */
    Popup popup;

    /** @brief Custom Lua action definitions. */
    Action action;

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

        /** @brief Corresponding action ID in the Action::Registry. */
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
    /** @brief Called by jam::File::Watcher when a watched file changes. */
    void fileChanged (const juce::File& file, jam::File::Watcher::Event event) override;

    /** @brief Initialises all struct fields to their default values. */
    void initDefaults();

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

    /** @brief Parses the nexus table from the loaded Lua state. */
    void parseNexus();

    /** @brief Parses the display table from the loaded Lua state. */
    void parseDisplay();

    /** @brief Parses the whelmed table from the loaded Lua state. */
    void parseWhelmed();

    /** @brief Parses the keys table from keys.lua. */
    void parseKeys();

    /** @brief Parses the popups table from popups.lua. */
    void parsePopups();

    /** @brief Parses the actions table from actions.lua. */
    void parseActions();

    /** @brief Parses the selection key bindings from keys.lua. */
    void parseSelectionKeys();

    //==========================================================================
    /** @brief The persistent Lua state. */
    jam::lua::State lua;

    /** @brief File system watcher for hot-reload. */
    jam::File::Watcher watcher;

    /** @brief Pane/tab operation callbacks wired by MainComponent. */
    DisplayCallbacks displayCallbacks;

    /** @brief Popup launch callback wired by MainComponent. */
    PopupCallbacks popupCallbacks;

    /** @brief Error message from the last load() call, or empty on success. */
    juce::String loadError;

    /** @brief Whether the current key file supports on-disk patching. */
    bool keyFileRemappable { true };

    //==========================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Engine)
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace lua
