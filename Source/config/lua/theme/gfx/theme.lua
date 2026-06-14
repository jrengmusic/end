-- ============================================================================
-- END theme/gfx/theme.lua — visual theme properties
-- https://github.com/jrengmusic/end
-- ============================================================================
--
-- This file defines the visual appearance for the "gfx" theme.
-- Each section maps 1:1 to a component scope. Colour properties map
-- directly to JUCE/jam colourIds.
--
-- Colour format: 0xAARRGGBB hex integer (JUCE native ARGB format).
--
-- ============================================================================

return {
	-- ========================================================================
	-- WINDOW
	-- ========================================================================

	window = {
		-- Window background tint.
		-- Alpha controls window transparency (glass mode).
		background = 0xbf090d12,

		-- Background blur radius in pixels (0 = no blur). GPU only.
		blur_radius = 32,

		-- Window visual effect style per platform.
		-- macOS: "backgroundBlur", "visualFXWindowBackground", "glassFXRegular", "glassFXClear"
		-- Windows: "blurBehind", "acrylic10", "acrylic11", "mica"
		window_fx = {
			mac = "backgroundBlur",
			win = "blurBehind",
		},
	},

	-- ========================================================================
	-- ANSI 16-COLOUR TERMINAL PALETTE
	-- ========================================================================
	--
	-- The 16 standard terminal colours. Programs like ls, git, and vim use these.
	-- The first 8 are normal, the next 8 are bright variants.
	--

	ansi = {
		black = 0xff090d12,
		red = 0xfffc704c,
		green = 0xffc5f0e9,
		yellow = 0xfff3f5c5,
		blue = 0xff8cc9d9,
		magenta = 0xff519299,
		cyan = 0xff699daa,
		white = 0xffdddddd,
		bright_black = 0xff33535b,
		bright_red = 0xfffc704c,
		bright_green = 0xffbafffd,
		bright_yellow = 0xfffeffd2,
		bright_blue = 0xff67dfef,
		bright_magenta = 0xff01c2d2,
		bright_cyan = 0xff00c8d8,
		bright_white = 0xffbafffd,
	},

	-- ========================================================================
	-- CODE — terminal editing area
	-- ========================================================================

	code = {
		-- Default text foreground (jam::CodeView::textColourId).
		text = 0xffa1d6e5,

		-- Default background (jam::CodeView::backgroundColourId).
		-- Transparent lets the window glass effect show through.
		background = 0x00000000,

		-- Caret (jam::CaretComponent::caretColourId).
		caret = 0xff4e8c93,

		-- Selection highlight (juce::TextEditor::highlightColourId).
		-- Semi-transparent recommended so text remains readable.
		highlight = 0x2000ddee,

		-- Selection-mode cursor (selectionCursorColourId).
		-- Shown instead of the normal cursor when selection mode is active.
		selection_cursor = 0xff00ddee,

		-- Editor widget background fill (juce::TextEditor::backgroundColourId).
		-- Transparent lets the window glass effect show through.
		editor_background = 0x00000000,

		-- Editor widget outline (juce::TextEditor::outlineColourId).
		editor_outline = 0x00000000,

		-- Space between the window edge and the terminal text, in pixels.
		-- CSS convention: { top, right, bottom, left }.
		padding = { 10, 10, 10, 10 },

		-- Font used for terminal text.
		-- Must be a monospace font installed on the system.
		font_family = "Display Mono",

		-- Font size in points before zoom is applied (1 - 200).
		font_size = 12,

		-- Combine certain character sequences into symbols (e.g. -> becomes an arrow).
		ligatures = true,

		-- Make text appear bolder.
		-- Useful for thin fonts that are hard to read at small sizes.
		embolden = true,

		-- Line height multiplier applied to terminal cell height (0.5 - 3.0).
		-- 1.0 = no adjustment. Values above 1.0 increase spacing, below decrease it.
		line_height = 1,

		-- Cell width multiplier applied to terminal cell width (0.5 - 3.0).
		-- 1.0 = no adjustment. Values above 1.0 widen cells, below narrow them.
		cell_width = 1,

		-- Whether font size follows the Windows desktop scale.
		-- true  — font size scales with the desktop scale (system default behaviour).
		-- false — font stays at its configured point size in physical pixels
		--         regardless of the desktop scale slider (12pt always looks 12pt).
		-- Windows only. No effect on macOS or Linux.
		desktop_scale = false,
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
		blink = true,

		-- Blink interval in milliseconds (100 - 5000).
		-- Full cycle = 2x this value (on for interval, off for interval).
		blink_interval = 500,

		-- Lock the cursor to your configured shape and colour. Programs cannot change it.
		force = false,
	},

	-- ========================================================================
	-- SCROLLBAR
	-- ========================================================================

	scrollbar = {
		-- Scrollbar thumb (juce::ScrollBar::thumbColourId).
		-- Semi-transparent recommended.
		thumb = 0x802c4144,

		-- Scrollbar track (juce::ScrollBar::trackColourId).
		-- Transparent for no visible track.
		track = 0x00000000,

		-- Scrollbar width in pixels. Set to 0 to hide scrollbar.
		width = 8,
	},

	-- ========================================================================
	-- TAB BAR
	-- ========================================================================

	tab = {
		-- Bar strip background (jam::button::Bar::backgroundColourId).
		background = 0xFF8FC6D0,

		-- Sliding selection highlight (jam::button::Bar::highlightColourId).
		highlight = 0xff6b9099,

		-- SVG group outline (jam::button::Bar::outlineColourId).
		outline = 0xff273233,

		-- Tab bar font family.
		font_family = "Display",

		-- Tab bar font size in points.
		font_size = 12,

		-- Extra spacing between tab label characters, as a fraction of the
		-- font size (0.0 = font default).
		kerning_factor = 0.075,

		-- Tab bar height as a multiple of the tab font height.
		depth = 3.0,

		-- Horizontal space between tab text and tab edge, in pixels (per side).
		text_padding = 8,

		-- Component padding: space between bar edges and tab content.
		-- CSS convention: { top, right, bottom, left }.
		padding = { 4, 8, 4, 8 },

		-- Tab bar orientation: "top", "bottom", "left", "right".
		orientation = "top",

		-- Always convert tab label to upper-case.
		uppercase = true,
	},

	-- ========================================================================
	-- TAB BUTTON
	-- ========================================================================

	button = {
		-- Inactive tab fill (juce::TextButton::buttonColourId).
		button = 0xff001a20,

		-- Active tab fill (juce::TextButton::buttonOnColourId).
		button_on = 0xff2d3b40,

		-- Inactive tab text (juce::TextButton::textColourOffId).
		text_off = 0xff33535b,

		-- Active tab text (juce::TextButton::textColourOnId).
		text_on = 0xff00c8d8,
	},

	-- ========================================================================
	-- OVERLAY
	-- ========================================================================

	overlay = {
		-- Overlay background (juce::Label::backgroundColourId).
		background = 0xbf090d12,

		-- Overlay text (juce::Label::textColourId).
		text = 0xff4e8c93,

		-- Overlay font family (used for status messages).
		font_family = "Display Mono",

		-- Overlay font size in points.
		font_size = 14,
	},

	-- ========================================================================
	-- PANE
	-- ========================================================================

	pane = {
		-- Pane divider bar (paneBarColourId).
		bar_colour = 0xff33535b,

		-- Pane divider bar when dragging or hovering (paneBarHighlightColourId).
		bar_highlight = 0xff4e8c93,
	},

	-- ========================================================================
	-- STATUS BAR
	-- ========================================================================

	status_bar = {
		-- Status bar background (statusBarBackgroundColourId).
		background = 0xff090d12,

		-- Mode label background (statusBarLabelBackgroundColourId).
		label_background = 0xff112130,

		-- Mode label text (statusBarLabelTextColourId).
		label_text = 0xff4e8c93,

		-- Spinner (statusBarSpinnerColourId).
		spinner = 0xff00c8d8,

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
	-- HINT
	-- ========================================================================

	hint = {
		-- Hint label background (hintLabelBgColourId).
		background = 0xff00ffff,

		-- Hint label text (hintLabelFgColourId).
		text = 0xff111111,
	},

	-- ========================================================================
	-- MENU
	-- ========================================================================

	menu = {
		-- Popup menu background opacity (0.0 - 1.0).
		opacity = 0.65,
	},

	-- ========================================================================
	-- ACTION LIST
	-- ========================================================================

	action_list = {
		-- Action list position: "top" or "bottom".
		position = "top",

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
		-- CSS convention: { top, right, bottom, left }.
		padding = { 10, 10, 10, 10 },

		-- Text colour for action name labels.
		name_colour = 0xffa1d6e5,

		-- Text colour for keyboard shortcut labels.
		shortcut_colour = 0xff00c8d8,

		-- Proportional width of the action list relative to the terminal window (0.1 - 1.0).
		width = 0.3,

		-- Maximum proportional height of the action list relative to the terminal window (0.1 - 1.0).
		-- When all results exceed this height, the list scrolls.
		height = 0.3,

		-- Background colour for the highlighted/selected row.
		highlight_colour = 0x2000ddee,
	},

	-- ========================================================================
	-- GRAPHICS — SVG asset filename mappings
	-- ========================================================================
	--
	-- SVG files are loaded from themes/<name>/graphics/.
	-- Each key specifies a filename relative to that directory.
	-- Empty = not loaded.
	--

	graphics = {
		-- Tab bar background shape.
		tab_bar = "tab_bar.svg",

		-- Sliding tab highlight shape (moves to the active tab).
		tab_highlight = "tab_highlight.svg",

		-- Tab button state slots. Eight states, any subset may be authored:
		-- normal, over, down, disabled, normalOn, overOn, downOn, disabledOn.
		-- Unset states are not painted. Default ships only normalOn.
		tab_button = {
			normalOn = "tab_button_normalOn.svg",
		},
	},
}
