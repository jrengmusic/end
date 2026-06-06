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
-- Colour format: 0xAARRGGBB hex integer (JUCE native ARGB format).
--   - Alpha 0xFF = fully opaque, 0x00 = fully transparent.
--   - Example: 0xffb3f9f5 = frostbite teal, fully opaque.
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
    -- All colours are 0xAARRGGBB hex integers.
    -- Document background, text colours, and heading colours.
    --

    -- Document background colour.
    background = 0xff0d141c,

    -- Body text colour.
    body_colour = 0xffb3f9f5,

    -- Link text colour.
    link_colour = 0xff01c2d2,

    -- Heading colours. All headings share the same colour by default.
    -- Differentiation comes from size and weight, not colour.
    h1_colour = 0xffd4c8a0,
    h2_colour = 0xffd4c8a0,
    h3_colour = 0xffd4c8a0,
    h4_colour = 0xffd4c8a0,
    h5_colour = 0xffd4c8a0,
    h6_colour = 0xffd4c8a0,

    -- =========================================================================
    -- CODE BLOCKS
    -- =========================================================================
    --
    -- Fenced code blocks (```language ... ```) are rendered with syntax
    -- highlighting using the monospace font. Colours follow a vim-pablo-inspired
    -- scheme derived from the Oblivion TET palette.
    --

    -- Code block background colour.
    code_fence_background = 0xff090d12,

    -- Inline code colour (e.g. `code` in body text).
    code_colour = 0xff00d0ff,

    -- Syntax token colours.
    token_error        = 0xfff74a4a,          -- error tokens
    token_comment      = 0xff6080c0,        -- comments
    token_keyword      = 0xff1919ff,        -- language keywords
    token_operator     = 0xffb0b0b0,       -- operators (+, -, =, etc.)
    token_identifier   = 0xff00c6ff,     -- variable and function names
    token_integer      = 0xff00ff00,        -- integer literals
    token_float        = 0xff00ff00,          -- float literals
    token_string       = 0xffffc0c0,         -- string literals
    token_bracket      = 0xff80ffff,        -- brackets ({, }, [, ], (, ))
    token_punctuation  = 0xffff9080,    -- punctuation (;, ,, .)
    token_preprocessor = 0xff9aff00,   -- preprocessor directives (#include)

    -- =========================================================================
    -- TABLE
    -- =========================================================================
    --
    -- Markdown tables are rendered with alternating row colours,
    -- configurable borders, and distinct header styling.
    --

    table_background        = 0xff090d12,         -- table background
    table_header_background = 0xff112130,  -- header row background
    table_row_alt           = 0xff0d141c,            -- alternating row colour
    table_border_colour     = 0xff2c4144,      -- table border colour
    table_header_text       = 0xffbafffd,        -- header text colour
    table_cell_text         = 0xffb3f9f5,          -- cell text colour

    -- =========================================================================
    -- PROGRESS BAR
    -- =========================================================================
    --
    -- Shown while the document is being parsed. A braille spinner and
    -- percentage label are overlaid on a translucent bar.
    --

    progress_background     = 0xff1a1a1a,     -- bar background
    progress_foreground     = 0xff4488cc,     -- bar fill colour
    progress_text_colour    = 0xffcccccc,    -- percentage label colour
    progress_spinner_colour = 0xff4488cc, -- braille spinner colour

    -- =========================================================================
    -- SCROLLBAR
    -- =========================================================================
    --
    -- Viewport scrollbar appearance.
    --

    scrollbar_thumb      = 0xff2c4144,      -- scrollbar thumb (draggable)
    scrollbar_track      = 0xff0d141c,      -- scrollbar track
    scrollbar_background = 0xff0d141c, -- scrollbar background

    -- Selection highlight colour (0xAARRGGBB).
    selection_colour = 0x8000c8d8,         -- Selection highlight (0xAARRGGBB)

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
