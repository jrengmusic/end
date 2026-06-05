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

return {

    -- =========================================================================
    -- TYPOGRAPHY
    -- =========================================================================
    --
    -- Body text uses a proportional font. Code blocks use a monospace font.
    -- Both ship embedded in the binary (Display / Display Mono).
    -- You can override with any font installed on the system.
    --

    -- Proportional body font family.
    font_family = "Display",

    -- Body font style (e.g. "Regular", "Medium", "Bold").
    font_style = "Medium",

    -- Base body size in points (8 - 72).
    font_size = 16,

    -- Monospace font family for code blocks.
    code_family = "Display Mono",

    -- Code font style (e.g. "Regular", "Medium", "Bold").
    code_style = "Medium",

    -- Code block font size in points (8 - 72).
    code_size = 12,

    -- Line height multiplier (0.8 - 3.0).
    line_height = 1.5,

    -- =========================================================================
    -- HEADING SIZES
    -- =========================================================================
    --
    -- Font sizes for each heading level, in points (8 - 72).
    -- Headings are rendered in bold using the body font family.
    --

    h1_size = 28,
    h2_size = 28,
    h3_size = 24,
    h4_size = 20,
    h5_size = 18,
    h6_size = 16,

    -- =========================================================================
    -- LAYOUT
    -- =========================================================================
    --
    -- Padding around the document content, in pixels.
    -- Order: top, right, bottom, left (CSS convention).
    --

    padding = { 10, 10, 10, 10 },

    -- =========================================================================
    -- COLOURS
    -- =========================================================================
    --
    -- All colours are RRGGBBAA hex strings.
    -- Document background, text colours, and heading colours.
    --

    -- Document background colour.
    background = "0d141cff",

    -- Body text colour.
    body_colour = "b3f9f5ff",

    -- Link text colour.
    link_colour = "01c2d2ff",

    -- Heading colours. All headings share the same colour by default.
    -- Differentiation comes from size and weight, not colour.
    h1_colour = "d4c8a0ff",
    h2_colour = "d4c8a0ff",
    h3_colour = "d4c8a0ff",
    h4_colour = "d4c8a0ff",
    h5_colour = "d4c8a0ff",
    h6_colour = "d4c8a0ff",

    -- =========================================================================
    -- CODE BLOCKS
    -- =========================================================================
    --
    -- Fenced code blocks (```language ... ```) are rendered with syntax
    -- highlighting using the monospace font. Colours follow a vim-pablo-inspired
    -- scheme derived from the Oblivion TET palette.
    --

    -- Code block background colour.
    code_fence_background = "090d12ff",

    -- Inline code colour (e.g. `code` in body text).
    code_colour = "00d0ffff",

    -- Syntax token colours.
    token_error        = "f74a4aff",          -- error tokens
    token_comment      = "6080c0ff",        -- comments
    token_keyword      = "1919ffff",        -- language keywords
    token_operator     = "b0b0b0ff",       -- operators (+, -, =, etc.)
    token_identifier   = "00c6ffff",     -- variable and function names
    token_integer      = "00ff00ff",        -- integer literals
    token_float        = "00ff00ff",          -- float literals
    token_string       = "ffc0c0ff",         -- string literals
    token_bracket      = "80ffffff",        -- brackets ({, }, [, ], (, ))
    token_punctuation  = "ff9080ff",    -- punctuation (;, ,, .)
    token_preprocessor = "9aff00ff",   -- preprocessor directives (#include)

    -- =========================================================================
    -- TABLE
    -- =========================================================================
    --
    -- Markdown tables are rendered with alternating row colours,
    -- configurable borders, and distinct header styling.
    --

    table_background        = "090d12ff",         -- table background
    table_header_background = "112130ff",  -- header row background
    table_row_alt           = "0d141cff",            -- alternating row colour
    table_border_colour     = "2c4144ff",      -- table border colour
    table_header_text       = "bafffdff",        -- header text colour
    table_cell_text         = "b3f9f5ff",          -- cell text colour

    -- =========================================================================
    -- PROGRESS BAR
    -- =========================================================================
    --
    -- Shown while the document is being parsed. A braille spinner and
    -- percentage label are overlaid on a translucent bar.
    --

    progress_background     = "1a1a1aff",     -- bar background
    progress_foreground     = "4488ccff",     -- bar fill colour
    progress_text_colour    = "ccccccff",    -- percentage label colour
    progress_spinner_colour = "4488ccff", -- braille spinner colour

    -- =========================================================================
    -- SCROLLBAR
    -- =========================================================================
    --
    -- Viewport scrollbar appearance.
    --

    scrollbar_thumb      = "2c4144ff",      -- scrollbar thumb (draggable)
    scrollbar_track      = "0d141cff",      -- scrollbar track
    scrollbar_background = "0d141cff", -- scrollbar background

    -- Selection highlight colour (RRGGBBAA).
    selection_colour = "00c8d880",         -- Selection highlight (RRGGBBAA)

    -- =========================================================================
    -- NAVIGATION
    -- =========================================================================
    --
    -- Vim-style keyboard navigation within the document.
    --

    scroll_down   = "j",    -- scroll down one step
    scroll_up     = "k",      -- scroll up one step
    scroll_top    = "gg",     -- jump to top (gg)
    scroll_bottom = "G",  -- jump to bottom (G)
    scroll_step   = 50,      -- pixels per scroll step
}
