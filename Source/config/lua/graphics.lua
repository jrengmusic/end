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

	-- Sliding tab highlight shape (moves to the active tab).
	tab_highlight = "tab_highlight.svg",

	-- Tab button state slots. Eight states, any subset may be authored:
	-- normal, over, down, disabled, normalOn, overOn, downOn, disabledOn.
	-- Unset states are not painted. Default ships only normalOn.
	tab_button = {
		normalOn = "tab_button_normalOn.svg",
	},
}
