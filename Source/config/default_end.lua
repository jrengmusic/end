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
--	                Ephemeral Nexus Display  v%%versionString%%
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
--   - "#RGB" and "#RGBA" shorthand supported (e.g. "#F00" becomes "#FF0000").
--   - "rgba(r, g, b, a)" functional notation (a is 0.0 - 1.0).
--
-- Key binding format: "modifier+key" (e.g. "cmd+c", "ctrl+shift+t").
--   - Modifiers: cmd, ctrl, alt, shift
--   - Some keys use a two-step sequence: press the prefix key first, then the
--     action key. See the keys section below.
--
-- ============================================================================

END = {

	-- ========================================================================
	-- GPU
	-- ========================================================================
	--
	-- Rendering backend selection. Hot-reloadable (Cmd+R).
	--
	-- "auto"  — Use GPU if available, CPU fallback. (default)
	-- "true"  — Force GPU rendering. Falls back to CPU if unavailable.
	-- "false" — Force CPU rendering. No GPU used.
	--

	gpu = "%%gpu%%",

	-- ========================================================================
	-- NEXUS
	-- ========================================================================
	--
	-- Enable the Nexus background daemon.
	-- When "true", sessions survive window close. Relaunch reconnects.
	-- When "false", sessions die with the window (no daemon).
	--

	daemon = "%%daemon%%",

	-- ========================================================================
	-- FONT
	-- ========================================================================

	font = {
		-- Font used for terminal text.
		-- Must be a monospace font installed on the system.
		family = "%%font_family%%",

		-- Font size in points before zoom is applied (1 - 200).
		size = %%font_size%%,

		-- Combine certain character sequences into symbols (e.g. -> becomes an arrow).
		ligatures = "%%font_ligatures%%",

		-- Make text appear bolder.
		-- Useful for thin fonts that are hard to read at small sizes.
		embolden = "%%font_embolden%%",

		-- Line height multiplier applied to terminal cell height (0.5 - 3.0).
		-- 1.0 = no adjustment. Values above 1.0 increase spacing, below decrease it.
		line_height = %%font_line_height%%,

		-- Cell width multiplier applied to terminal cell width (0.5 - 3.0).
		-- 1.0 = no adjustment. Values above 1.0 widen cells, below narrow them.
		cell_width = %%font_cell_width%%,

		-- Whether font size follows the Windows desktop scale.
		-- "true"  — font size scales with the desktop scale (system default behaviour).
		-- "false" — font stays at its configured point size in physical pixels
		--           regardless of the desktop scale slider (12pt always looks 12pt).
		-- Windows only. No effect on macOS or Linux.
		desktop_scale = "%%font_desktop_scale%%",
	},

	-- ========================================================================
	-- CURSOR
	-- ========================================================================

	cursor = {
		-- Character displayed as the cursor.
		-- Default is a solid block (the standard blinking rectangle).
		-- You can use any char, NF icons, including color emoji.
		-- Only used when cursor shape is "glyph" (user-defined).
		-- Programs (like vim or tmux) can change the cursor shape
		-- unless cursor.force is true.
		char = "%%cursor_char%%",

		-- Enable cursor blinking.
		blink = "%%cursor_blink%%",

		-- Blink interval in milliseconds (100 - 5000).
		-- Full cycle = 2x this value (on for interval, off for interval).
		blink_interval = %%cursor_blink_interval%%,

		-- Lock the cursor to your configured shape and colour. Programs cannot change it.
		-- When "true", programs cannot change cursor shape or colour.
		force = "%%cursor_force%%",
	},

	-- ========================================================================
	-- COLOURS
	-- ========================================================================
	--
	-- The 16 standard terminal colours. Programs like ls, git, and vim use these.
	-- The first 8 are normal, the next 8 are brighter versions.
	-- Format: "#RRGGBB" (opaque) or "#RRGGBBAA" (with alpha).
	--

	colours = {
		-- Default text foreground colour.
		foreground = "%%colours_foreground%%",

		-- Default background colour.
		-- The last two hex digits control background transparency (GPU only).
		-- CPU rendering always uses a fully opaque background.
		background = "%%colours_background%%",

		-- Cursor colour.
		-- Programs may change this colour while running.
		cursor = "%%colours_cursor%%",

		-- Selection highlight colour.
		-- Semi-transparent recommended so text remains readable.
		selection = "%%colours_selection%%",

		-- Selection-mode cursor colour.
		-- Shown instead of the normal cursor when selection mode is active.
		selection_cursor = "%%colours_selection_cursor%%",

		-- Black
		black = "%%colours_black%%",

		-- Red
		red = "%%colours_red%%",

		-- Green
		green = "%%colours_green%%",

		-- Yellow
		yellow = "%%colours_yellow%%",

		-- Blue
		blue = "%%colours_blue%%",

		-- Magenta
		magenta = "%%colours_magenta%%",

		-- Cyan
		cyan = "%%colours_cyan%%",

		-- White
		white = "%%colours_white%%",

		-- Bright black
		bright_black = "%%colours_bright_black%%",

		-- Bright red
		bright_red = "%%colours_bright_red%%",

		-- Bright green
		bright_green = "%%colours_bright_green%%",

		-- Bright yellow
		bright_yellow = "%%colours_bright_yellow%%",

		-- Bright blue
		bright_blue = "%%colours_bright_blue%%",

		-- Bright magenta
		bright_magenta = "%%colours_bright_magenta%%",

		-- Bright cyan
		bright_cyan = "%%colours_bright_cyan%%",

		-- Bright white
		bright_white = "%%colours_bright_white%%",

		-- Status bar full background colour.
		-- Default matches the active tab background (tab.active).
		status_bar = "%%colours_status_bar%%",

		-- Status bar mode label background colour.
		-- Default matches the active tab indicator colour (tab.indicator).
		status_bar_label_bg = "%%colours_status_bar_label_bg%%",

		-- Status bar mode label text colour.
		status_bar_label_fg = "%%colours_status_bar_label_fg%%",

		-- Status bar spinner colour.
		status_bar_spinner = "%%colours_status_bar_spinner%%",

		-- Status bar font family.
		status_bar_font_family = "%%colours_status_bar_font_family%%",

		-- Status bar font size in points.
		status_bar_font_size = %%colours_status_bar_font_size%%,

		-- Status bar font style.
		status_bar_font_style = "%%colours_status_bar_font_style%%",

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

		-- Tint colour for the window background. Most visible with blur enabled.
		colour = "%%window_colour%%",

		-- Window opacity (0.0 fully transparent - 1.0 fully opaque).
		-- GPU only. Has no effect with CPU rendering.
		-- macOS and Windows 10 only. No effect on Windows 11.
		opacity = %%window_opacity%%,

		-- Background blur radius in pixels (0 = no blur).
		-- GPU only. Has no effect with CPU rendering.
		-- macOS: controls blur intensity.
		-- Windows 10: blur is on but intensity is set by the system.
		-- Windows 11: uses the system glass effect. This setting has no effect.
		blur_radius = %%window_blur_radius%%,

		-- Keep window above all other windows.
		always_on_top = "%%window_always_on_top%%",

		-- Show native window buttons (close / minimise / maximise).
		buttons = "%%window_buttons%%",

		-- Force DWM visual effects on Windows 11 virtual machines.
		-- When "true", injects the ForceEffectMode registry key to enable
		-- rounded window corners that DWM normally disables inside VMs.
		-- Only takes effect on Windows 11 running on a software renderer (VM).
		-- Requires elevated privileges (Run as Administrator).
		-- Reload config and restart END to apply.
		-- No effect on macOS, Linux, or physical Windows machines.
		force_dwm = "%%window_force_dwm%%",

		-- Persist window size across instances to ~/.config/end/window.state.
		-- When "true", every quit writes the current window size; new instances
		-- (with no session to restore) load it as their initial size.
		-- When "false", the file is neither read nor written; new instances
		-- fall back to window.width and window.height above.
		-- Restored sessions always use their own persisted window size and
		-- ignore window.state regardless of this setting.
		save_size = "%%window_save_size%%",

		-- Show a confirmation dialog when Ctrl+Q is pressed.
		-- When "true", a Yes/No dialog asks before quitting (or saving the session
		-- in daemon mode).  When "false", Ctrl+Q quits immediately with no prompt.
		confirmation_on_exit = "%%window_confirmation_on_exit%%",
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

		-- Enable automatic shell integration.
		-- When "true", END creates shell hook scripts in ~/.config/end/
		-- and injects them on shell startup. This enables:
		--   - Clickable file links in command output
		--   - Output block detection for the Open File feature
		-- Supported shells: zsh, bash, fish.
		-- Set to "false" to disable and remove integration scripts.
		integration = "%%shell_integration%%",
	},

	-- ========================================================================
	-- TERMINAL
	-- ========================================================================

	terminal = {
		-- Maximum number of lines you can scroll back through (100 - 1000000).
		scrollback_lines = %%terminal_scrollback_lines%%,

		-- Lines scrolled per mouse wheel tick and per Shift+PgUp/PgDn step (1 - 100).
		scroll_step = %%terminal_scroll_step%%,

		-- Space between the window edge and the terminal text, in pixels. Four values:
		--   { top, right, bottom, left }
		-- All four values must be present.  Valid range: 0 - 200.
		-- Example: { 10, 10, 10, 10 } gives equal padding on all sides.
		--          { 4, 10, 10, 10 } gives a tighter top edge.
		padding = { %%terminal_padding_top%%, %%terminal_padding_right%%, %%terminal_padding_bottom%%, %%terminal_padding_left%% },

		-- Separator for multiple dropped file paths.
		-- "space" joins paths with spaces (shell convention).
		-- "newline" joins paths with newlines.
		drop_multifiles = "%%terminal_drop_multifiles%%",

		-- Wrap dropped file paths in quotes so spaces and special characters work correctly.
		-- "true": paths with special characters are quoted for the active shell.
		-- "false": paths are pasted raw (for TUI apps that handle paths directly).
		drop_quoted = "%%terminal_drop_quoted%%",
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

		-- Open a new window.
		new_window = "%%keys_new_window%%",

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
	-- ACTION LIST
	-- ========================================================================

	action_list = {
		-- Close the action list after running an action.
		-- When "false", the list stays open after execution.
		close_on_run = "%%action_list_close_on_run%%",

		-- Font family for action name labels.
		name_font_family = "%%action_list_name_font_family%%",

		-- Action-list action-name font style (Regular, Bold, Book, Medium).
		name_font_style = "%%action_list_name_font_style%%",

		-- Font size for action name labels in points (6 - 72).
		name_font_size = %%action_list_name_font_size%%,

		-- Font family for keyboard shortcut labels. Should be monospace.
		shortcut_font_family = "%%action_list_shortcut_font_family%%",

		-- Action-list shortcut font style (Regular, Bold).
		shortcut_font_style = "%%action_list_shortcut_font_style%%",

		-- Font size for keyboard shortcut labels in points (6 - 72).
		shortcut_font_size = %%action_list_shortcut_font_size%%,

		-- Space between the action list edge and content, in pixels.
		-- Four values: { top, right, bottom, left }. Valid range: 0 - 200.
		padding = { %%action_list_padding_top%%, %%action_list_padding_right%%, %%action_list_padding_bottom%%, %%action_list_padding_left%% },

		-- Text colour for action name labels.
		name_colour = "%%action_list_name_colour%%",

		-- Text colour for keyboard shortcut labels.
		shortcut_colour = "%%action_list_shortcut_colour%%",

		-- Proportional width of the action list relative to the terminal window (0.1 - 1.0).
		width = %%action_list_width%%,

		-- Maximum proportional height of the action list relative to the terminal window (0.1 - 1.0).
		-- When all results exceed this height, the list scrolls.
		height = %%action_list_height%%,

		-- Background colour for the highlighted/selected row.
		-- Leave empty to use the terminal selection colour (colours.selection).
		highlight_colour = "%%action_list_highlight_colour%%",
	},

	-- ========================================================================
	-- POPUP DEFAULTS
	-- ========================================================================

	popup = {
		-- Default popup width in columns.
		-- Individual popup entries can override this.
		cols = %%popup_cols%%,

		-- Default popup height in rows.
		-- Individual popup entries can override this.
		rows = %%popup_rows%%,

		-- Default popup position: "center".
		position = "%%popup_position%%",

		-- Popup border colour.
		border_colour = "%%popup_border_colour%%",

		-- Popup border stroke width in pixels (0 = no border).
		border_width = %%popup_border_width%%,
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
	-- in a floating panel on top of the terminal. The popup blocks the main
	-- window until the process exits (quit the TUI, Ctrl+C a script, etc.).
	--
	-- Each entry is a named table. The table key is the unique identifier.
	--
	-- Fields:
	--   command  (string, required)  Shell command or executable to run.
	--   args     (string, optional)  Arguments passed to the command.
	--   cwd      (string, optional)  Working directory. Empty = inherit active terminal cwd.
	--   cols     (number, optional)  Width in columns. Overrides popup.cols.
	--   rows     (number, optional)  Height in rows. Overrides popup.rows.
	--   modal    (string, optional)  Key pressed after the prefix key (e.g. "t").
	--   global   (string, optional)  Global key: direct shortcut, no prefix needed.
	--
	-- At least one of modal or global is required.
	-- Both can coexist on the same entry.
	--
	-- Examples:
	--
	popups = {
		tit = {
			command = "tit",
			args = "",
			cwd = "",
			cols = 80,
			rows = 30,
			modal = "t",
		},
		cake = {
			command = "cake",
			args = "",
			cwd = "",
			cols = 80,
			rows = 30,
			modal = "c",
		},
		btop = {
			command = "btop",
			args = "",
			cwd = "",
			cols = 100,
			rows = 40,
			modal = "q",
		},
	},
}
