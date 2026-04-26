-- ============================================================================
-- END actions.lua — custom user actions
-- https://github.com/jrengmusic/end
-- ============================================================================
--
-- This file is auto-generated with default values on first launch.
-- Edit any value below to define your own custom actions.
-- Reload with Cmd+R (no restart needed).
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

return {
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
}
