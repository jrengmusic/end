--	████████████████████  ████████████    ████  ████    ████    ████
--	████████████████████  ████████████    ████  ████    ████    ████
--	████░░░░░░░░░░░░████  ████░░░░████    ████  ░░░░    ░░░░    ████
--	████            ████  ████    ████    ████                  ████
--	████████████████████  ████    ████    ████  ████████████████████
--	████████████████████  ████    ████    ████  ████████████████████
--	████░░░░░░░░░░░░░░░░  ████    ████    ████  ████░░░░░░░░░░░░████
--	████                  ████    ████    ████  ████            ████
--	████████████████████  ████    ████████████  ████████████████████
--	████████████████████  ████    ████████████  ████████████████████
--	░░░░░░░░░░░░░░░░░░░░  ░░░░    ░░░░░░░░░░░░  ░░░░░░░░░░░░░░░░░░░░
-- 
--	                Ephemeral Nexus Display  v%versionString%
-- ============================================================================
-- Configuration
-- https://github.com/jrengmusic/end
-- ============================================================================
--
-- This file is auto-generated with default values on first launch.
-- Edit any value below to customise your terminal.
-- Invalid or missing values fall back to defaults silently.
-- Reload with Cmd+R (no restart needed).
--
-- Colour format: "#RRGGBB" (fully opaque) or "#RRGGBBAA" (with alpha).
--   - "#RGB" and "#RGBA" shorthand supported (each nibble expanded to two digits).
--   - "rgba(r, g, b, a)" functional notation (a is 0.0 - 1.0).
--
-- Key binding format: "modifier+key" (e.g. "cmd+c", "ctrl+shift+t").
--   - Modifiers: cmd, ctrl, alt, shift
--   - Prefix-mode keys (split, pane navigation) are single characters
--     activated after pressing the prefix key.
--
-- ============================================================================

END = {

	-- ========================================================================
	-- FONT
	-- ========================================================================

	font = {
		-- Font family name for the terminal grid.
		-- Must be a monospace font installed on the system.
		family = "%%font_family%%",

		-- Font size in points before zoom is applied (1 - 200).
		size = %%font_size%%,

		-- Enable OpenType ligature substitution (e.g. -> becomes arrow).
		ligatures = %%font_ligatures%%,

		-- Embolden glyphs for heavier strokes.
		-- Useful for thin fonts that are hard to read at small sizes.
		embolden = %%font_embolden%%,
	},

	-- ========================================================================
	-- CURSOR
	-- ========================================================================

	cursor = {
		-- Unicode character used as the cursor glyph.
		-- Default is the full block character U+2588.
		-- Only used when cursor shape is "glyph" (user-defined).
		-- Programs can override shape via DECSCUSR escape sequence
		-- unless cursor.force is true.
		char = "%%cursor_char%%",

		-- Enable cursor blinking.
		blink = %%cursor_blink%%,

		-- Blink interval in milliseconds (100 - 5000).
		-- Full cycle = 2x this value (on for interval, off for interval).
		blink_interval = %%cursor_blink_interval%%,

		-- Force user-configured cursor, ignoring DECSCUSR and OSC 12.
		-- When true, programs cannot change cursor shape or colour.
		force = %%cursor_force%%,
	},

	-- ========================================================================
	-- COLOURS
	-- ========================================================================
	--
	-- The 16 ANSI colours are used by terminal programs via SGR escape codes.
	-- Indices 0-7 are normal colours, 8-15 are bright variants.
	-- Format: "#RRGGBB" (opaque) or "#RRGGBBAA" (with alpha).
	--

	colours = {
		-- Default text foreground colour.
		foreground = "%%colours_foreground%%",

		-- Default background colour.
		-- Alpha channel controls terminal background opacity.
		background = "%%colours_background%%",

		-- Cursor colour.
		-- Can be overridden per-session by programs via OSC 12.
		cursor = "%%colours_cursor%%",

		-- Selection highlight colour.
		-- Semi-transparent recommended so text remains readable.
		selection = "%%colours_selection%%",

		-- Selection-mode cursor colour.
		-- Shown instead of the normal cursor when selection mode is active.
		selection_cursor = "%%colours_selection_cursor%%",

		-- ANSI colour 0: black
		black = "%%colours_black%%",

		-- ANSI colour 1: red
		red = "%%colours_red%%",

		-- ANSI colour 2: green
		green = "%%colours_green%%",

		-- ANSI colour 3: yellow
		yellow = "%%colours_yellow%%",

		-- ANSI colour 4: blue
		blue = "%%colours_blue%%",

		-- ANSI colour 5: magenta
		magenta = "%%colours_magenta%%",

		-- ANSI colour 6: cyan
		cyan = "%%colours_cyan%%",

		-- ANSI colour 7: white
		white = "%%colours_white%%",

		-- ANSI colour 8: bright black
		bright_black = "%%colours_bright_black%%",

		-- ANSI colour 9: bright red
		bright_red = "%%colours_bright_red%%",

		-- ANSI colour 10: bright green
		bright_green = "%%colours_bright_green%%",

		-- ANSI colour 11: bright yellow
		bright_yellow = "%%colours_bright_yellow%%",

		-- ANSI colour 12: bright blue
		bright_blue = "%%colours_bright_blue%%",

		-- ANSI colour 13: bright magenta
		bright_magenta = "%%colours_bright_magenta%%",

		-- ANSI colour 14: bright cyan
		bright_cyan = "%%colours_bright_cyan%%",

		-- ANSI colour 15: bright white
		bright_white = "%%colours_bright_white%%",

		-- Status bar full background colour.
		-- Default matches the active tab background (tab.active).
		status_bar = "%%colours_status_bar%%",

		-- Status bar mode label background colour.
		-- Default matches the active tab indicator colour (tab.indicator).
		status_bar_label_bg = "%%colours_status_bar_label_bg%%",

		-- Status bar mode label text colour.
		status_bar_label_fg = "%%colours_status_bar_label_fg%%",

		-- Open File mode hint label background colour.
		-- Shown as the badge background behind single- or double-letter hint keys.
		hint_label_bg = "%%colours_hint_label_bg%%",

		-- Open File mode hint label foreground (text) colour.
		hint_label_fg = "%%colours_hint_label_fg%%",
	},

	-- ========================================================================
	-- WINDOW
	-- ========================================================================

	window = {
		-- Window title shown in the title bar and mission control.
		title = "%%window_title%%",

		-- Initial window width in pixels.
		width = %%window_width%%,

		-- Initial window height in pixels.
		height = %%window_height%%,

		-- Window background tint colour (no alpha, used for blur tint).
		colour = "%%window_colour%%",

		-- Window opacity (0.0 fully transparent - 1.0 fully opaque).
		opacity = %%window_opacity%%,

		-- Background blur radius in pixels (0 = no blur).
		-- macOS: controls blur intensity via CoreGraphics private SPI.
		-- Windows: blur intensity is controlled by DWM (this value is accepted
		-- but not forwarded — the acrylic effect has a fixed blur radius).
		blur_radius = %%window_blur_radius%%,

		-- Keep window above all other windows.
		always_on_top = %%window_always_on_top%%,

		-- Show native window buttons (close / minimise / maximise).
		buttons = %%window_buttons%%,

		-- Zoom multiplier (1.0 - 4.0).
		-- Scales the terminal grid and font proportionally.
		zoom = %%window_zoom%%,
	},

	-- ========================================================================
	-- TAB BAR
	-- ========================================================================

	tab = {
		-- Tab bar font family.
		family = "%%tab_family%%",

		-- Tab bar font size in points.
		size = %%tab_size%%,

		-- Active tab text colour.
		foreground = "%%tab_foreground%%",

		-- Inactive tab text colour.
		inactive = "%%tab_inactive%%",

		-- Tab bar position: "top", "bottom", "left", "right".
		position = "%%tab_position%%",

		-- Tab separator line colour.
		line = "%%tab_line%%",

		-- Active tab background colour.
		active = "%%tab_active%%",

		-- Active tab indicator colour.
		indicator = "%%tab_indicator%%",
	},

	-- ========================================================================
	-- MENU
	-- ========================================================================

	menu = {
		-- Popup menu background opacity (0.0 - 1.0).
		opacity = %%menu_opacity%%,
	},

	-- ========================================================================
	-- OVERLAY
	-- ========================================================================

	overlay = {
		-- Overlay font family (used for status messages).
		family = "%%overlay_family%%",

		-- Overlay font size in points.
		size = %%overlay_size%%,

		-- Overlay text colour.
		colour = "%%overlay_colour%%",
	},

	-- ========================================================================
	-- SHELL
	-- ========================================================================

	shell = {
		-- Shell program name or absolute path.
		program = "%%shell_program%%",

		-- Arguments passed to the shell program.
		args = "%%shell_args%%",

		-- Enable automatic shell integration (OSC 133 markers).
		-- When true, END creates shell hook scripts in ~/.config/end/
		-- and injects them on shell startup. This enables:
		--   - Clickable file links in command output
		--   - Output block detection for the Open File feature
		-- Supported shells: zsh, bash, fish.
		-- Set to false to disable and remove integration scripts.
		integration = %%shell_integration%%,
	},

	-- ========================================================================
	-- TERMINAL
	-- ========================================================================

	terminal = {
		-- Maximum number of scrollback lines retained in the ring buffer (100 - 1000000).
		scrollback_lines = %%terminal_scrollback_lines%%,

		-- Lines scrolled per mouse wheel tick and per Shift+PgUp/PgDn step (1 - 100).
		scroll_step = %%terminal_scroll_step%%,

		-- Grid padding in logical pixels — space between the window edge and the
		-- terminal grid on each side.  Four values in CSS order:
		--   { top, right, bottom, left }
		-- All four values must be present.  Valid range: 0 - 200.
		-- Example: { 10, 10, 10, 10 } gives equal padding on all sides.
		--          { 4, 10, 10, 10 } gives a tighter top edge.
		padding = { %%terminal_padding_top%%, %%terminal_padding_right%%, %%terminal_padding_bottom%%, %%terminal_padding_left%% },

		-- Separator for multiple dropped file paths.
		-- "space" joins paths with spaces (shell convention).
		-- "newline" joins paths with newlines.
		drop_multifiles = "%%terminal_drop_multifiles%%",

		-- Whether dropped file paths are shell-quoted.
		-- true: paths with special characters are quoted for the active shell.
		-- false: paths are pasted raw (for TUI apps that handle paths directly).
		drop_quoted = %%terminal_drop_quoted%%,
	},

	-- ========================================================================
	-- PANE
	-- ========================================================================

	pane = {
		-- Pane divider bar colour.
		bar_colour = "%%pane_bar_colour%%",

		-- Pane divider bar colour when dragging or hovering.
		bar_highlight = "%%pane_bar_highlight%%",
	},

	-- ========================================================================
	-- KEY BINDINGS
	-- ========================================================================
	--
	-- Direct key bindings use "modifier+key" format (e.g. "cmd+c").
	-- Prefix-mode keys are single characters pressed AFTER the prefix key.
	-- Prefix mode: press prefix key, then within timeout press the action key.
	--

	keys = {
		-- Copy selection to clipboard.
		copy = "%%keys_copy%%",

		-- Paste from clipboard.
		paste = "%%keys_paste%%",

		-- Quit application.
		quit = "%%keys_quit%%",

		-- Close active pane, then tab, then window.
		close_tab = "%%keys_close_tab%%",

		-- Reload configuration file.
		reload = "%%keys_reload%%",

		-- Increase zoom level.
		zoom_in = "%%keys_zoom_in%%",

		-- Decrease zoom level.
		zoom_out = "%%keys_zoom_out%%",

		-- Reset zoom to 1.0.
		zoom_reset = "%%keys_zoom_reset%%",

		-- Open a new tab.
		new_tab = "%%keys_new_tab%%",

		-- Switch to previous tab.
		prev_tab = "%%keys_prev_tab%%",

		-- Switch to next tab.
		next_tab = "%%keys_next_tab%%",

		-- Split pane horizontally (left/right). Prefix-mode key.
		split_horizontal = "%%keys_split_horizontal%%",

		-- Split pane vertically (top/bottom). Prefix-mode key.
		split_vertical = "%%keys_split_vertical%%",

		-- Prefix key for modal pane commands.
		-- Press this key first, then press a pane action key within the timeout.
		prefix = "%%keys_prefix%%",

		-- Prefix key timeout in milliseconds (100 - 5000).
		-- How long to wait for a pane action key after pressing prefix.
		prefix_timeout = %%keys_prefix_timeout%%,

		-- Focus pane to the left. Prefix-mode key.
		pane_left = "%%keys_pane_left%%",

		-- Focus pane below. Prefix-mode key.
		pane_down = "%%keys_pane_down%%",

		-- Focus pane above. Prefix-mode key.
		pane_up = "%%keys_pane_up%%",

		-- Focus pane to the right. Prefix-mode key.
		pane_right = "%%keys_pane_right%%",

		-- Insert a literal newline (LF) instead of carriage return.
		newline = "%%keys_newline%%",

		-- Enter text selection mode. Prefix-mode key.
		enter_selection = "%%keys_enter_selection%%",

		-- Enter open-file mode (hyperlink hint labels). Prefix-mode key.
		enter_open_file = "%%keys_enter_open_file%%",

		-- ---- Selection mode ----

		-- Move cursor up in selection mode.
		selection_up = "%%keys_selection_up%%",

		-- Move cursor down in selection mode.
		selection_down = "%%keys_selection_down%%",

		-- Move cursor left in selection mode.
		selection_left = "%%keys_selection_left%%",

		-- Move cursor right in selection mode.
		selection_right = "%%keys_selection_right%%",

		-- Toggle character-wise visual selection.
		selection_visual = "%%keys_selection_visual%%",

		-- Toggle line-wise visual selection.
		selection_visual_line = "%%keys_selection_visual_line%%",

		-- Toggle block visual selection.
		selection_visual_block = "%%keys_selection_visual_block%%",

		-- Yank (copy) the current selection and exit selection mode.
		selection_copy = "%%keys_selection_copy%%",

		-- Jump to top of buffer (press twice: gg).
		selection_top = "%%keys_selection_top%%",

		-- Jump to bottom of buffer.
		selection_bottom = "%%keys_selection_bottom%%",

		-- Jump to start of current line.
		selection_line_start = "%%keys_selection_line_start%%",

		-- Jump to end of current line.
		selection_line_end = "%%keys_selection_line_end%%",

		-- Exit selection mode.
		selection_exit = "%%keys_selection_exit%%",

		-- Open the action list (command palette).
		action_list = "%%keys_action_list%%",

		-- Action list position: "top" or "bottom".
		action_list_position = "%%keys_action_list_position%%",

		-- Status bar position: "top" or "bottom".
		status_bar_position = "%%keys_status_bar_position%%",
	},

	-- ========================================================================
	-- POPUP DEFAULTS
	-- ========================================================================

	popup = {
		-- Default popup width as a fraction of the window width (0.1 - 1.0).
		-- Individual popup entries can override this.
		width = %%popup_width%%,

		-- Default popup height as a fraction of the window height (0.1 - 1.0).
		-- Individual popup entries can override this.
		height = %%popup_height%%,

		-- Default popup position: "center".
		position = "%%popup_position%%",
	},

	-- ========================================================================
	-- HYPERLINKS
	-- ========================================================================

	hyperlinks = {
		-- Editor command for opening files from hyperlinks and Open File mode.
		-- The command receives the file path as its first argument.
		-- Example: "nvim", "vim", "nano", "/usr/local/bin/hx"
		editor = "%%hyperlinks_editor%%",

		-- Per-extension handler commands (override the editor for specific file types).
		-- Keys are file extensions (with leading dot), values are shell commands.
		-- handlers = {
		--     [".png"] = "open",
		--     [".pdf"] = "open -a Preview",
		-- },

		-- Extra clickable extensions beyond the built-in set.
		-- Use this for frameworks or custom extensions not in the built-in list.
		-- These fall back to the editor command.
		-- extensions = { ".vue", ".svelte", ".astro" },
	},

	-- ========================================================================
	-- POPUPS
	-- ========================================================================
	--
	-- Modal popup terminals. Each entry spawns a terminal running a command
	-- in a glass overlay window. The popup blocks the main window until the
	-- process exits (quit the TUI, Ctrl+C a script, etc.).
	--
	-- Each entry is a named table. The table key is the unique identifier.
	--
	-- Fields:
	--   command  (string, required)  Shell command or executable to run.
	--   args     (string, optional)  Arguments passed to the command.
	--   cwd      (string, optional)  Working directory. Empty = inherit active terminal cwd.
	--   width    (number, optional)  Fraction of window width (0.1-1.0). Overrides popup.width.
	--   height   (number, optional)  Fraction of window height (0.1-1.0). Overrides popup.height.
	--   modal    (string, optional)  Modal key: prefix + key. Can include modifiers (e.g. "cmd+t").
	--   global   (string, optional)  Global key: direct shortcut, no prefix needed.
	--
	-- At least one of modal or global is required.
	-- Both can coexist on the same entry.
	--
	-- Examples:
	--
	-- popups = {
	--     tit = {
	--         command = "tit.exe",
	--         args = "",
	--         cwd = "",
	--         width = 0.8,
	--         height = 0.6,
	--         modal = "t",
	--     },
	--     lazygit = {
	--         command = "lazygit",
	--         width = 0.9,
	--         height = 0.9,
	--         modal = "g",
	--     },
	--     htop = {
	--         command = "htop",
	--         cwd = "~",
	--         width = 0.7,
	--         height = 0.5,
	--         modal = "p",
	--         global = "cmd+shift+p",
	--     },
	-- },
}
