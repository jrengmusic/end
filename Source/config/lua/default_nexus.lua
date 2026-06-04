-- ============================================================================
-- END nexus.lua — shell, terminal, and daemon settings
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

    -- ========================================================================
    -- GPU
    -- ========================================================================
    --
    -- Rendering backend selection. Hot-reloadable (Cmd+R).
    --
    -- "auto"  — Use GPU if available, CPU fallback. (default)
    -- "true"  — Force GPU rendering. Falls back to CPU if unavailable.
    -- "false" — Force CPU rendering. No GPU used.
    --

    gpu = "%%gpu%%",

    -- ========================================================================
    -- NEXUS
    -- ========================================================================
    --
    -- Enable the Nexus background daemon.
    -- When "true", sessions survive window close. Relaunch reconnects.
    -- When "false", sessions die with the window (no daemon).
    --

    daemon = "%%daemon%%",

    -- ========================================================================
    -- AUTO RELOAD
    -- ========================================================================
    --
    -- Automatically reload configuration when files change.
    -- When "true", END watches ~/.config/end/ for changes and reloads
    -- end.lua and action.lua on save. No restart or Cmd+R needed.
    -- When "false", manual reload only (Cmd+R).
    --

    auto_reload = "%%auto_reload%%",

    -- ========================================================================
    -- SHELL
    -- ========================================================================

    shell = {
        -- Shell program name or absolute path.
        program = "%%shell_program%%",

        -- Arguments passed to the shell program.
        args = "%%shell_args%%",

        -- Enable automatic shell integration.
        -- When "true", END creates shell hook scripts in ~/.config/end/
        -- and injects them on shell startup. This enables:
        --   - Clickable file links in command output
        --   - Output block detection for the Open File feature
        -- Supported shells: zsh, bash, fish.
        -- Set to "false" to disable and remove integration scripts.
        integration = "%%shell_integration%%",
    },

    -- ========================================================================
    -- TERMINAL
    -- ========================================================================

    terminal = {
        -- Maximum number of lines you can scroll back through (100 - 1000000).
        scrollback_lines = %%terminal_scrollback_lines%%,

        -- Lines scrolled per mouse wheel tick and per Shift+PgUp/PgDn step (1 - 100).
        scroll_step = %%terminal_scroll_step%%,

        -- Space between the window edge and the terminal text, in pixels. Four values:
        --   { top, right, bottom, left }
        -- All four values must be present.  Valid range: 0 - 200.
        -- Example: { 10, 10, 10, 10 } gives equal padding on all sides.
        --          { 4, 10, 10, 10 } gives a tighter top edge.
        padding = { %%terminal_padding_top%%, %%terminal_padding_right%%, %%terminal_padding_bottom%%, %%terminal_padding_left%% },

        -- Separator for multiple dropped file paths.
        -- "space" joins paths with spaces (shell convention).
        -- "newline" joins paths with newlines.
        drop_multifiles = "%%terminal_drop_multifiles%%",

        -- Wrap dropped file paths in quotes so spaces and special characters work correctly.
        -- "true": paths with special characters are quoted for the active shell.
        -- "false": paths are pasted raw (for TUI apps that handle paths directly).
        drop_quoted = "%%terminal_drop_quoted%%",
    },

    -- ========================================================================
    -- HYPERLINKS
    -- ========================================================================

    hyperlinks = {
        -- Editor command for opening files from hyperlinks and Open File mode.
        -- The command receives the file path as its first argument.
        -- Example: "nvim", "vim", "nano", "/usr/local/bin/hx"
        editor = "%%hyperlinks_editor%%",

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

    -- ========================================================================
    -- IMAGE
    -- ========================================================================

    image = {
        -- Maximum RGBA bytes retained in the inline image atlas before
        -- eviction, in bytes. Range: 1 MiB - 256 MiB.
        -- atlas_budget = 33554432,

        -- Maximum image atlas dimension in pixels. Images exceeding this
        -- in either dimension are downscaled to fit. Range: 1024 - 8192.
        atlas_dimension = %%image_atlas_dimension%%,

        -- Native preview panel width in cell columns. Range: 10 - 200.
        -- These settings apply to END's own preview surface (hyperlink click,
        -- file open). CLI/TUI tools (fzf, yazi) manage their own layout.
        cols = %%image_cols%%,

        -- Native preview panel height in cell rows. Range: 5 - 100.
        rows = %%image_rows%%,

        -- Padding inside the preview panel, in pixels. Range: 0 - 64.
        padding = %%image_padding%%,

        -- Draw a native border around the preview region.
        border = "%%image_border%%",
    },

}
