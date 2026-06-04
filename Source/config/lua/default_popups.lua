-- ============================================================================
-- END popups.lua — popup terminal definitions
-- https://github.com/jrengmusic/end
-- ============================================================================
--
-- This file is auto-generated with default values on first launch.
-- Edit any value below to customise your popup terminals.
-- Invalid or missing values fall back to defaults silently.
-- Reload with Cmd+R (no restart needed).
--
-- Modal popup terminals. Each entry spawns a floating terminal running a
-- command on top of the main terminal. The popup blocks input until the
-- process exits (quit the TUI, Ctrl+C a script, etc.).
--
-- Fields:
--   command  (string, required)  Shell command or executable to run.
--   args     (string, optional)  Arguments passed to the command.
--   cwd      (string, optional)  Working directory. Empty = inherit cwd.
--   cols     (number, optional)  Width in columns. Default: defaults.cols below.
--   rows     (number, optional)  Height in rows. Default: defaults.rows below.
--   modal    (string, optional)  Key pressed after the prefix key (e.g. "t").
--   global   (string, optional)  Direct shortcut, no prefix needed (e.g. "cmd+g").
--
-- At least one of modal or global is required.
-- Both can coexist on the same entry.
--
-- ============================================================================

return {

    -- ========================================================================
    -- POPUP DEFAULTS
    -- ========================================================================

    defaults = {
        -- Default popup width in columns.
        -- Individual popup entries can override this.
        cols = %%popup_cols%%,

        -- Default popup height in rows.
        -- Individual popup entries can override this.
        rows = %%popup_rows%%,

        -- Default popup position: "center".
        position = "%%popup_position%%",
    },

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
