-- ============================================================================
-- END display.lua — visual appearance settings
-- https://github.com/jrengmusic/end
-- ============================================================================
--
-- This file is auto-generated with default values on first launch.
-- Edit any value below to customise your terminal.
-- Invalid or missing values fall back to defaults silently.
-- Reload with Cmd+R (no restart needed).
--
-- Colour format: "#RRGGBB" (fully opaque) or "#RRGGBBAA" (with alpha).
--   - "#RGB" and "#RGBA" shorthand supported (e.g. "#F00" becomes "#FF0000").
--   - "rgba(r, g, b, a)" functional notation (a is 0.0 - 1.0).
--
-- ============================================================================

return {

    -- ========================================================================
    -- FONT
    -- ========================================================================

    font = {
        -- Font used for terminal text.
        -- Must be a monospace font installed on the system.
        family = "%%font_family%%",

        -- Font size in points before zoom is applied (1 - 200).
        size = %%font_size%%,

        -- Combine certain character sequences into symbols (e.g. -> becomes an arrow).
        ligatures = "%%font_ligatures%%",

        -- Make text appear bolder.
        -- Useful for thin fonts that are hard to read at small sizes.
        embolden = "%%font_embolden%%",

        -- Line height multiplier applied to terminal cell height (0.5 - 3.0).
        -- 1.0 = no adjustment. Values above 1.0 increase spacing, below decrease it.
        line_height = %%font_line_height%%,

        -- Cell width multiplier applied to terminal cell width (0.5 - 3.0).
        -- 1.0 = no adjustment. Values above 1.0 widen cells, below narrow them.
        cell_width = %%font_cell_width%%,

        -- Whether font size follows the Windows desktop scale.
        -- "true"  — font size scales with the desktop scale (system default behaviour).
        -- "false" — font stays at its configured point size in physical pixels
        --           regardless of the desktop scale slider (12pt always looks 12pt).
        -- Windows only. No effect on macOS or Linux.
        desktop_scale = "%%font_desktop_scale%%",
    },

    -- ========================================================================
    -- CURSOR
    -- ========================================================================

    cursor = {
        -- Character displayed as the cursor.
        -- Default is a solid block (the standard blinking rectangle).
        -- You can use any char, NF icons, including color emoji.
        -- Only used when cursor shape is "glyph" (user-defined).
        -- Programs (like vim or tmux) can change the cursor shape
        -- unless cursor.force is true.
        char = "%%cursor_char%%",

        -- Enable cursor blinking.
        blink = "%%cursor_blink%%",

        -- Blink interval in milliseconds (100 - 5000).
        -- Full cycle = 2x this value (on for interval, off for interval).
        blink_interval = %%cursor_blink_interval%%,

        -- Lock the cursor to your configured shape and colour. Programs cannot change it.
        -- When "true", programs cannot change cursor shape or colour.
        force = "%%cursor_force%%",
    },

    -- ========================================================================
    -- COLOURS
    -- ========================================================================
    --
    -- The 16 standard terminal colours. Programs like ls, git, and vim use these.
    -- The first 8 are normal, the next 8 are brighter versions.
    -- Format: "#RRGGBB" (opaque) or "#RRGGBBAA" (with alpha).
    --

    colours = {
        -- Default text foreground colour.
        foreground = "%%colours_foreground%%",

        -- Default background colour.
        -- The last two hex digits control background transparency (GPU only).
        -- CPU rendering always uses a fully opaque background.
        background = "%%colours_background%%",

        -- Cursor colour.
        -- Programs may change this colour while running.
        cursor = "%%colours_cursor%%",

        -- Selection highlight colour.
        -- Semi-transparent recommended so text remains readable.
        selection = "%%colours_selection%%",

        -- Selection-mode cursor colour.
        -- Shown instead of the normal cursor when selection mode is active.
        selection_cursor = "%%colours_selection_cursor%%",

        -- Black
        black = "%%colours_black%%",

        -- Red
        red = "%%colours_red%%",

        -- Green
        green = "%%colours_green%%",

        -- Yellow
        yellow = "%%colours_yellow%%",

        -- Blue
        blue = "%%colours_blue%%",

        -- Magenta
        magenta = "%%colours_magenta%%",

        -- Cyan
        cyan = "%%colours_cyan%%",

        -- White
        white = "%%colours_white%%",

        -- Bright black
        bright_black = "%%colours_bright_black%%",

        -- Bright red
        bright_red = "%%colours_bright_red%%",

        -- Bright green
        bright_green = "%%colours_bright_green%%",

        -- Bright yellow
        bright_yellow = "%%colours_bright_yellow%%",

        -- Bright blue
        bright_blue = "%%colours_bright_blue%%",

        -- Bright magenta
        bright_magenta = "%%colours_bright_magenta%%",

        -- Bright cyan
        bright_cyan = "%%colours_bright_cyan%%",

        -- Bright white
        bright_white = "%%colours_bright_white%%",

        -- Status bar full background colour.
        -- Default matches the active tab background (tab.active).
        status_bar = "%%colours_status_bar%%",

        -- Status bar mode label background colour.
        -- Default matches the active tab indicator colour (tab.indicator).
        status_bar_label_bg = "%%colours_status_bar_label_bg%%",

        -- Status bar mode label text colour.
        status_bar_label_fg = "%%colours_status_bar_label_fg%%",

        -- Status bar spinner colour.
        status_bar_spinner = "%%colours_status_bar_spinner%%",

        -- Open File mode hint label background colour.
        -- Shown as the badge background behind single- or double-letter hint keys.
        hint_label_bg = "%%colours_hint_label_bg%%",

        -- Open File mode hint label foreground (text) colour.
        hint_label_fg = "%%colours_hint_label_fg%%",
    },

    -- ========================================================================
    -- WINDOW
    -- ========================================================================

    window = {
        -- Window title shown in the title bar and mission control.
        title = "%%window_title%%",

        -- Initial window width in pixels.
        width = %%window_width%%,

        -- Initial window height in pixels.
        height = %%window_height%%,

        -- Tint colour for the window background. Most visible with blur enabled.
        colour = "%%window_colour%%",

        -- Window opacity (0.0 fully transparent - 1.0 fully opaque).
        -- GPU only. Has no effect with CPU rendering.
        -- macOS and Windows 10 only. No effect on Windows 11.
        opacity = %%window_opacity%%,

        -- Background blur radius in pixels (0 = no blur).
        -- GPU only. Has no effect with CPU rendering.
        -- macOS: controls blur intensity.
        -- Windows 10: blur is on but intensity is set by the system.
        -- Windows 11: uses the system glass effect. This setting has no effect.
        blur_radius = %%window_blur_radius%%,

        -- Keep window above all other windows.
        always_on_top = "%%window_always_on_top%%",

        -- Show native window buttons (close / minimise / maximise).
        buttons = "%%window_buttons%%",

        -- Force DWM visual effects on Windows 11 virtual machines.
        -- When "true", injects the ForceEffectMode registry key to enable
        -- rounded window corners that DWM normally disables inside VMs.
        -- Only takes effect on Windows 11 running on a software renderer (VM).
        -- Requires elevated privileges (Run as Administrator).
        -- Reload config and restart END to apply.
        -- No effect on macOS, Linux, or physical Windows machines.
        force_dwm = "%%window_force_dwm%%",

        -- Persist window size across instances to ~/.config/end/window.state.
        -- When "true", every quit writes the current window size; new instances
        -- (with no session to restore) load it as their initial size.
        -- When "false", the file is neither read nor written; new instances
        -- fall back to window.width and window.height above.
        -- Restored sessions always use their own persisted window size and
        -- ignore window.state regardless of this setting.
        save_size = "%%window_save_size%%",

        -- Show a confirmation dialog when Ctrl+Q is pressed.
        -- When "true", a Yes/No dialog asks before quitting (or saving the session
        -- in daemon mode).  When "false", Ctrl+Q quits immediately with no prompt.
        confirmation_on_exit = "%%window_confirmation_on_exit%%",
    },

    -- ========================================================================
    -- TAB BAR
    -- ========================================================================

    tab = {
        -- Tab bar font family.
        family = "%%tab_family%%",

        -- Tab bar font size in points.
        size = %%tab_size%%,

        -- Active tab text colour.
        foreground = "%%tab_foreground%%",

        -- Inactive tab text colour.
        inactive = "%%tab_inactive%%",

        -- Tab bar position: "top", "bottom", "left", "right".
        position = "%%tab_position%%",

        -- Tab separator line colour.
        line = "%%tab_line%%",

        -- Active tab background colour.
        active = "%%tab_active%%",

        -- Active tab indicator colour.
        indicator = "%%tab_indicator%%",
    },

    -- ========================================================================
    -- MENU
    -- ========================================================================

    menu = {
        -- Popup menu background opacity (0.0 - 1.0).
        opacity = %%menu_opacity%%,
    },

    -- ========================================================================
    -- OVERLAY
    -- ========================================================================

    overlay = {
        -- Overlay font family (used for status messages).
        family = "%%overlay_family%%",

        -- Overlay font size in points.
        size = %%overlay_size%%,

        -- Overlay text colour.
        colour = "%%overlay_colour%%",
    },

    -- ========================================================================
    -- PANE
    -- ========================================================================

    pane = {
        -- Pane divider bar colour.
        bar_colour = "%%pane_bar_colour%%",

        -- Pane divider bar colour when dragging or hovering.
        bar_highlight = "%%pane_bar_highlight%%",
    },

    -- ========================================================================
    -- STATUS BAR
    -- ========================================================================

    status_bar = {
        -- Status bar position: "top" or "bottom".
        position = "%%status_bar_position%%",

        -- Status bar font family.
        font_family = "%%status_bar_font_family%%",

        -- Status bar font size in points.
        font_size = %%status_bar_font_size%%,

        -- Status bar font style.
        font_style = "%%status_bar_font_style%%",
    },

    -- ========================================================================
    -- ACTION LIST
    -- ========================================================================

    action_list = {
        -- Action list position: "top" or "bottom".
        position = "%%action_list_position%%",

        -- Close the action list after running an action.
        -- When "false", the list stays open after execution.
        close_on_run = "%%action_list_close_on_run%%",

        -- Font family for action name labels.
        name_font_family = "%%action_list_name_font_family%%",

        -- Action-list action-name font style (Regular, Bold, Book, Medium).
        name_font_style = "%%action_list_name_font_style%%",

        -- Font size for action name labels in points (6 - 72).
        name_font_size = %%action_list_name_font_size%%,

        -- Font family for keyboard shortcut labels. Should be monospace.
        shortcut_font_family = "%%action_list_shortcut_font_family%%",

        -- Action-list shortcut font style (Regular, Bold).
        shortcut_font_style = "%%action_list_shortcut_font_style%%",

        -- Font size for keyboard shortcut labels in points (6 - 72).
        shortcut_font_size = %%action_list_shortcut_font_size%%,

        -- Space between the action list edge and content, in pixels.
        -- Four values: { top, right, bottom, left }. Valid range: 0 - 200.
        padding = { %%action_list_padding_top%%, %%action_list_padding_right%%, %%action_list_padding_bottom%%, %%action_list_padding_left%% },

        -- Text colour for action name labels.
        name_colour = "%%action_list_name_colour%%",

        -- Text colour for keyboard shortcut labels.
        shortcut_colour = "%%action_list_shortcut_colour%%",

        -- Proportional width of the action list relative to the terminal window (0.1 - 1.0).
        width = %%action_list_width%%,

        -- Maximum proportional height of the action list relative to the terminal window (0.1 - 1.0).
        -- When all results exceed this height, the list scrolls.
        height = %%action_list_height%%,

        -- Background colour for the highlighted/selected row.
        -- Leave empty to use the terminal selection colour (colours.selection).
        highlight_colour = "%%action_list_highlight_colour%%",
    },

    -- ========================================================================
    -- POPUP BORDER
    -- ========================================================================

    popup = {
        -- Popup border colour.
        border_colour = "%%popup_border_colour%%",

        -- Popup border stroke width in pixels (0 = no border).
        border_width = %%popup_border_width%%,
    },

}
