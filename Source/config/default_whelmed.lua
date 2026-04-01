-- =============================================================================
--
--                  ████                      ████                                       ████
--                  ████                      ████                                       ████
-- ████        ████ ██████████     ████████   ████ ██████████████     ████████     ██████████
-- ████  ████  ████ ████░░░░████ ████░░░░████ ████ ████░░████░░████ ████░░░░████ ████░░░░████
-- ████  ████  ████ ████    ████ ████████████ ████ ████  ████  ████ ████████████ ████    ████
-- ████████████████ ████    ████ ████░░░░░░░░ ████ ████  ████  ████ ████░░░░░░░░ ████    ████
-- ░░████░░░░████░░ ████    ████ ░░██████████ ████ ████  ████  ████ ░░██████████ ░░██████████
--   ░░░░    ░░░░   ░░░░    ░░░░   ░░░░░░░░░░ ░░░░ ░░░░  ░░░░  ░░░░   ░░░░░░░░░░   ░░░░░░░░░░
--
--          WYSIWYG Hybrid Encoder Lightweight Markdown/Mermaid Editor
--
-- =============================================================================
-- Configuration
-- =============================================================================
--
-- This file is auto-generated with default values on first launch.
-- Edit any value below to customise the Whelmed document viewer.
-- Invalid or missing values fall back to defaults silently.
-- Reload with Cmd+R (no restart needed).
--
-- Colour format: "RRGGBBAA" hex strings (red, green, blue, alpha).
--   - Alpha FF = fully opaque, 00 = fully transparent.
--   - Example: "B3F9F5FF" = frostbite teal, fully opaque.
--
-- =============================================================================

WHELMED = {

    -- =========================================================================
    -- TYPOGRAPHY
    -- =========================================================================
    --
    -- Body text uses a proportional font. Code blocks use a monospace font.
    -- Both ship embedded in the binary (Display / Display Mono).
    -- You can override with any font installed on the system.
    --

    -- Proportional body font family.
    font_family = "%%font_family%%",

    -- Body font style (e.g. "Regular", "Medium", "Bold").
    font_style = "%%font_style%%",

    -- Base body size in points (8 - 72).
    font_size = %%font_size%%,

    -- Monospace font family for code blocks.
    code_family = "%%code_family%%",

    -- Code font style (e.g. "Regular", "Medium", "Bold").
    code_style = "%%code_style%%",

    -- Code block font size in points (8 - 72).
    code_size = %%code_size%%,

    -- Line height multiplier (0.8 - 3.0).
    line_height = %%line_height%%,

    -- =========================================================================
    -- HEADING SIZES
    -- =========================================================================
    --
    -- Font sizes for each heading level, in points (8 - 72).
    -- Headings are rendered in bold using the body font family.
    --

    h1_size = %%h1_size%%,
    h2_size = %%h2_size%%,
    h3_size = %%h3_size%%,
    h4_size = %%h4_size%%,
    h5_size = %%h5_size%%,
    h6_size = %%h6_size%%,

    -- =========================================================================
    -- LAYOUT
    -- =========================================================================
    --
    -- Padding around the document content, in pixels.
    -- Order: top, right, bottom, left (CSS convention).
    --

    padding = { %%padding_top%%, %%padding_right%%, %%padding_bottom%%, %%padding_left%% },

    -- =========================================================================
    -- COLOURS
    -- =========================================================================
    --
    -- All colours are RRGGBBAA hex strings.
    -- Document background, text colours, and heading colours.
    --

    -- Document background colour.
    background = "%%background%%",

    -- Body text colour.
    body_colour = "%%body_colour%%",

    -- Link text colour.
    link_colour = "%%link_colour%%",

    -- Heading colours. All headings share the same colour by default.
    -- Differentiation comes from size and weight, not colour.
    h1_colour = "%%h1_colour%%",
    h2_colour = "%%h2_colour%%",
    h3_colour = "%%h3_colour%%",
    h4_colour = "%%h4_colour%%",
    h5_colour = "%%h5_colour%%",
    h6_colour = "%%h6_colour%%",

    -- =========================================================================
    -- CODE BLOCKS
    -- =========================================================================
    --
    -- Fenced code blocks (```language ... ```) are rendered with syntax
    -- highlighting using the monospace font. Colours follow a vim-pablo-inspired
    -- scheme derived from the Oblivion TET palette.
    --

    -- Code block background colour.
    code_fence_background = "%%code_fence_background%%",

    -- Inline code colour (e.g. `code` in body text).
    code_colour = "%%code_colour%%",

    -- Syntax token colours.
    token_error        = "%%token_error%%",          -- error tokens
    token_comment      = "%%token_comment%%",        -- comments
    token_keyword      = "%%token_keyword%%",        -- language keywords
    token_operator     = "%%token_operator%%",       -- operators (+, -, =, etc.)
    token_identifier   = "%%token_identifier%%",     -- variable and function names
    token_integer      = "%%token_integer%%",        -- integer literals
    token_float        = "%%token_float%%",          -- float literals
    token_string       = "%%token_string%%",         -- string literals
    token_bracket      = "%%token_bracket%%",        -- brackets ({, }, [, ], (, ))
    token_punctuation  = "%%token_punctuation%%",    -- punctuation (;, ,, .)
    token_preprocessor = "%%token_preprocessor%%",   -- preprocessor directives (#include)

    -- =========================================================================
    -- TABLE
    -- =========================================================================
    --
    -- Markdown tables are rendered with alternating row colours,
    -- configurable borders, and distinct header styling.
    --

    table_background        = "%%table_background%%",         -- table background
    table_header_background = "%%table_header_background%%",  -- header row background
    table_row_alt           = "%%table_row_alt%%",            -- alternating row colour
    table_border_colour     = "%%table_border_colour%%",      -- table border colour
    table_header_text       = "%%table_header_text%%",        -- header text colour
    table_cell_text         = "%%table_cell_text%%",          -- cell text colour

    -- =========================================================================
    -- PROGRESS BAR
    -- =========================================================================
    --
    -- Shown while the document is being parsed. A braille spinner and
    -- percentage label are overlaid on a translucent bar.
    --

    progress_background     = "%%progress_background%%",     -- bar background
    progress_foreground     = "%%progress_foreground%%",     -- bar fill colour
    progress_text_colour    = "%%progress_text_colour%%",    -- percentage label colour
    progress_spinner_colour = "%%progress_spinner_colour%%", -- braille spinner colour

    -- =========================================================================
    -- SCROLLBAR
    -- =========================================================================
    --
    -- Viewport scrollbar appearance.
    --

    scrollbar_thumb      = "%%scrollbar_thumb%%",      -- scrollbar thumb (draggable)
    scrollbar_track      = "%%scrollbar_track%%",      -- scrollbar track
    scrollbar_background = "%%scrollbar_background%%", -- scrollbar background

    -- Selection highlight colour (RRGGBBAA).
    selection_colour = "%%selection_colour%%",         -- Selection highlight (RRGGBBAA)

    -- =========================================================================
    -- NAVIGATION
    -- =========================================================================
    --
    -- Vim-style keyboard navigation within the document.
    --

    scroll_down   = "%%scroll_down%%",    -- scroll down one step
    scroll_up     = "%%scroll_up%%",      -- scroll up one step
    scroll_top    = "%%scroll_top%%",     -- jump to top (gg)
    scroll_bottom = "%%scroll_bottom%%",  -- jump to bottom (G)
    scroll_step   = %%scroll_step%%,      -- pixels per scroll step
}
