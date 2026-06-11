-- END-GENERATED v1
-- ============================================================================
-- END graphics.lua — SVG asset paths
-- https://github.com/jrengmusic/end
-- ============================================================================
--
-- This file is auto-generated with default values on first launch.
-- Edit any value below to customise your tab bar appearance.
-- Invalid or missing values fall back to defaults silently.
-- Reload with Cmd+R (no restart needed).
--
-- SVG files are loaded from the path directory. Each key specifies a
-- filename relative to that directory. Empty = not loaded.
--
-- ============================================================================

return {

	-- Base directory for SVG assets, relative to ~/.config/end/graphics/.
	-- Can also be an absolute path.
	path = "gfx",

	-- Tab bar background shape.
	tab_bar = "tab_bar.svg",

	-- Sliding tab indicator shape (moves to the active tab).
	tab_indicator = "tab_indicator.svg",

	-- Tab button state slots. Present keys must be the first N states in order:
	-- normal, over, down, disabled, normalOn, overOn, downOn, disabledOn.
	-- Valid counts: 1, 3, 4, 6, 8. Default ships only normal.
	tab_button = {
		normal = "tab_button_normal.svg",
	},
}
