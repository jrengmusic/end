-- ============================================================================
-- END theme/gfx/display.lua — visual theme properties
-- https://github.com/jrengmusic/end
-- ============================================================================
--
-- This file defines the visual appearance for the "gfx" theme.
-- Each section maps 1:1 to a component's colour IDs, fonts, and metrics.
--
-- Colour format: 0xAARRGGBB hex integer (JUCE native ARGB format).
--
-- ============================================================================

return {
	window = {
		-- Window background tint colour (juce::ResizableWindow::backgroundColourId).
		-- Alpha controls window transparency (glass mode).
		background_colour = 0xbf090d12,

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
}
