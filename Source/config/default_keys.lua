-- END-GENERATED v1
-- ============================================================================
-- END keys.lua — key bindings
-- https://github.com/jrengmusic/end
-- ============================================================================
--
-- This file is auto-generated with default values on first launch.
-- Edit any value below to customise your key bindings.
-- Invalid or missing values fall back to defaults silently.
-- Reload with Cmd+R (no restart needed).
--
-- Key binding format: "modifier+key" (e.g. "cmd+c", "ctrl+shift+t").
--   Modifiers: cmd, ctrl, alt, shift
--   Special keys: return, escape, space, tab, backspace, delete,
--                 pageup, pagedown, home, end, f1-f12
--
-- Direct bindings fire immediately (e.g. "cmd+c" for copy).
-- Modal bindings require the prefix key first, then the action key
-- within the timeout window (e.g. press ` then \  to split).
--
-- ============================================================================

return {

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

    -- Rename the active tab. Press prefix first.
    rename_tab = "shift+t",

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
