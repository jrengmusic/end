--
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
--	                Ephemeral Nexus Display  v0.0.1
-- ============================================================================
-- Application Configuration
-- https://github.com/jrengmusic/end
-- ============================================================================
--
-- This file is auto-generated with default values on first launch.
-- Edit any value below to customise your terminal.
-- Invalid or missing values fall back to defaults silently.
-- Reload with Cmd+R (no restart needed).
--
-- ============================================================================

return {

	-- ==========================================================================
	-- APP
	-- ==========================================================================

	-- Active theme directory name.
	-- Themes live in ~/.config/end/themes/<name>/theme.lua.
	-- Changing this value reloads all visual properties (colours, fonts,
	-- metrics, SVGs) from the named theme directory.
	theme = "gfx",

	-- Initial window size in pixels {width, height}.
	size = { 640, 480 },

	-- Keep window above all other windows.
	always_on_top = false,

	-- Show native title bar buttons (close / minimise / maximise).
	-- macOS: hides/shows traffic-light buttons and the title bar together.
	-- Windows: toggles the native title bar. Close/min/max buttons are
	-- fixed at construction and cannot be added/removed at runtime.
	title_bar_buttons = false,

	-- Tab bar position: "top", "bottom", "left", "right".
	tab_orientation = "top",

	-- Persist window size across instances to ~/.config/end/window.state.
	-- When true, every quit writes the current window size; new instances
	-- (with no session to restore) load it as their initial size.
	-- When false, the file is neither read nor written; new instances
	-- fall back to the size above.
	-- Restored sessions always use their own persisted window size
	-- regardless of this setting.
	save_window_state = true,

	-- Message shown briefly after a successful config reload (Cmd+R).
	success_message = "RELOAD",

	-- Show a confirmation dialog when Ctrl+Q / Cmd+Q is pressed.
	-- When true, a Yes/No dialog asks before quitting (or saving the
	-- session in daemon mode). When false, quit is immediate.
	confirmation_on_exit = true,

	-- Force DWM visual effects on Windows 11 virtual machines.
	-- Injects the ForceEffectMode registry key to enable rounded window
	-- corners that DWM normally disables inside VMs.
	-- Only takes effect on Windows 11 running on a software renderer (VM).
	-- Requires elevated privileges (Run as Administrator).
	-- Reload config and restart END to apply.
	-- No effect on macOS, Linux, or physical Windows machines.
	force_dwm = true,

	-- Enable GPU-accelerated rendering.
	-- When true, use GPU if available with CPU fallback.
	-- When false, force CPU rendering (no blur, no transparency).
	gpu = true,

	-- Graphics configuration.
	-- Shader/graphics projects live in ~/.config/end/shaders/<name>/.
	-- Files: Common, Image, BufferA, BufferB, BufferC, BufferD (Shadertoy convention).
	graphics = {
		-- Background shader project directory name. Empty string disables.
		background = "",
		-- Background shader opacity (0.0 = transparent, 1.0 = opaque).
		background_opacity = 0.5,
		-- Post-processing shader project directory name. Empty string disables.
		post_processing = "",
		-- Shader frame rate (1-120). Controls how many times per second
		-- shader passes execute. Lower values reduce GPU load.
		frame_rate = 30,
		-- Resolution scale (0.1-1.0). Shader passes render at this fraction
		-- of the screen resolution, then upscale to full size.
		resolution_scale = 0.5,
		-- Texture filter mode for shader upscaling: "linear" (bilinear) or "nearest" (pixel-sharp).
		filter = "linear",
	},

	-- ==========================================================================
	-- NEXUS DAEMON
	-- ==========================================================================
	--
	-- Enable the Nexus background daemon.
	-- When true, sessions survive window close. Relaunch reconnects.
	-- When false, sessions die with the window (no daemon).
	--

	daemon = false,

	-- ==========================================================================
	-- AUTO RELOAD
	-- ==========================================================================
	--
	-- Automatically reload configuration when files change.
	-- When true, END watches ~/.config/end/ for changes and reloads
	-- config.lua on save. No restart or Cmd+R needed.
	-- When false, manual reload only (Cmd+R).
	--

	auto_reload = true,

	-- ==========================================================================
	-- SHELL
	-- ==========================================================================

	shell = {
		-- Shell program name or absolute path.
		program = "zsh",

		-- Arguments passed to the shell program.
		args = "",

		-- Enable automatic shell integration.
		-- When true, END creates shell hook scripts in ~/.config/end/
		-- and injects them on shell startup. This enables:
		--   - Clickable file links in command output
		--   - Output block detection for the Open File feature
		-- Supported shells: zsh, bash, fish.
		-- Set to false to disable and remove integration scripts.
		integration = true,
	},

	-- ==========================================================================
	-- TERMINAL
	-- ==========================================================================

	terminal = {
		-- Maximum number of lines you can scroll back through (100 - 1000000).
		scrollback_lines = 10000,

		-- Lines scrolled per mouse wheel tick and per Shift+PgUp/PgDn step (1 - 100).
		scroll_step = 5,

		-- Separator for multiple dropped file paths.
		-- "space" joins paths with spaces (shell convention).
		-- "newline" joins paths with newlines.
		drop_multifiles = "space",

		-- Wrap dropped file paths in quotes so spaces and special characters work correctly.
		-- true: paths with special characters are quoted for the active shell.
		-- false: paths are pasted raw (for TUI apps that handle paths directly).
		drop_quoted = true,
	},

	-- ==========================================================================
	-- HYPERLINKS
	-- ==========================================================================

	hyperlinks = {
		-- Editor command for opening files from hyperlinks and Open File mode.
		-- The command receives the file path as its first argument.
		-- Example: "nvim", "vim", "nano", "/usr/local/bin/hx"
		editor = "nvim",

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

	-- ==========================================================================
	-- IMAGE
	-- ==========================================================================

	image = {
		-- Maximum RGBA bytes retained in the inline image atlas before
		-- eviction, in bytes. Range: 1 MiB - 256 MiB.
		-- atlas_budget = 33554432,

		-- Maximum image atlas dimension in pixels. Images exceeding this
		-- in either dimension are downscaled to fit. Range: 1024 - 8192.
		atlas_dimension = 4096,

		-- Native preview panel width in cell columns. Range: 10 - 200.
		-- These settings apply to END's own preview surface (hyperlink click,
		-- file open). CLI/TUI tools (fzf, yazi) manage their own layout.
		cols = 40,

		-- Native preview panel height in cell rows. Range: 5 - 100.
		rows = 20,

		-- Padding inside the preview panel, in pixels. Range: 0 - 64.
		padding = 10,

		-- Draw a native border around the preview region.
		border = true,
	},

	-- ==========================================================================
	-- ACTIONS
	-- ==========================================================================
	--
	-- Define your own actions composed from END's api.
	-- Each action gets a name, description, keybinding, and an execute function.
	--
	-- Available api:
	--
	--   api.split_horizontal()              Split into side-by-side columns (50/50).
	--   api.split_vertical()                Split into stacked rows (50/50).
	--   api.split_with_ratio(dir, ratio)    Split at a custom ratio (0.0-1.0).
	--                                       dir: "vertical" for columns,
	--                                            "horizontal" for rows.
	--   api.new_tab()                       Open a new tab.
	--   api.close_tab()                     Close the active pane/tab.
	--   api.next_tab()                      Switch to the next tab.
	--   api.prev_tab()                      Switch to the previous tab.
	--   api.focus_pane(dx, dy)              Focus a neighbouring pane.
	--                                       dx: -1 left, +1 right, 0 none.
	--                                       dy: -1 up, +1 down, 0 none.
	--   api.close_pane()                    Close the active pane.
	--
	-- Action fields:
	--   name         (string)    Display name shown in the action list.
	--   description  (string)    One-line description for the command palette.
	--   modal        (string)    Modal key: pressed after the prefix key (e.g. "3").
	--   global       (string)    Direct shortcut (e.g. "cmd+shift+3").
	--   execute      (function)  The function to run when the action is triggered.
	--
	-- Provide modal, global, or both.
	--
	-- ---- Examples (working out of the box) ----

	actions = {
		split_thirds_h = {
			name = "Split Horizontal Thirds",
			description = "Split into three equal horizontal panes",
			modal = "3",
			execute = function()
				api.split_with_ratio("vertical", 0.333)
				api.split_with_ratio("vertical", 0.5)
			end,
		},

		split_thirds_v = {
			name = "Split Vertical Thirds",
			description = "Split into three equal vertical panes",
			modal = "4",
			execute = function()
				api.split_with_ratio("horizontal", 0.333)
				api.split_with_ratio("horizontal", 0.5)
			end,
		},
	},

}
