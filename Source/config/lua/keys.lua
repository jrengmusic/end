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
    copy = "cmd+c",

    -- Paste from clipboard.
    paste = "cmd+v",

    -- ---- Window ----

    -- Quit application.
    quit = "cmd+q",

    -- Close active pane, then tab, then window.
    close_pane = "cmd+w",

    -- Reload all configuration files (config.lua).
    reload = "cmd+r",

    -- Increase font size.
    zoom_in = "cmd+=",

    -- Decrease font size.
    zoom_out = "cmd+-",

    -- Reset font size to configured default.
    zoom_reset = "cmd+0",

    -- Open a new window.
    new_window = "cmd+n",

    -- Reduce the focused pane's width by pane_step (display.lua).
    reduce_pane_width = "cmd+alt+h",

    -- Reduce the focused pane's height by pane_step (display.lua).
    reduce_pane_height = "cmd+alt+j",

    -- Expand the focused pane's width by pane_step (display.lua).
    expand_pane_width = "cmd+alt+l",

    -- Expand the focused pane's height by pane_step (display.lua).
    expand_pane_height = "cmd+alt+k",

    -- ---- Tabs ----

    -- Open a new tab.
    new_tab = "cmd+t",

    -- Switch to previous tab.
    prev_tab = "cmd+[",

    -- Switch to next tab.
    next_tab = "cmd+]",

    -- Rename the active tab. Press prefix first.
    rename_tab = "shift+t",

    -- ---- Panes (modal) ----

    -- Split pane vertically (side-by-side columns). Press prefix first.
    split_vertical = "\\",

    -- Split pane horizontally (stacked rows). Press prefix first.
    split_horizontal = "-",

    -- Focus pane to the left. Press prefix first.
    pane_left = "h",

    -- Focus pane below. Press prefix first.
    pane_down = "j",

    -- Focus pane above. Press prefix first.
    pane_up = "k",

    -- Focus pane to the right. Press prefix first.
    pane_right = "l",

    -- Absorb the pane to the left into the focused pane.
    join_left = "ctrl+h",

    -- Absorb the pane below into the focused pane.
    join_down = "ctrl+j",

    -- Absorb the pane above into the focused pane.
    join_up = "ctrl+k",

    -- Absorb the pane to the right into the focused pane.
    join_right = "ctrl+l",

    -- Swap the focused pane with the pane to the left. Press prefix first.
    swap_left = "shift+h",

    -- Swap the focused pane with the pane below. Press prefix first.
    swap_down = "shift+j",

    -- Swap the focused pane with the pane above. Press prefix first.
    swap_up = "shift+k",

    -- Swap the focused pane with the pane to the right. Press prefix first.
    swap_right = "shift+l",

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

    -- ---- Whelmed document navigation ----

    -- Scroll document down one step.
    scroll_down = "j",

    -- Scroll document up one step.
    scroll_up = "k",

    -- Jump to top of document (press twice: gg).
    scroll_top = "gg",

    -- Jump to bottom of document.
    scroll_bottom = "G",
}
