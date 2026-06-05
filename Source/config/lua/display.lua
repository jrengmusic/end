-- ============================================================================
-- END display.lua — visual appearance settings
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
-- ============================================================================

return {

	-- ========================================================================
	-- FONT
	-- ========================================================================

	font = {
		-- Font used for terminal text.
		-- Must be a monospace font installed on the system.
		family = "Display Mono",

		-- Font size in points before zoom is applied (1 - 200).
		size = 12,

		-- Combine certain character sequences into symbols (e.g. -> becomes an arrow).
		ligatures = "true",

		-- Make text appear bolder.
		-- Useful for thin fonts that are hard to read at small sizes.
		embolden = "true",

		-- Line height multiplier applied to terminal cell height (0.5 - 3.0).
		-- 1.0 = no adjustment. Values above 1.0 increase spacing, below decrease it.
		line_height = 1,

		-- Cell width multiplier applied to terminal cell width (0.5 - 3.0).
		-- 1.0 = no adjustment. Values above 1.0 widen cells, below narrow them.
		cell_width = 1,

		-- Whether font size follows the Windows desktop scale.
		-- "true"  — font size scales with the desktop scale (system default behaviour).
		-- "false" — font stays at its configured point size in physical pixels
		--           regardless of the desktop scale slider (12pt always looks 12pt).
		-- Windows only. No effect on macOS or Linux.
		desktop_scale = "false",
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
		char = "█",

		-- Geometric cursor shape used when the cursor glyph cannot render.
		-- "block" (default), "underline", or "bar".
		-- Programs (like vim) can change this via DECSCUSR unless cursor.force is true.
		style = "block",

		-- Enable cursor blinking.
		blink = "true",

		-- Blink interval in milliseconds (100 - 5000).
		-- Full cycle = 2x this value (on for interval, off for interval).
		blink_interval = 500,

		-- Lock the cursor to your configured shape and colour. Programs cannot change it.
		-- When "true", programs cannot change cursor shape or colour.
		force = "false",
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
		foreground = "#a1d6e5ff",

		-- Default background colour.
		-- The last two hex digits control background transparency (GPU only).
		-- CPU rendering always uses a fully opaque background.
		background = "#00000000",

		-- Editor widget background fill (behind the cell grid).
		-- Transparent lets the window glass effect show through.
		editor_background = "#00000000",

		-- Editor widget outline colour.
		-- Transparent for no visible outline.
		editor_outline = "#00000000",

		-- Cursor colour.
		-- Programs may change this colour while running.
		cursor = "#4e8c93ff",

		-- Selection highlight colour.
		-- Semi-transparent recommended so text remains readable.
		selection = "#00ddee20",

		-- Selection-mode cursor colour.
		-- Shown instead of the normal cursor when selection mode is active.
		selection_cursor = "#00ddeeff",

		-- Black
		black = "#090d12ff",

		-- Red
		red = "#fc704cff",

		-- Green
		green = "#c5f0e9ff",

		-- Yellow
		yellow = "#f3f5c5ff",

		-- Blue
		blue = "#8cc9d9ff",

		-- Magenta
		magenta = "#519299ff",

		-- Cyan
		cyan = "#699daaff",

		-- White
		white = "#ddddddff",

		-- Bright black
		bright_black = "#33535bff",

		-- Bright red
		bright_red = "#fc704cff",

		-- Bright green
		bright_green = "#bafffdff",

		-- Bright yellow
		bright_yellow = "#feffd2ff",

		-- Bright blue
		bright_blue = "#67dfefff",

		-- Bright magenta
		bright_magenta = "#01c2d2ff",

		-- Bright cyan
		bright_cyan = "#00c8d8ff",

		-- Bright white
		bright_white = "#bafffdff",

		-- Status bar full background colour.
		-- Default matches the active tab background (tab.active).
		status_bar = "#090d12ff",

		-- Status bar mode label background colour.
		-- Default matches the active tab indicator colour (tab.indicator).
		status_bar_label_bg = "#112130ff",

		-- Status bar mode label text colour.
		status_bar_label_fg = "#4e8c93ff",

		-- Status bar spinner colour.
		status_bar_spinner = "#00c8d8ff",

		-- Open File mode hint label background colour.
		-- Shown as the badge background behind single- or double-letter hint keys.
		hint_label_bg = "#00ffffff",

		-- Open File mode hint label foreground (text) colour.
		hint_label_fg = "#111111ff",

		-- Terminal scrollbar thumb colour. Semi-transparent recommended.
		scrollbar_thumb = "#2c414480",

		-- Terminal scrollbar track colour. Transparent for no visible track.
		scrollbar_track = "#00000000",
	},

	-- ========================================================================
	-- WINDOW
	-- ========================================================================

	window = {
		-- Window title shown in the title bar and mission control.
		title = "END",

		-- Initial window width in pixels.
		width = 640,

		-- Initial window height in pixels.
		height = 480,

		-- Tint colour for the window background. The last two hex digits
		-- control window transparency (glass mode). Most visible with blur enabled.
		colour = "#090d12bf",

		-- Background blur radius in pixels (0 = no blur).
		-- GPU only. Has no effect with CPU rendering.
		-- macOS: controls blur intensity.
		-- Windows 10: blur is on but intensity is set by the system.
		-- Windows 11: uses the system glass effect. This setting has no effect.
		blur_radius = 32,

		-- Native blur backend selection per platform.
		-- "mac" / "win" keys select the backend used on that platform; the
		-- string value is mapped to a jam::BackgroundBlur::Backend enum.
		-- macOS values: backgroundBlur, visualFXWindowBackground,
		--               glassFXRegular, glassFXClear (last two require macOS 26+).
		-- Windows values: blurBehind, acrylic10, acrylic11, mica
		--                 (acrylic11/mica require Windows 11 22H2+).
		-- Falls back to backgroundBlur (mac) / blurBehind (win) if unrecognised.
		blur_style = {
			mac = "backgroundBlur",
			win = "blurBehind",
		},

		-- Keep window above all other windows.
		always_on_top = "false",

		-- Show native window buttons (close / minimise / maximise).
		buttons = "false",

		-- Force DWM visual effects on Windows 11 virtual machines.
		-- When "true", injects the ForceEffectMode registry key to enable
		-- rounded window corners that DWM normally disables inside VMs.
		-- Only takes effect on Windows 11 running on a software renderer (VM).
		-- Requires elevated privileges (Run as Administrator).
		-- Reload config and restart END to apply.
		-- No effect on macOS, Linux, or physical Windows machines.
		force_dwm = "true",

		-- Persist window size across instances to ~/.config/end/window.state.
		-- When "true", every quit writes the current window size; new instances
		-- (with no session to restore) load it as their initial size.
		-- When "false", the file is neither read nor written; new instances
		-- fall back to window.width and window.height above.
		-- Restored sessions always use their own persisted window size and
		-- ignore window.state regardless of this setting.
		save_size = "true",

		-- Show a confirmation dialog when Ctrl+Q is pressed.
		-- When "true", a Yes/No dialog asks before quitting (or saving the session
		-- in daemon mode).  When "false", Ctrl+Q quits immediately with no prompt.
		confirmation_on_exit = "true",
	},

	-- ========================================================================
	-- TAB BAR
	-- ========================================================================

	tab = {
		-- Tab bar font family.
		family = "Display Mono",

		-- Tab bar font size in points.
		size = 12,

		-- Active tab text colour.
		foreground = "#00c8d8ff",

		-- Inactive tab text colour.
		inactive = "#33535bff",

		-- Tab bar position: "top", "bottom", "left", "right".
		position = "left",

		-- Tab separator line colour.
		line = "#2c4144ff",

		-- Active tab background colour.
		active = "#002b35ff",

		-- Active tab indicator colour.
		indicator = "#01c2d2ff",
	},

	-- ========================================================================
	-- MENU
	-- ========================================================================

	menu = {
		-- Popup menu background opacity (0.0 - 1.0).
		opacity = 0.65,
	},

	-- ========================================================================
	-- OVERLAY
	-- ========================================================================

	overlay = {
		-- Overlay font family (used for status messages).
		family = "Display Mono",

		-- Overlay font size in points.
		size = 14,

		-- Overlay text colour.
		colour = "#4e8c93ff",
	},

	-- ========================================================================
	-- PANE
	-- ========================================================================

	pane = {
		-- Pane divider bar colour.
		bar_colour = "#33535bff",

		-- Pane divider bar colour when dragging or hovering.
		bar_highlight = "#4e8c93ff",
	},

	-- ========================================================================
	-- STATUS BAR
	-- ========================================================================

	status_bar = {
		-- Status bar position: "top" or "bottom".
		position = "bottom",

		-- Status bar font family.
		font_family = "Display Mono",

		-- Status bar font size in points.
		font_size = 12,

		-- Status bar font style.
		font_style = "Bold",
	},

	-- ========================================================================
	-- ACTION LIST
	-- ========================================================================

	action_list = {
		-- Action list position: "top" or "bottom".
		position = "top",

		-- Close the action list after running an action.
		-- When "false", the list stays open after execution.
		close_on_run = "true",

		-- Font family for action name labels.
		name_font_family = "Display",

		-- Action-list action-name font style (Regular, Bold, Book, Medium).
		name_font_style = "Bold",

		-- Font size for action name labels in points (6 - 72).
		name_font_size = 13,

		-- Font family for keyboard shortcut labels. Should be monospace.
		shortcut_font_family = "Display Mono",

		-- Action-list shortcut font style (Regular, Bold).
		shortcut_font_style = "Bold",

		-- Font size for keyboard shortcut labels in points (6 - 72).
		shortcut_font_size = 12,

		-- Space between the action list edge and content, in pixels.
		-- Four values: { top, right, bottom, left }. Valid range: 0 - 200.
		padding = { 10, 10, 10, 10 },

		-- Text colour for action name labels.
		name_colour = "#a1d6e5ff",

		-- Text colour for keyboard shortcut labels.
		shortcut_colour = "#00c8d8ff",

		-- Proportional width of the action list relative to the terminal window (0.1 - 1.0).
		width = 0.3,

		-- Maximum proportional height of the action list relative to the terminal window (0.1 - 1.0).
		-- When all results exceed this height, the list scrolls.
		height = 0.3,

		-- Background colour for the highlighted/selected row.
		-- Leave empty to use the terminal selection colour (colours.selection).
		highlight_colour = "#00ddee20",
	},

	-- ========================================================================
	-- POPUP BORDER
	-- ========================================================================

	popup = {
		-- Popup border colour.
		border_colour = "#4e8c93ff",

		-- Popup border stroke width in pixels (0 = no border).
		border_width = 1,
	},

	-- ========================================================================
	-- SCROLLBAR
	-- ========================================================================

	-- Scrollbar width in pixels. Set to 0 to hide scrollbar.
	scrollbar_width = 8,
}
