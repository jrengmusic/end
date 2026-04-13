/**
 * @file Config.h
 * @brief Lua-driven configuration system for END.
 *
 * Config is the single source of truth for every user-tunable setting in the
 * terminal emulator.  It inherits `jreng::Context<Config>` so that any
 * subsystem can retrieve the singleton via `Config::getContext()` without
 * passing references through the call stack.
 *
 * ### File layout
 * | File                        | Purpose                                  |
 * |-----------------------------|------------------------------------------|
 * | `~/.config/end/end.lua`     | User config (loaded at startup, reloaded on Cmd+R) |
 * | `~/.config/end/end.state`      | Persisted application state (managed by AppState) |
 *
 * ### Config format (end.lua)
 * @code{.lua}
 * END = {
 *     font = { family = "Display Mono", size = 14 },
 *     colours = { foreground = "#B3F9F5", background = "#090D12E0" },
 *     window = { opacity = 0.85, always_on_top = true },
 * }
 * @endcode
 *
 * Keys are validated against the `schema` map.  Unknown keys, type mismatches,
 * and out-of-range numbers produce non-fatal warnings collected in `loadError`.
 *
 * ### Zoom
 * Zoom is stored as a multiplier in `[zoomMin, zoomMax]` by `AppState`.
 * `Terminal::Display::applyZoom()` applies it to the renderer by scaling
 * the base font size.
 *
 * @note All public methods are called on the **MESSAGE THREAD**.
 *
 * @see Terminal::Display
 * @see Config::Theme
 * @see Config::Key
 */

#pragma once

#include <JuceHeader.h>
#include <unordered_map>
#include <unordered_set>

/**
 * @struct Config
 * @brief Lua config loader and runtime value store for END.
 *
 * Inherits `jreng::Context<Config>` to provide a process-wide singleton
 * accessible via `Config::getContext()`.  Constructed once in `ENDApplication`
 * before any other subsystem.
 *
 * ### Lifecycle
 * 1. `Config()` — calls `initKeys()`, loads `end.lua`.
 * 2. `reload()` — re-runs `initKeys()` then re-loads `end.lua`; used by
 *    Cmd+R in `Terminal::Display`.
 *
 * @par Thread context
 * **MESSAGE THREAD** — all methods must be called from the JUCE message thread.
 *
 * @see Config::Key
 * @see Config::Theme
 * @see Terminal::Display::applyConfig
 */
struct Config : jreng::Context<Config>
{
    /** @brief Minimum zoom multiplier (1× = no zoom). */
    static constexpr float zoomMin { 1.0f };

    /** @brief Maximum zoom multiplier (4× = quadruple size). */
    static constexpr float zoomMax { 4.0f };

    /** @brief Zoom increment/decrement step size. */
    static constexpr float zoomStep { 0.25f };

    //==============================================================================
    /**
     * @struct Theme
     * @brief Resolved colour set built from the current config values.
     *
     * Constructed by `buildTheme()` and passed to `Screen::setTheme()`.
     * Decouples the renderer from Config so the renderer never calls
     * `Config::getContext()` directly.
     *
     * @see Config::buildTheme
     * @see Screen::setTheme
     */
    struct Theme
    {
        /** @brief Default text foreground colour (ANSI colour index -1). */
        juce::Colour defaultForeground { juce::Colours::white };

        /** @brief Default cell background colour (ANSI colour index -1). */
        juce::Colour defaultBackground { juce::Colours::black };

        /** @brief Selection highlight colour (typically semi-transparent). */
        juce::Colour selectionColour { 0x8000C8D8 };

        /** @brief Cursor colour (from `colours.cursor` config). */
        juce::Colour cursorColour { juce::Colours::white };

        /** @brief Selection-mode cursor colour (from `colours.selection_cursor` config). */
        juce::Colour selectionCursorColour { 0xFF00D8FF };

        /** @brief Hint label background colour used in Open File mode (from `colours.hint_label_bg` config). */
        juce::Colour hintLabelBg { 0xFFFFD700 };

        /** @brief Hint label foreground colour used in Open File mode (from `colours.hint_label_fg` config). */
        juce::Colour hintLabelFg { 0xFF111111 };

        /** @brief Unicode codepoint for the user cursor glyph (from `cursor.char`). */
        uint32_t cursorCodepoint { 0x2588 };

        /** @brief When true, always use the user cursor glyph regardless of DECSCUSR. */
        bool cursorForce { false };

        /**
         * @brief The 16 standard ANSI palette entries.
         *
         * Indices 0–7 are the normal colours (black, red, green, yellow, blue,
         * magenta, cyan, white).  Indices 8–15 are the bright variants.
         */
        std::array<juce::Colour, 16> ansi {};
    };

    //==============================================================================
    /**
     * @struct PopupEntry
     * @brief Configuration for a single popup terminal entry.
     *
     * Each entry in the `popups` Lua table maps to one PopupEntry.
     * The table key (e.g. "tit", "lazygit") serves as the unique identifier.
     *
     * @see Config::getPopups
     */
    struct PopupEntry
    {
        /** @brief Shell command or executable to run inside the popup terminal. */
        juce::String command;

        /** @brief Arguments passed to the command (space-separated string). */
        juce::String args;

        /** @brief Working directory. Empty = inherit active terminal's cwd. */
        juce::String cwd;

        /** @brief Popup width in columns. Zero = use global default. */
        int cols { 0 };

        /** @brief Popup height in rows. Zero = use global default. */
        int rows { 0 };

        /** @brief Modal key binding (prefix + key). Empty = no modal binding. */
        juce::String modal;

        /** @brief Global key binding (direct shortcut). Empty = no global binding. */
        juce::String global;
    };

    //==============================================================================
    /**
     * @struct Key
     * @brief String constants for every config key understood by Config.
     *
     * Keys use dot-notation matching the Lua table hierarchy:
     * `"font.family"` corresponds to `END.font.family` in `end.lua`.
     *
     * All members are `inline static const juce::String` so they are
     * zero-cost to pass by reference and have a single definition.
     *
     * @par Usage
     * @code
     * const auto family { Config::getContext()->getString (Config::Key::fontFamily) };
     * @endcode
     *
     * @see Config::getString
     * @see Config::getInt
     * @see Config::getFloat
     * @see Config::getBool
     * @see Config::getColour
     */
    struct Key
    {
        /** @brief Primary monospace font family name (e.g. "JetBrains Mono"). */
        inline static const juce::String fontFamily { "font.family" };

        /** @brief Base font size in points before zoom is applied. */
        inline static const juce::String fontSize { "font.size" };

        /** @brief Whether OpenType ligature substitution is enabled. */
        inline static const juce::String fontLigatures { "font.ligatures" };

        /** @brief Whether synthetic bold (embolden) is applied to the main font. */
        inline static const juce::String fontEmbolden { "font.embolden" };

        /** @brief Line height multiplier applied to terminal cell height (default 1.0). */
        inline static const juce::String fontLineHeight { "font.line_height" };

        /** @brief Cell width multiplier applied to terminal cell width (default 1.0). */
        inline static const juce::String fontCellWidth { "font.cell_width" };

        /** @brief Windows only. When "true", font size follows the Windows desktop scale.
         *         When "false" (default), font size is persistent across desktop scales. No-op on macOS/Linux. */
        inline static const juce::String fontDesktopScale { "font.desktop_scale" };

        /** @brief Unicode codepoint string used as the cursor glyph. */
        inline static const juce::String cursorChar { "cursor.char" };

        /** @brief Whether the cursor blinks. */
        inline static const juce::String cursorBlink { "cursor.blink" };

        /** @brief Blink period in milliseconds (half-cycle on, half-cycle off). */
        inline static const juce::String cursorBlinkInterval { "cursor.blink_interval" };

        /** @brief Whether to force user-configured cursor, ignoring DECSCUSR and OSC 12. */
        inline static const juce::String cursorForce { "cursor.force" };

        /** @brief Default text foreground colour (hex or rgba string). */
        inline static const juce::String coloursForeground { "colours.foreground" };

        /** @brief Default cell background colour (hex or rgba string). */
        inline static const juce::String coloursBackground { "colours.background" };

        /** @brief Cursor glyph tint colour (hex or rgba string). */
        inline static const juce::String coloursCursor { "colours.cursor" };

        /** @brief Selection highlight colour (hex or rgba string). */
        inline static const juce::String coloursSelection { "colours.selection" };

        /** @brief Selection-mode cursor colour (hex or rgba string). */
        inline static const juce::String coloursSelectionCursor { "colours.selection_cursor" };

        /** @brief ANSI colour 0 — black. */
        inline static const juce::String coloursBlack { "colours.black" };

        /** @brief ANSI colour 1 — red. */
        inline static const juce::String coloursRed { "colours.red" };

        /** @brief ANSI colour 2 — green. */
        inline static const juce::String coloursGreen { "colours.green" };

        /** @brief ANSI colour 3 — yellow. */
        inline static const juce::String coloursYellow { "colours.yellow" };

        /** @brief ANSI colour 4 — blue. */
        inline static const juce::String coloursBlue { "colours.blue" };

        /** @brief ANSI colour 5 — magenta. */
        inline static const juce::String coloursMagenta { "colours.magenta" };

        /** @brief ANSI colour 6 — cyan. */
        inline static const juce::String coloursCyan { "colours.cyan" };

        /** @brief ANSI colour 7 — white. */
        inline static const juce::String coloursWhite { "colours.white" };

        /** @brief ANSI colour 8 — bright black (dark grey). */
        inline static const juce::String coloursBrightBlack { "colours.bright_black" };

        /** @brief ANSI colour 9 — bright red. */
        inline static const juce::String coloursBrightRed { "colours.bright_red" };

        /** @brief ANSI colour 10 — bright green. */
        inline static const juce::String coloursBrightGreen { "colours.bright_green" };

        /** @brief ANSI colour 11 — bright yellow. */
        inline static const juce::String coloursBrightYellow { "colours.bright_yellow" };

        /** @brief ANSI colour 12 — bright blue. */
        inline static const juce::String coloursBrightBlue { "colours.bright_blue" };

        /** @brief ANSI colour 13 — bright magenta. */
        inline static const juce::String coloursBrightMagenta { "colours.bright_magenta" };

        /** @brief ANSI colour 14 — bright cyan. */
        inline static const juce::String coloursBrightCyan { "colours.bright_cyan" };

        /** @brief ANSI colour 15 — bright white. */
        inline static const juce::String coloursBrightWhite { "colours.bright_white" };

        /** @brief Native window title bar string. */
        inline static const juce::String windowTitle { "window.title" };

        /** @brief Initial (and persisted) window width in pixels. */
        inline static const juce::String windowWidth { "window.width" };

        /** @brief Initial (and persisted) window height in pixels. */
        inline static const juce::String windowHeight { "window.height" };

        /** @brief Native window background colour used for blur tint (hex string, no alpha). */
        inline static const juce::String windowColour { "window.colour" };

        /** @brief Window translucency [0.0, 1.0]; 1.0 = fully opaque. */
        inline static const juce::String windowOpacity { "window.opacity" };

        /** @brief Background blur radius in points; 0 = no blur. */
        inline static const juce::String windowBlurRadius { "window.blur_radius" };

        /** @brief Whether the window floats above all other windows. */
        inline static const juce::String windowAlwaysOnTop { "window.always_on_top" };

        /** @brief Whether native traffic-light window buttons are shown. */
        inline static const juce::String windowButtons { "window.buttons" };

        /** @brief Force DWM effects on Windows 11 VMs for rounded corners. */
        inline static const juce::String windowForceDwm { "window.force_dwm" };

        /** @brief GPU acceleration mode: "auto", "true", or "false". */
        inline static const juce::String gpuAcceleration { "gpu" };

        /** @brief Whether the background daemon is enabled. When true, sessions survive window close. */
        inline static const juce::String daemon { "daemon" };

        /** @brief Font family for the tab bar labels. */
        inline static const juce::String tabFamily { "tab.family" };

        /** @brief Font size in points for the tab bar labels; font is 75% of tab bar height. */
        inline static const juce::String tabSize { "tab.size" };

        /** @brief Active tab text colour (hex string). */
        inline static const juce::String tabForeground { "tab.foreground" };

        /** @brief Inactive tab text colour (hex string). */
        inline static const juce::String tabInactive { "tab.inactive" };

        /** @brief Tab bar position: "top", "bottom", "left", or "right". */
        inline static const juce::String tabPosition { "tab.position" };

        /** @brief Active tab indicator line colour (hex string). */
        inline static const juce::String tabLine { "tab.line" };

        /** @brief Active tab fill colour (hex string). */
        inline static const juce::String tabActive { "tab.active" };

        /** @brief Active tab indicator colour (hex string). */
        inline static const juce::String tabIndicator { "tab.indicator" };

        /** @brief Popup menu background opacity (0.0–1.0). Applied as NSWindow tint alpha. */
        inline static const juce::String menuOpacity { "menu.opacity" };

        /** @brief Font family for the MessageOverlay display. */
        inline static const juce::String overlayFamily { "overlay.family" };

        /** @brief Font size in points for the MessageOverlay display. */
        inline static const juce::String overlaySize { "overlay.size" };

        /** @brief Text colour for the MessageOverlay display (hex string). */
        inline static const juce::String overlayColour { "overlay.colour" };

        /** @brief Shell program name or absolute path (e.g. "zsh", "/opt/homebrew/bin/fish"). */
        inline static const juce::String shellProgram { "shell.program" };

        /** @brief Shell arguments passed after the program name (array of strings). */
        inline static const juce::String shellArgs { "shell.args" };

        /** @brief Whether automatic shell integration (OSC 133 markers) is enabled. */
        inline static const juce::String shellIntegration { "shell.integration" };

        /** @brief Maximum number of scrollback lines retained in the grid. */
        inline static const juce::String terminalScrollbackLines { "terminal.scrollback_lines" };

        /** @brief Number of lines scrolled per mouse-wheel tick and scrollback keyboard navigation. */
        inline static const juce::String terminalScrollStep { "terminal.scroll_step" };

        /**
         * @brief Grid padding — top edge inset in logical pixels (0–200).
         *
         * Parsed from the four-value `terminal.padding` array in end.lua.
         * Order: top, right, bottom, left (CSS convention).
         */
        inline static const juce::String terminalPaddingTop { "terminal.padding_top" };

        /** @brief Grid padding — right edge inset in logical pixels (0–200). */
        inline static const juce::String terminalPaddingRight { "terminal.padding_right" };

        /** @brief Grid padding — bottom edge inset in logical pixels (0–200). */
        inline static const juce::String terminalPaddingBottom { "terminal.padding_bottom" };

        /** @brief Grid padding — left edge inset in logical pixels (0–200). */
        inline static const juce::String terminalPaddingLeft { "terminal.padding_left" };

        /** @brief Separator for multiple dropped file paths: "space" or "newline". */
        inline static const juce::String terminalDropMultifiles { "terminal.drop_multifiles" };

        /** @brief Whether dropped file paths are shell-quoted (true) or raw (false). */
        inline static const juce::String terminalDropQuoted { "terminal.drop_quoted" };

        inline static const juce::String keysCopy { "keys.copy" };
        inline static const juce::String keysPaste { "keys.paste" };
        inline static const juce::String keysQuit { "keys.quit" };
        inline static const juce::String keysCloseTab { "keys.close_tab" };
        inline static const juce::String keysReload { "keys.reload" };
        inline static const juce::String keysZoomIn { "keys.zoom_in" };
        inline static const juce::String keysZoomOut { "keys.zoom_out" };
        inline static const juce::String keysZoomReset { "keys.zoom_reset" };
        inline static const juce::String keysNewWindow { "keys.new_window" };
        inline static const juce::String keysNewTab { "keys.new_tab" };
        inline static const juce::String keysPrevTab { "keys.prev_tab" };
        inline static const juce::String keysNextTab { "keys.next_tab" };

        /** @brief Key binding for horizontal split (left/right). */
        inline static const juce::String keysSplitHorizontal { "keys.split_horizontal" };

        /** @brief Key binding for vertical split (top/bottom). */
        inline static const juce::String keysSplitVertical { "keys.split_vertical" };

        inline static const juce::String keysPrefix { "keys.prefix" };
        inline static const juce::String keysPrefixTimeout { "keys.prefix_timeout" };
        inline static const juce::String keysPaneLeft { "keys.pane_left" };
        inline static const juce::String keysPaneDown { "keys.pane_down" };
        inline static const juce::String keysPaneUp { "keys.pane_up" };
        inline static const juce::String keysPaneRight { "keys.pane_right" };
        inline static const juce::String keysNewline { "keys.newline" };
        inline static const juce::String keysActionList { "keys.action_list" };
        inline static const juce::String keysActionListPosition { "keys.action_list_position" };

        /** @brief Close the action list after running an action. */
        inline static const juce::String keysActionListCloseOnRun { "action_list.close_on_run" };

        /** @brief Font family for action list name labels. */
        inline static const juce::String actionListNameFamily { "action_list.name_font_family" };

        /** @brief Font size in points for action list name labels (6–72). */
        inline static const juce::String actionListNameSize { "action_list.name_font_size" };

        /** @brief Font family for action list shortcut labels. */
        inline static const juce::String actionListShortcutFamily { "action_list.shortcut_font_family" };

        /** @brief Font size in points for action list shortcut labels (6–72). */
        inline static const juce::String actionListShortcutSize { "action_list.shortcut_font_size" };

        /** @brief Action list padding — top edge inset in logical pixels (0–200). */
        inline static const juce::String actionListPaddingTop { "action_list.padding_top" };

        /** @brief Action list padding — right edge inset in logical pixels (0–200). */
        inline static const juce::String actionListPaddingRight { "action_list.padding_right" };

        /** @brief Action list padding — bottom edge inset in logical pixels (0–200). */
        inline static const juce::String actionListPaddingBottom { "action_list.padding_bottom" };

        /** @brief Action list padding — left edge inset in logical pixels (0–200). */
        inline static const juce::String actionListPaddingLeft { "action_list.padding_left" };

        /** @brief Text colour for action list name labels (hex string). */
        inline static const juce::String actionListNameColour { "action_list.name_colour" };

        /** @brief Text colour for action list shortcut labels (hex string). */
        inline static const juce::String actionListShortcutColour { "action_list.shortcut_colour" };

        inline static const juce::String actionListWidth          { "action_list.width" };
        inline static const juce::String actionListHeight         { "action_list.height" };
        inline static const juce::String actionListHighlightColour { "action_list.highlight_colour" };

        inline static const juce::String keysEnterSelection { "keys.enter_selection" };
        inline static const juce::String keysEnterOpenFile { "keys.enter_open_file" };
        inline static const juce::String keysOpenFileNextPage { "keys.open_file_next_page" };

        /** @brief Move cursor up in selection mode. */
        inline static const juce::String keysSelectionUp { "keys.selection_up" };

        /** @brief Move cursor down in selection mode. */
        inline static const juce::String keysSelectionDown { "keys.selection_down" };

        /** @brief Move cursor left in selection mode. */
        inline static const juce::String keysSelectionLeft { "keys.selection_left" };

        /** @brief Move cursor right in selection mode. */
        inline static const juce::String keysSelectionRight { "keys.selection_right" };

        /** @brief Toggle character-wise visual selection in selection mode. */
        inline static const juce::String keysSelectionVisual { "keys.selection_visual" };

        /** @brief Toggle line-wise visual selection in selection mode. */
        inline static const juce::String keysSelectionVisualLine { "keys.selection_visual_line" };

        /** @brief Toggle block visual selection in selection mode. */
        inline static const juce::String keysSelectionVisualBlock { "keys.selection_visual_block" };

        /** @brief Yank (copy) the current selection in selection mode. */
        inline static const juce::String keysSelectionCopy { "keys.selection_copy" };

        /** @brief Jump to top (gg double-press) in selection mode. */
        inline static const juce::String keysSelectionTop { "keys.selection_top" };

        /** @brief Jump to bottom in selection mode. */
        inline static const juce::String keysSelectionBottom { "keys.selection_bottom" };

        /** @brief Jump to line start in selection mode. */
        inline static const juce::String keysSelectionLineStart { "keys.selection_line_start" };

        /** @brief Jump to line end in selection mode. */
        inline static const juce::String keysSelectionLineEnd { "keys.selection_line_end" };

        /** @brief Exit selection mode. */
        inline static const juce::String keysSelectionExit { "keys.selection_exit" };

        inline static const juce::String popupCols { "popup.cols" };
        inline static const juce::String popupRows { "popup.rows" };

        /** @brief Popup default position: "center" for now. */
        inline static const juce::String popupPosition { "popup.position" };

        /** @brief Popup border colour (hex or rgba string). */
        inline static const juce::String popupBorderColour { "popup.border_colour" };

        /** @brief Popup border stroke width in pixels (0 = no border). */
        inline static const juce::String popupBorderWidth { "popup.border_width" };

        /** @brief Pane divider bar colour (hex string). */
        inline static const juce::String paneBarColour { "pane.bar_colour" };

        /** @brief Pane divider bar colour when dragging or hovering (hex string). */
        inline static const juce::String paneBarHighlight { "pane.bar_highlight" };

        /** @brief Status bar full background colour (hex string). */
        inline static const juce::String coloursStatusBar { "colours.status_bar" };

        /** @brief Status bar mode label background colour (hex string). */
        inline static const juce::String coloursStatusBarLabelBg { "colours.status_bar_label_bg" };

        /** @brief Status bar mode label text colour (hex string). */
        inline static const juce::String coloursStatusBarLabelFg { "colours.status_bar_label_fg" };

        /** @brief Hint label background colour for Open File mode (hex string). */
        inline static const juce::String coloursHintLabelBg { "colours.hint_label_bg" };

        /** @brief Hint label foreground colour for Open File mode (hex string). */
        inline static const juce::String coloursHintLabelFg { "colours.hint_label_fg" };

        /** @brief Status bar position: "top" or "bottom". */
        inline static const juce::String keysStatusBarPosition { "keys.status_bar_position" };

        /** @brief Font family for the status bar overlay. */
        inline static const juce::String statusBarFontFamily { "colours.status_bar_font_family" };

        /** @brief Font size in points for the status bar overlay. */
        inline static const juce::String statusBarFontSize { "colours.status_bar_font_size" };

        /** @brief Font style for the status bar overlay (e.g. "Bold"). */
        inline static const juce::String statusBarFontStyle { "colours.status_bar_font_style" };

        /** @brief Spinner glyph colour for the status bar and loader overlay (hex string). */
        inline static const juce::String statusBarSpinnerColour { "colours.status_bar_spinner" };

        /** @brief Editor command used to open files from hyperlinks (e.g. "nvim", "vim"). */
        inline static const juce::String hyperlinksEditor { "hyperlinks.editor" };
    };

    //==============================================================================
    /**
     * @brief Returns the handler command for the given file extension.
     *
     * Looks up @p extension (with leading dot, lowercase) in `hyperlinkHandlers`.
     * Returns the command string if found, or an empty string if no handler is
     * configured.  Falls back to the `editor` command at the call site when empty.
     *
     * @param extension  Lowercase extension with leading dot (e.g. `".pdf"`).
     * @return The configured command string, or empty.
     * @note MESSAGE THREAD.
     */
    juce::String getHandler (const juce::String& extension) const noexcept;

    /**
     * @brief Returns `true` if @p extension is clickable (built-in or user-configured).
     *
     * Checks both `hyperlinkExtensions` (user-added extensions without handlers)
     * and the keys of `hyperlinkHandlers` (extensions with explicit handlers).
     * Does NOT check built-in extensions — the caller merges with `builtInExtensions()`.
     *
     * @param extension  Lowercase extension with leading dot (e.g. `".vue"`).
     * @return `true` if the extension appears in the user config.
     * @note MESSAGE THREAD.
     */
    bool isClickableExtension (const juce::String& extension) const noexcept;

    //==============================================================================
    /**
     * @brief Constructs Config: loads defaults, schema, then end.lua.
     *
     * If `~/.config/end/end.lua` does not exist it is created with an empty
     * `END = {}` table.  Errors from `end.lua` are stored in `loadError` and
     * displayed by `Terminal::Display` at startup.
     *
     * @note MESSAGE THREAD — called once from ENDApplication member init.
     */
    Config();

    //==============================================================================
    /**
     * @brief Returns a config value as a string.
     * @param key  A `Config::Key` constant (e.g. `Config::Key::fontFamily`).
     * @return The stored string value.
     * @note Throws `std::out_of_range` if @p key is not in the values map.
     */
    juce::String getString (const juce::String& key) const;

    /**
     * @brief Returns a config value as an integer.
     * @param key  A `Config::Key` constant.
     * @return The stored value cast to `int`.
     * @note Throws `std::out_of_range` if @p key is not in the values map.
     */
    int getInt (const juce::String& key) const;

    /**
     * @brief Returns a config value as a float.
     * @param key  A `Config::Key` constant.
     * @return The stored value cast to `float`.
     * @note Throws `std::out_of_range` if @p key is not in the values map.
     */
    float getFloat (const juce::String& key) const;

    /**
     * @brief Returns a config value as a boolean.
     * @param key  A `Config::Key` constant.
     * @return The stored boolean value.
     * @note Throws `std::out_of_range` if @p key is not in the values map.
     */
    bool getBool (const juce::String& key) const;

    /**
     * @brief Returns a config value parsed as a JUCE Colour.
     * @param key  A `Config::Key` constant whose value is a colour string.
     * @return The parsed `juce::Colour`.
     * @see parseColour
     */
    juce::Colour getColour (const juce::String& key) const;

    /**
     * @brief Builds a fully resolved Theme from the current config values.
     *
     * Reads all colour keys and assembles them into a `Theme` struct ready to
     * pass to `Screen::setTheme()`.  Called after construction and after every
     * `reload()`.
     *
     * @return A `Theme` with all 16 ANSI colours and the default fg/bg/selection.
     * @see Theme
     * @see Screen::setTheme
     */
    Theme buildTheme() const;

    /**
     * @brief Returns the configured font size with the Windows desktop-scale correction
     *         applied per `Key::fontDesktopScale`. On macOS/Linux returns the raw size.
     */
    float dpiCorrectedFontSize() const noexcept;

    /**
     * @brief Returns the parsed popup entries from the `popups` Lua table.
     *
     * Each map entry is keyed by the popup name (e.g. "tit", "lazygit").
     * Entries have per-popup width/height that fall back to global
     * `popup.width` / `popup.height` defaults when zero.
     *
     * @return Const reference to the popup entries map.
     * @see PopupEntry
     */
    const std::unordered_map<juce::String, PopupEntry>& getPopups() const noexcept;

    //==============================================================================
    /**
     * @brief Loads config from @p file, storing errors in `loadError`.
     * @param file  The Lua config file to load (typically `end.lua`).
     * @return @c true if the file was parsed without a fatal Lua error.
     * @note Non-fatal warnings (unknown keys, type mismatches) are appended to
     *       `loadError` but do not cause a @c false return.
     */
    bool load (const juce::File& file);

    /**
     * @brief Loads config from @p file, writing errors to @p errorOut.
     *
     * Two-argument overload used by `reload()` to collect errors into a
     * temporary string without overwriting `loadError`.
     *
     * @param file      The Lua config file to load.
     * @param errorOut  Receives the error/warning string on return.
     * @return @c true if the file was parsed without a fatal Lua error.
     */
    bool load (const juce::File& file, juce::String& errorOut);

    /**
     * @brief Resets to defaults and reloads `end.lua`.
     *
     * Called by `Terminal::Display` on Cmd+R.  Window size and zoom are
     * managed by `AppState` and preserved across reloads.
     *
     * @return The error/warning string from the reload, or empty if clean.
     * @see Terminal::Display::keyPressed
     */
    juce::String reload();

    /**
     * @brief Patches a single key-value pair in `end.lua` without reloading.
     *
     * Performs targeted line replacement inside the user config file.
     * Does NOT call `onReload` — the caller is responsible for any
     * post-patch action (e.g. `Action::Registry::buildKeyMap()`).
     *
     * @param key    Dot-notation config key (e.g. `"keys.copy"`).
     * @param value  The new value string (e.g. `"cmd+shift+c"`).
     * @note MESSAGE THREAD.
     */
    void patchKey (const juce::String& key, const juce::String& value);

    /**
     * @brief Callback fired after `reload()` completes successfully.
     *
     * Wired by MainComponent to rebuild actions, apply config to tabs,
     * update LookAndFeel, etc.  Fired at the end of `reload()` after
     * defaults are reset and `end.lua` is re-parsed.
     *
     * @note MESSAGE THREAD.
     */
    std::function<void()> onReload;

    //==============================================================================
    /**
     * @brief Returns the path to `~/.config/end/end.lua`, creating it if absent.
     * @return The config file; guaranteed to exist after this call.
     */
    juce::File getConfigFile() const;

    /**
     * @brief Returns the last load error/warning string.
     *
     * Non-empty after a load that produced warnings or a fatal Lua error.
     * Displayed by `Terminal::Display` as a `MessageOverlay` at startup.
     *
     * @return Reference to the internal error string.
     */
    const juce::String& getLoadError() const { return loadError; }

    //==============================================================================
    /**
     * @brief Parses a colour string in several supported formats.
     *
     * Supported formats:
     * | Format          | Example                  | Notes                        |
     * |-----------------|--------------------------|------------------------------|
     * | `#RGB`          | `#F0A`                   | Expands each nibble × 17     |
     * | `#RGBA`         | `#F0A8`                  | Expands each nibble × 17     |
     * | `#RRGGBB`       | `#FF00AA`                | Fully opaque RGB             |
     * | `#RRGGBBAA`     | `#FF00AA80`              | Alpha in low byte            |
     * | `rgba(r,g,b,a)` | `rgba(255,0,128,0.5)`    | Alpha is float [0, 1]        |
     *
     * @param input  The colour string to parse (leading/trailing whitespace is trimmed).
     * @return The parsed `juce::Colour`, or `juce::Colours::magenta` on parse failure.
     * @note Triggers `jassertfalse` on unrecognised format in debug builds.
     */
    static juce::Colour parseColour (const juce::String& input);

    //==============================================================================
    /**
     * @struct Value
     * @brief Schema entry describing the expected type and optional range for a key.
     *
     * Used by `load()` to validate values read from Lua before storing them.
     * Out-of-range numbers are rejected with a warning; type mismatches are
     * also warned but the default value is kept.
     *
     * @note Public so that file-scope helper functions in Config.cpp can reference
     *       `Config::Value` without requiring friend declarations.
     */
    struct Value
    {
        /** @brief Expected Lua type for this key. */
        enum class Type
        {
            string,
            number
        };

        /** @brief The expected Lua type. */
        Type expectedType;

        /** @brief Minimum allowed value (inclusive); only checked when `hasRange` is true. */
        double minValue { 0.0 };

        /** @brief Maximum allowed value (inclusive); only checked when `hasRange` is true. */
        double maxValue { 0.0 };

        /** @brief Whether `minValue` / `maxValue` are enforced. */
        bool hasRange { false };
    };

private:
    //==============================================================================
    /** @brief Runtime value store: key → juce::var (string, double, or bool). */
    std::unordered_map<juce::String, juce::var> values;

    /** @brief Schema map: key → Value for type and range validation. */
    std::unordered_map<juce::String, Value> schema;

    /** @brief Parsed popup entries from the `popups` Lua table. */
    std::unordered_map<juce::String, PopupEntry> popups;

    /**
     * @brief Parsed colour cache; keyed by config key, not by colour string.
     *
     * Populated lazily by `getColour()` and cleared on every `load()` and
     * `reload()` call so stale entries cannot survive a config change.
     * Declared `mutable` because the cache is a pure optimisation with no
     * observable semantic effect on the Config's logical state.
     */
    mutable std::unordered_map<juce::String, juce::Colour> colourCache;

    /** @brief Last load error or warning string; empty if the last load was clean. */
    juce::String loadError;

    /**
     * @brief Per-extension command overrides from the `hyperlinks.handlers` Lua table.
     *
     * Keys are lowercase extensions with a leading dot (e.g. `".pdf"`).
     * Values are shell commands (e.g. `"open -a Preview"`).
     * Populated by the `hyperlinks` group loader; cleared on every `reload()`.
     */
    std::unordered_map<juce::String, juce::String> hyperlinkHandlers;

    /**
     * @brief Extra clickable extensions from the `hyperlinks.extensions` Lua array.
     *
     * Lowercase extensions with a leading dot (e.g. `".vue"`).  Extends the
     * built-in set in `LinkDetector::builtInExtensions()` without a handler —
     * these extensions fall back to the `editor` command.
     * Populated by the `hyperlinks` group loader; cleared on every `reload()`.
     */
    std::unordered_set<juce::String> hyperlinkExtensions;

    //==============================================================================
    /**
     * @brief Registers a single config key with its default value and schema spec.
     *
     * Called exclusively from `initKeys()`.  Inserts into both `values` and
     * `schema` in one call, eliminating the former parallel
     * `initDefaults` / `initSchema` pattern.
     *
     * @param key          The dot-notation key string (e.g. `"font.size"`).
     * @param defaultVal   Default value stored in the values map.
     * @param spec         Type and range constraints for validation.
     */
    void addKey (const juce::String& key, const juce::var& defaultVal, Value spec);

    /**
     * @brief Populates both `values` and `schema` from a single unified key table.
     *
     * Replaces the former parallel `initDefaults()` + `initSchema()` pair.
     * Every key is declared exactly once — default value and validation spec
     * together — making it impossible for the two maps to diverge.
     *
     * Called at construction and at the start of `reload()`.
     */
    void initKeys();

    /** @brief Clears all popup entries; called at the start of reload(). */
    void clearPopups();

    /**
     * @brief Writes a minimal `END = {}` skeleton to @p file.
     *
     * Called when `end.lua` does not exist so the user has a valid starting
     * point to customise.
     *
     * @param file  The file to create (parent directory must already exist).
     */
    void writeDefaults (const juce::File& file) const;

    //==============================================================================
    /** @brief Prefix prepended to all user-visible config error messages. */
    static constexpr const char* configErrorPrefix { "Config: close, but no cigar.\n" };

    /**
     * @brief Lua snippet injected before the user config to detect undefined globals.
     *
     * Sets a `__index` metamethod on `_G` that records every access to an
     * undefined global variable (name + line number) into the `_undefined`
     * table.  After the user script runs, `load()` iterates `_undefined` and
     * emits a warning for each entry.
     *
     * @note This is a static string compiled into the binary; it is not
     *       user-visible and cannot be overridden.
     */
    inline static const std::string validationScript { R"lua(
_undefined = {}
local _insert = table.insert
local _getinfo = debug.getinfo

setmetatable(_G, {
    __index = function(_, key)
        local info = _getinfo(2, "l")
        local line = info and info.currentline or 0
        _insert(_undefined, {name = key, line = line})
        return nil
    end
})
)lua" };
};

