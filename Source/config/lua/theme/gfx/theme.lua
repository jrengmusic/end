-- ============================================================================
-- END theme/gfx/theme.lua — visual theme properties
-- https://github.com/jrengmusic/end
-- ============================================================================
--
-- This file defines the visual appearance for the END default "gfx" theme.
-- Each section maps 1:1 to a component scope.
--
-- Colour format: 0xAARRGGBB hex integer (Alpha|Red|Green|Blue).
--
-- ============================================================================
-- COLOUR PALETTE
-- ============================================================================

local colours = {
	-- Alpha variant: a is 0..1, returns 0xAARRGGBB.
	withAlpha = function(c, a)
		return (math.floor(a * 255 + 0.5) << 24) | (c & 0xffffff)
	end,

	narwhalGrey = 0xff090d12,
	preciousPersimmon = 0xfffc704c,
	gentleCold = 0xffc5f0e9,
	silkStar = 0xfff3f5c5,
	skyFall = 0xff8cc9d9,
	lagoon = 0xff519299,
	tranquiliTeal = 0xff699daa,
	steam = 0xffdddddd,
	mediterranea = 0xff33535b,
	paleSky = 0xffbafffd,
	mattWhite = 0xfffeffd2,
	poseidonJr_ = 0xff67dfef,
	caribbeanBlue = 0xff01c2d2,
	blueBikini = 0xff00c8d8,
	crystal = 0xffa1d6e5,
	paradiso = 0xff4e8c93,
	coldLightOfDay = 0xff00ddee,
	littleMermaid = 0xff2c4144,
	continentalWaters = 0xff8fc6d0,
	cavoloNero = 0xff6b9099,
	aztec = 0xff273233,
	void = 0xff001a20,
	vulcan = 0xff2d3b40,
	trappedDarkness = 0xff112130,
	aqua = 0xff00ffff,
	dreamlessSleep = 0xff111111,
	transparent = 0x00000000,
}

return {
	-- ========================================================================
	-- WINDOW
	-- ========================================================================

	window = {
		-- Window background tint.
		-- Alpha controls window transparency (glass mode).
		background = colours.withAlpha(colours.narwhalGrey, 0.75),

		-- Background blur radius in pixels (0 = no blur). GPU only.
		blur_radius = 32,

		-- Window visual effect style per platform.
		-- macOS: "backgroundBlur", "visualFXWindowBackground", "glassFXRegular", "glassFXClear"
		-- Windows: "blurBehind", "acrylic10", "acrylic11", "mica"
		style = {
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
		black = colours.narwhalGrey,
		red = colours.preciousPersimmon,
		green = colours.gentleCold,
		yellow = colours.silkStar,
		blue = colours.skyFall,
		magenta = colours.lagoon,
		cyan = colours.tranquiliTeal,
		white = colours.steam,
		bright_black = colours.mediterranea,
		bright_red = colours.preciousPersimmon,
		bright_green = colours.paleSky,
		bright_yellow = colours.mattWhite,
		bright_blue = colours.poseidonJr_,
		bright_magenta = colours.caribbeanBlue,
		bright_cyan = colours.blueBikini,
		bright_white = colours.paleSky,
	},

	-- ========================================================================
	-- CODE — terminal editing area
	-- ========================================================================

	code = {
		-- Default text foreground (jam::CodeView::textColourId).
		text = colours.crystal,

		-- Default background (jam::CodeView::backgroundColourId).
		-- Transparent lets the window glass effect show through.
		background = colours.transparent,

		-- Caret (jam::CaretComponent::caretColourId).
		caret = colours.paradiso,

		-- Selection highlight (juce::TextEditor::highlightColourId).
		-- Semi-transparent recommended so text remains readable.
		highlight = colours.withAlpha(colours.coldLightOfDay, 0.125),

		-- Selection-mode cursor (selectionCursorColourId).
		-- Shown instead of the normal cursor when selection mode is active.
		selection_cursor = colours.coldLightOfDay,

		-- Editor widget background fill (juce::TextEditor::backgroundColourId).
		-- Transparent lets the window glass effect show through.
		editor_background = colours.transparent,

		-- Editor widget outline (juce::TextEditor::outlineColourId).
		editor_outline = colours.transparent,

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
		thumb = colours.withAlpha(colours.littleMermaid, 0.5),

		-- Scrollbar track (juce::ScrollBar::trackColourId).
		-- Transparent for no visible track.
		track = colours.transparent,

		-- Scrollbar width in pixels. Set to 0 to hide scrollbar.
		width = 8,
	},

	-- ========================================================================
	-- TAB BAR
	-- ========================================================================

	tab = {
		-- Bar strip background (jam::button::Bar::backgroundColourId).
		background = colours.continentalWaters,

		-- Sliding selection highlight (jam::button::Bar::highlightColourId).
		highlight = colours.cavoloNero,

		-- SVG group outline (jam::button::Bar::outlineColourId).
		outline = colours.aztec,

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

		-- Always convert tab label to upper-case.
		uppercase = true,
	},

	-- ========================================================================
	-- TAB BUTTON
	-- ========================================================================

	button = {
		-- Inactive tab fill (juce::TextButton::buttonColourId).
		button = colours.void,

		-- Active tab fill (juce::TextButton::buttonOnColourId).
		button_on = colours.vulcan,

		-- Inactive tab text (juce::TextButton::textColourOffId).
		text_off = colours.mediterranea,

		-- Active tab text (juce::TextButton::textColourOnId).
		text_on = colours.blueBikini,
	},

	-- ========================================================================
	-- OVERLAY
	-- ========================================================================

	overlay = {
		-- Overlay background (juce::Label::backgroundColourId).
		background = colours.withAlpha(colours.narwhalGrey, 0.75),

		-- Overlay text (juce::Label::textColourId).
		text = colours.paradiso,

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
		resize_bar = colours.littleMermaid,

		-- Pane divider bar when dragging or hovering (paneBarHighlightColourId).
		resize_bar_highlight = colours.cavoloNero,

		-- Pane divider bar thickness in pixels.
		resize_bar_thickness = 8,
	},

	-- ========================================================================
	-- STATUS BAR
	-- ========================================================================

	status_bar = {
		-- Status bar background (statusBarBackgroundColourId).
		background = colours.narwhalGrey,

		-- Mode label background (statusBarLabelBackgroundColourId).
		label_background = colours.trappedDarkness,

		-- Mode label text (statusBarLabelTextColourId).
		label_text = colours.paradiso,

		-- Spinner (statusBarSpinnerColourId).
		spinner = colours.blueBikini,

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
		background = colours.aqua,

		-- Hint label text (hintLabelFgColourId).
		text = colours.dreamlessSleep,
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
		name_colour = colours.crystal,

		-- Text colour for keyboard shortcut labels.
		shortcut_colour = colours.blueBikini,

		-- Proportional width of the action list relative to the terminal window (0.1 - 1.0).
		width = 0.3,

		-- Maximum proportional height of the action list relative to the terminal window (0.1 - 1.0).
		-- When all results exceed this height, the list scrolls.
		height = 0.3,

		-- Background colour for the highlighted/selected row.
		highlight_colour = colours.withAlpha(colours.coldLightOfDay, 0.125),
	},
}
