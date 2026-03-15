-- ============================================================================
-- END Terminal Emulator Configuration
-- https://github.com/jreng/end
-- ============================================================================
--
-- This file is auto-generated with default values on first launch.
-- Edit any value below to customise your terminal.
-- Invalid or missing values fall back to defaults silently.
-- Reload with Cmd+R (no restart needed).
--
-- Colour format: "#AARRGGBB" where AA = alpha, RR/GG/BB = red/green/blue.
--   - "#RRGGBB" is also accepted (fully opaque, alpha = FF).
--   - "#RGB" shorthand is supported (each nibble expanded to two digits).
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
	-- Format: "#AARRGGBB" (alpha + red + green + blue).
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
		-- Examples: "zsh", "bash", "fish", "/opt/homebrew/bin/fish"
		program = "%%shell_program%%",

		-- Arguments passed to the shell program (space-separated string).
		-- Default: "-l" on Unix (login shell), "" on Windows.
		-- Set to "" to launch the shell with no arguments.
		args = "%%shell_args%%",
	},

	-- ========================================================================
	-- SCROLLBACK
	-- ========================================================================

	scrollback = {
		-- Maximum number of scrollback lines retained (100 - 1000000).
		num_lines = %%scrollback_num_lines%%,

		-- Lines scrolled per mouse wheel tick (1 - 100).
		step = %%scrollback_step%%,
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
	},
}
