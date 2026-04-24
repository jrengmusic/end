-- ============================================================================
--
--  END Action Configuration
--  https://github.com/jrengmusic/end
--
-- ============================================================================
--
--  This file is the single source of truth for all keybindings, popup
--  terminals, and custom user actions. END loads it from:
--
--      ~/.config/end/action.lua
--
--  On first launch, this default is written automatically.
--  Changes are hot-reloaded on save when auto_reload is enabled in end.lua.
--  Manual reload: Cmd+R (macOS) / Ctrl+R (Linux/Windows).
--
--  Key binding format: "modifier+key" (e.g. "cmd+c", "ctrl+shift+t").
--    Modifiers: cmd, ctrl, alt, shift
--    Special keys: return, escape, space, tab, backspace, delete,
--                  pageup, pagedown, home, end, f1-f12
--
--  Direct bindings fire immediately (e.g. "cmd+c" for copy).
--  Modal bindings require the prefix key first, then the action key
--  within the timeout window (e.g. press ` then \  to split).
--
-- ============================================================================


-- ============================================================================
-- KEY BINDINGS
-- ============================================================================
--
-- Maps action names to keyboard shortcuts.
--
-- Direct shortcuts use "modifier+key" format (e.g. "cmd+c").
-- Modal shortcuts are single characters pressed AFTER the prefix key.
-- A key containing a "+" is always treated as a direct (global) binding.
-- A single character is always treated as a modal binding.
--

keys = {

	-- ---- Prefix key (modal system) ----

	-- The prefix key activates modal mode. Press it, then press a modal
	-- action key within the timeout. Set to "" to disable modal mode entirely.
	prefix = "`",

	-- How long to wait (ms) for a modal key after pressing the prefix key.
	prefix_timeout = 1000,

	-- ---- Clipboard ----

	-- Copy selection to clipboard.
	copy = "%%copy%%",

	-- Paste from clipboard.
	paste = "%%paste%%",

	-- ---- Window ----

	-- Quit application.
	quit = "%%quit%%",

	-- Close active pane, then tab, then window.
	close_tab = "%%close_tab%%",

	-- Reload all configuration files (end.lua + action.lua).
	reload = "%%reload%%",

	-- Increase font size.
	zoom_in = "%%zoom_in%%",

	-- Decrease font size.
	zoom_out = "%%zoom_out%%",

	-- Reset font size to configured default.
	zoom_reset = "%%zoom_reset%%",

	-- Open a new window.
	new_window = "%%new_window%%",

	-- ---- Tabs ----

	-- Open a new tab.
	new_tab = "%%new_tab%%",

	-- Switch to previous tab.
	prev_tab = "%%prev_tab%%",

	-- Switch to next tab.
	next_tab = "%%next_tab%%",

	-- ---- Panes (modal) ----

	-- Split pane horizontally (side-by-side columns). Press prefix first.
	split_horizontal = "\\",

	-- Split pane vertically (stacked rows). Press prefix first.
	split_vertical = "-",

	-- Focus pane to the left. Press prefix first.
	pane_left = "h",

	-- Focus pane below. Press prefix first.
	pane_down = "j",

	-- Focus pane above. Press prefix first.
	pane_up = "k",

	-- Focus pane to the right. Press prefix first.
	pane_right = "l",

	-- ---- Misc ----

	-- Insert a literal newline (LF) instead of carriage return.
	newline = "shift+return",

	-- Open the action list (command palette). Press prefix first.
	action_list = "?",

	-- Enter text selection mode (vim-like). Press prefix first.
	enter_selection = "[",

	-- Enter open-file mode (hyperlink hint labels). Press prefix first.
	enter_open_file = "o",

	-- Cycle to next page of open-file hints.
	open_file_next_page = "space",

	-- ---- Selection mode ----
	--
	-- These keys are active only while in selection mode (enter_selection).
	-- They follow vim conventions by default.
	--

	-- Move cursor up.
	selection_up = "k",

	-- Move cursor down.
	selection_down = "j",

	-- Move cursor left.
	selection_left = "h",

	-- Move cursor right.
	selection_right = "l",

	-- Toggle character-wise visual selection.
	selection_visual = "v",

	-- Toggle line-wise visual selection.
	selection_visual_line = "shift+v",

	-- Toggle block visual selection (real Ctrl, not Cmd on macOS).
	selection_visual_block = "ctrl+v",

	-- Yank (copy) the current selection and exit selection mode.
	selection_copy = "y",

	-- Jump to top of buffer (press twice: gg).
	selection_top = "g",

	-- Jump to bottom of buffer.
	selection_bottom = "shift+g",

	-- Jump to start of current line.
	selection_line_start = "0",

	-- Jump to end of current line.
	selection_line_end = "$",

	-- Exit selection mode.
	selection_exit = "escape",
}


-- ============================================================================
-- POPUP TERMINALS
-- ============================================================================
--
-- Modal popup terminals. Each entry spawns a floating terminal running a
-- command on top of the main terminal. The popup blocks input until the
-- process exits (quit the TUI, Ctrl+C a script, etc.).
--
-- Fields:
--   command  (string, required)  Shell command or executable to run.
--   args     (string, optional)  Arguments passed to the command.
--   cwd      (string, optional)  Working directory. Empty = inherit cwd.
--   cols     (number, optional)  Width in columns. Default: popup.cols in end.lua.
--   rows     (number, optional)  Height in rows. Default: popup.rows in end.lua.
--   modal    (string, optional)  Key pressed after the prefix key (e.g. "t").
--   global   (string, optional)  Direct shortcut, no prefix needed (e.g. "cmd+g").
--
-- At least one of modal or global is required.
-- Both can coexist on the same entry.
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
}


-- ============================================================================
-- CUSTOM ACTIONS
-- ============================================================================
--
-- Define your own actions composed from END's display API.
-- Each action gets a name, description, keybinding, and an execute function.
--
-- Available display API:
--
--   display.split_horizontal()              Split into side-by-side columns (50/50).
--   display.split_vertical()                Split into stacked rows (50/50).
--   display.split_with_ratio(dir, ratio)    Split at a custom ratio (0.0-1.0).
--                                           dir: "vertical" for columns,
--                                                "horizontal" for rows.
--   display.new_tab()                       Open a new tab.
--   display.close_tab()                     Close the active pane/tab.
--   display.next_tab()                      Switch to the next tab.
--   display.prev_tab()                      Switch to the previous tab.
--   display.focus_pane(dx, dy)              Focus a neighbouring pane.
--                                           dx: -1 left, +1 right, 0 none.
--                                           dy: -1 up, +1 down, 0 none.
--   display.close_pane()                    Close the active pane.
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
			display.split_with_ratio("vertical", 0.333)
			display.split_with_ratio("vertical", 0.5)
		end,
	},

	split_thirds_v = {
		name = "Split Vertical Thirds",
		description = "Split into three equal vertical panes",
		modal = "4",
		execute = function()
			display.split_with_ratio("horizontal", 0.333)
			display.split_with_ratio("horizontal", 0.5)
		end,
	},
}
