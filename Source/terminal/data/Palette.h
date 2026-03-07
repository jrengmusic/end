/**
 * @file Palette.h
 * @brief Compile-time xterm-256 color palette for terminal color resolution.
 *
 * Defines the complete 256-entry xterm color table as `constexpr` data,
 * partitioned into three canonical segments:
 *
 *  - **Indices 0–15**   : 16 standard ANSI colors (system palette)
 *  - **Indices 16–231** : 6×6×6 RGB color cube (216 entries)
 *  - **Indices 232–255**: 24-step grayscale ramp (black → near-white)
 *
 * All tables and helper functions are `inline constexpr`, evaluated entirely
 * at compile time. No heap allocation or runtime initialization occurs.
 *
 * @note Include this header wherever SGR color attribute 38;5;n or 48;5;n
 *       sequences must be resolved to an RGB triple. Use `PALETTE.at(n)` for
 *       direct index lookup, or `resolvePalette()` to obtain a `juce::Colour`.
 */

#pragma once

#include <JuceHeader.h>
#include <array>
#include "Color.h"

namespace Terminal
{ /*____________________________________________________________________________*/

/** @brief The 16 standard ANSI terminal colors (xterm indices 0–15).
 *
 *  Indices 0–7 are the normal colors (black, red, green, yellow, blue,
 *  magenta, cyan, white). Indices 8–15 are the bright/high-intensity
 *  variants. Values match the xterm default palette.
 *
 *  @note These are the only entries whose exact RGB values are
 *        terminal-defined rather than algorithmically derived. Themes
 *        that override the ANSI 16 should replace this table.
 */
inline constexpr std::array<Color, 16> ANSI_16
{{
    { 0x00, 0x00, 0x00, Color::rgb },
    { 0xCD, 0x00, 0x00, Color::rgb },
    { 0x00, 0xCD, 0x00, Color::rgb },
    { 0xCD, 0xCD, 0x00, Color::rgb },
    { 0x00, 0x00, 0xEE, Color::rgb },
    { 0xCD, 0x00, 0xCD, Color::rgb },
    { 0x00, 0xCD, 0xCD, Color::rgb },
    { 0xE5, 0xE5, 0xE5, Color::rgb },
    { 0x7F, 0x7F, 0x7F, Color::rgb },
    { 0xFF, 0x00, 0x00, Color::rgb },
    { 0x00, 0xFF, 0x00, Color::rgb },
    { 0xFF, 0xFF, 0x00, Color::rgb },
    { 0x5C, 0x5C, 0xFF, Color::rgb },
    { 0xFF, 0x00, 0xFF, Color::rgb },
    { 0x00, 0xFF, 0xFF, Color::rgb },
    { 0xFF, 0xFF, 0xFF, Color::rgb },
}};

/** @brief First xterm index of the 6×6×6 RGB color cube (inclusive). */
inline constexpr int CUBE_START { 16 };

/** @brief Last xterm index of the 6×6×6 RGB color cube (inclusive). */
inline constexpr int CUBE_END { 231 };

/** @brief Total number of entries in the RGB color cube (6×6×6 = 216). */
inline constexpr int CUBE_SIZE { 216 };

/** @brief Maps a cube axis value [0–5] to its 8-bit channel intensity.
 *
 *  The xterm color cube uses a non-linear mapping: level 0 maps to 0,
 *  and levels 1–5 map to `55 + 40 * v`, producing the sequence
 *  {0, 95, 135, 175, 215, 255}.
 *
 *  @param v  Cube axis index in the range [0, 5].
 *  @return   Corresponding 8-bit channel value.
 */
inline constexpr uint8_t cubeComponent (int v) noexcept
{
    return (v == 0) ? uint8_t { 0 }
                    : static_cast<uint8_t> (55 + 40 * v);
}

/** @brief Constructs a single RGB Color from 6×6×6 cube coordinates.
 *
 *  Each axis value is independently mapped through `cubeComponent()`.
 *
 *  @param r  Red axis index [0, 5].
 *  @param g  Green axis index [0, 5].
 *  @param b  Blue axis index [0, 5].
 *  @return   Color with the corresponding RGB triple.
 */
inline constexpr Color cubeEntry (int r, int g, int b) noexcept
{
    return Color { cubeComponent (r), cubeComponent (g), cubeComponent (b), Color::rgb };
}

/** @brief The 216-entry 6×6×6 RGB color cube (xterm indices 16–231).
 *
 *  Entries are ordered with blue as the fastest-varying axis, then green,
 *  then red — i.e. index 0 = (r=0,g=0,b=0), index 1 = (r=0,g=0,b=1), …,
 *  index 215 = (r=5,g=5,b=5). Access via `COLOR_CUBE.at(index - CUBE_START)`.
 *
 *  @note Fully evaluated at compile time; zero runtime cost.
 */
inline constexpr std::array<Color, CUBE_SIZE> COLOR_CUBE
{{
    cubeEntry (0, 0, 0), cubeEntry (0, 0, 1), cubeEntry (0, 0, 2), cubeEntry (0, 0, 3), cubeEntry (0, 0, 4), cubeEntry (0, 0, 5),
    cubeEntry (0, 1, 0), cubeEntry (0, 1, 1), cubeEntry (0, 1, 2), cubeEntry (0, 1, 3), cubeEntry (0, 1, 4), cubeEntry (0, 1, 5),
    cubeEntry (0, 2, 0), cubeEntry (0, 2, 1), cubeEntry (0, 2, 2), cubeEntry (0, 2, 3), cubeEntry (0, 2, 4), cubeEntry (0, 2, 5),
    cubeEntry (0, 3, 0), cubeEntry (0, 3, 1), cubeEntry (0, 3, 2), cubeEntry (0, 3, 3), cubeEntry (0, 3, 4), cubeEntry (0, 3, 5),
    cubeEntry (0, 4, 0), cubeEntry (0, 4, 1), cubeEntry (0, 4, 2), cubeEntry (0, 4, 3), cubeEntry (0, 4, 4), cubeEntry (0, 4, 5),
    cubeEntry (0, 5, 0), cubeEntry (0, 5, 1), cubeEntry (0, 5, 2), cubeEntry (0, 5, 3), cubeEntry (0, 5, 4), cubeEntry (0, 5, 5),

    cubeEntry (1, 0, 0), cubeEntry (1, 0, 1), cubeEntry (1, 0, 2), cubeEntry (1, 0, 3), cubeEntry (1, 0, 4), cubeEntry (1, 0, 5),
    cubeEntry (1, 1, 0), cubeEntry (1, 1, 1), cubeEntry (1, 1, 2), cubeEntry (1, 1, 3), cubeEntry (1, 1, 4), cubeEntry (1, 1, 5),
    cubeEntry (1, 2, 0), cubeEntry (1, 2, 1), cubeEntry (1, 2, 2), cubeEntry (1, 2, 3), cubeEntry (1, 2, 4), cubeEntry (1, 2, 5),
    cubeEntry (1, 3, 0), cubeEntry (1, 3, 1), cubeEntry (1, 3, 2), cubeEntry (1, 3, 3), cubeEntry (1, 3, 4), cubeEntry (1, 3, 5),
    cubeEntry (1, 4, 0), cubeEntry (1, 4, 1), cubeEntry (1, 4, 2), cubeEntry (1, 4, 3), cubeEntry (1, 4, 4), cubeEntry (1, 4, 5),
    cubeEntry (1, 5, 0), cubeEntry (1, 5, 1), cubeEntry (1, 5, 2), cubeEntry (1, 5, 3), cubeEntry (1, 5, 4), cubeEntry (1, 5, 5),

    cubeEntry (2, 0, 0), cubeEntry (2, 0, 1), cubeEntry (2, 0, 2), cubeEntry (2, 0, 3), cubeEntry (2, 0, 4), cubeEntry (2, 0, 5),
    cubeEntry (2, 1, 0), cubeEntry (2, 1, 1), cubeEntry (2, 1, 2), cubeEntry (2, 1, 3), cubeEntry (2, 1, 4), cubeEntry (2, 1, 5),
    cubeEntry (2, 2, 0), cubeEntry (2, 2, 1), cubeEntry (2, 2, 2), cubeEntry (2, 2, 3), cubeEntry (2, 2, 4), cubeEntry (2, 2, 5),
    cubeEntry (2, 3, 0), cubeEntry (2, 3, 1), cubeEntry (2, 3, 2), cubeEntry (2, 3, 3), cubeEntry (2, 3, 4), cubeEntry (2, 3, 5),
    cubeEntry (2, 4, 0), cubeEntry (2, 4, 1), cubeEntry (2, 4, 2), cubeEntry (2, 4, 3), cubeEntry (2, 4, 4), cubeEntry (2, 4, 5),
    cubeEntry (2, 5, 0), cubeEntry (2, 5, 1), cubeEntry (2, 5, 2), cubeEntry (2, 5, 3), cubeEntry (2, 5, 4), cubeEntry (2, 5, 5),

    cubeEntry (3, 0, 0), cubeEntry (3, 0, 1), cubeEntry (3, 0, 2), cubeEntry (3, 0, 3), cubeEntry (3, 0, 4), cubeEntry (3, 0, 5),
    cubeEntry (3, 1, 0), cubeEntry (3, 1, 1), cubeEntry (3, 1, 2), cubeEntry (3, 1, 3), cubeEntry (3, 1, 4), cubeEntry (3, 1, 5),
    cubeEntry (3, 2, 0), cubeEntry (3, 2, 1), cubeEntry (3, 2, 2), cubeEntry (3, 2, 3), cubeEntry (3, 2, 4), cubeEntry (3, 2, 5),
    cubeEntry (3, 3, 0), cubeEntry (3, 3, 1), cubeEntry (3, 3, 2), cubeEntry (3, 3, 3), cubeEntry (3, 3, 4), cubeEntry (3, 3, 5),
    cubeEntry (3, 4, 0), cubeEntry (3, 4, 1), cubeEntry (3, 4, 2), cubeEntry (3, 4, 3), cubeEntry (3, 4, 4), cubeEntry (3, 4, 5),
    cubeEntry (3, 5, 0), cubeEntry (3, 5, 1), cubeEntry (3, 5, 2), cubeEntry (3, 5, 3), cubeEntry (3, 5, 4), cubeEntry (3, 5, 5),

    cubeEntry (4, 0, 0), cubeEntry (4, 0, 1), cubeEntry (4, 0, 2), cubeEntry (4, 0, 3), cubeEntry (4, 0, 4), cubeEntry (4, 0, 5),
    cubeEntry (4, 1, 0), cubeEntry (4, 1, 1), cubeEntry (4, 1, 2), cubeEntry (4, 1, 3), cubeEntry (4, 1, 4), cubeEntry (4, 1, 5),
    cubeEntry (4, 2, 0), cubeEntry (4, 2, 1), cubeEntry (4, 2, 2), cubeEntry (4, 2, 3), cubeEntry (4, 2, 4), cubeEntry (4, 2, 5),
    cubeEntry (4, 3, 0), cubeEntry (4, 3, 1), cubeEntry (4, 3, 2), cubeEntry (4, 3, 3), cubeEntry (4, 3, 4), cubeEntry (4, 3, 5),
    cubeEntry (4, 4, 0), cubeEntry (4, 4, 1), cubeEntry (4, 4, 2), cubeEntry (4, 4, 3), cubeEntry (4, 4, 4), cubeEntry (4, 4, 5),
    cubeEntry (4, 5, 0), cubeEntry (4, 5, 1), cubeEntry (4, 5, 2), cubeEntry (4, 5, 3), cubeEntry (4, 5, 4), cubeEntry (4, 5, 5),

    cubeEntry (5, 0, 0), cubeEntry (5, 0, 1), cubeEntry (5, 0, 2), cubeEntry (5, 0, 3), cubeEntry (5, 0, 4), cubeEntry (5, 0, 5),
    cubeEntry (5, 1, 0), cubeEntry (5, 1, 1), cubeEntry (5, 1, 2), cubeEntry (5, 1, 3), cubeEntry (5, 1, 4), cubeEntry (5, 1, 5),
    cubeEntry (5, 2, 0), cubeEntry (5, 2, 1), cubeEntry (5, 2, 2), cubeEntry (5, 2, 3), cubeEntry (5, 2, 4), cubeEntry (5, 2, 5),
    cubeEntry (5, 3, 0), cubeEntry (5, 3, 1), cubeEntry (5, 3, 2), cubeEntry (5, 3, 3), cubeEntry (5, 3, 4), cubeEntry (5, 3, 5),
    cubeEntry (5, 4, 0), cubeEntry (5, 4, 1), cubeEntry (5, 4, 2), cubeEntry (5, 4, 3), cubeEntry (5, 4, 4), cubeEntry (5, 4, 5),
    cubeEntry (5, 5, 0), cubeEntry (5, 5, 1), cubeEntry (5, 5, 2), cubeEntry (5, 5, 3), cubeEntry (5, 5, 4), cubeEntry (5, 5, 5),
}};

/** @brief First xterm index of the grayscale ramp (inclusive). */
inline constexpr int GRAY_START { 232 };

/** @brief Last xterm index of the grayscale ramp (inclusive). */
inline constexpr int GRAY_END { 255 };

/** @brief Total number of entries in the grayscale ramp. */
inline constexpr int GRAY_SIZE { 24 };

/** @brief Maps a grayscale xterm index [232–255] to its 8-bit luminance value.
 *
 *  The ramp starts at 8 and increments by 10 per step, producing the
 *  sequence {8, 18, 28, …, 238}. Neither pure black (0) nor pure white
 *  (255) is included — those are covered by ANSI indices 0 and 15.
 *
 *  @param index  Absolute xterm palette index in the range [232, 255].
 *  @return       8-bit luminance value for all three channels.
 */
inline constexpr uint8_t grayComponent (int index) noexcept
{
    return static_cast<uint8_t> (8 + 10 * (index - GRAY_START));
}

/** @brief The 24-entry grayscale ramp (xterm indices 232–255).
 *
 *  Each entry is a neutral gray with equal R, G, B values stepping from
 *  8 to 238 in increments of 10. Access via `GRAY_RAMP.at(index - GRAY_START)`.
 *
 *  @note Fully evaluated at compile time; zero runtime cost.
 */
inline constexpr std::array<Color, GRAY_SIZE> GRAY_RAMP
{{
    { grayComponent (232), grayComponent (232), grayComponent (232), Color::rgb },
    { grayComponent (233), grayComponent (233), grayComponent (233), Color::rgb },
    { grayComponent (234), grayComponent (234), grayComponent (234), Color::rgb },
    { grayComponent (235), grayComponent (235), grayComponent (235), Color::rgb },
    { grayComponent (236), grayComponent (236), grayComponent (236), Color::rgb },
    { grayComponent (237), grayComponent (237), grayComponent (237), Color::rgb },
    { grayComponent (238), grayComponent (238), grayComponent (238), Color::rgb },
    { grayComponent (239), grayComponent (239), grayComponent (239), Color::rgb },
    { grayComponent (240), grayComponent (240), grayComponent (240), Color::rgb },
    { grayComponent (241), grayComponent (241), grayComponent (241), Color::rgb },
    { grayComponent (242), grayComponent (242), grayComponent (242), Color::rgb },
    { grayComponent (243), grayComponent (243), grayComponent (243), Color::rgb },
    { grayComponent (244), grayComponent (244), grayComponent (244), Color::rgb },
    { grayComponent (245), grayComponent (245), grayComponent (245), Color::rgb },
    { grayComponent (246), grayComponent (246), grayComponent (246), Color::rgb },
    { grayComponent (247), grayComponent (247), grayComponent (247), Color::rgb },
    { grayComponent (248), grayComponent (248), grayComponent (248), Color::rgb },
    { grayComponent (249), grayComponent (249), grayComponent (249), Color::rgb },
    { grayComponent (250), grayComponent (250), grayComponent (250), Color::rgb },
    { grayComponent (251), grayComponent (251), grayComponent (251), Color::rgb },
    { grayComponent (252), grayComponent (252), grayComponent (252), Color::rgb },
    { grayComponent (253), grayComponent (253), grayComponent (253), Color::rgb },
    { grayComponent (254), grayComponent (254), grayComponent (254), Color::rgb },
    { grayComponent (255), grayComponent (255), grayComponent (255), Color::rgb },
}};

/** @brief Resolves a single xterm-256 palette index to its Color value.
 *
 *  Dispatches to the appropriate sub-table based on the index range:
 *  - [0, 15]    → ANSI_16
 *  - [16, 231]  → COLOR_CUBE
 *  - [232, 255] → GRAY_RAMP
 *
 *  @param index  xterm palette index in the range [0, 255].
 *  @return       Corresponding Color with Color::rgb mode set.
 *
 *  @note Prefer direct `PALETTE.at(n)` access over calling this function
 *        at runtime; this function exists to populate PALETTE at compile time.
 */
inline constexpr Color palette256At (int index) noexcept
{
    Color result { 0, 0, 0, Color::rgb };

    if (index >= GRAY_START)
    {
        result = GRAY_RAMP.at (static_cast<size_t> (index - GRAY_START));
    }
    else if (index >= CUBE_START)
    {
        result = COLOR_CUBE.at (static_cast<size_t> (index - CUBE_START));
    }
    else
    {
        result = ANSI_16.at (static_cast<size_t> (index));
    }

    return result;
}

/** @brief The complete 256-entry xterm color palette, fully resolved at compile time.
 *
 *  A flat lookup table mapping every xterm-256 color index [0–255] to its
 *  canonical RGB Color value. The three segments are laid out contiguously:
 *
 *  | Range     | Count | Source      | Description              |
 *  |-----------|-------|-------------|--------------------------|
 *  | 0–15      | 16    | ANSI_16     | Standard ANSI colors     |
 *  | 16–231    | 216   | COLOR_CUBE  | 6×6×6 RGB cube           |
 *  | 232–255   | 24    | GRAY_RAMP   | Grayscale ramp           |
 *
 *  @note All 256 entries are `constexpr` and reside in read-only data.
 *        Use `PALETTE.at(n)` for bounds-checked access.
 *        Pass the result to `resolvePalette()` to obtain a `juce::Colour`.
 */
inline constexpr std::array<Color, 256> PALETTE
{
    palette256At (0),  palette256At (1),  palette256At (2),  palette256At (3),
    palette256At (4),  palette256At (5),  palette256At (6),  palette256At (7),
    palette256At (8),  palette256At (9),  palette256At (10), palette256At (11),
    palette256At (12), palette256At (13), palette256At (14), palette256At (15),
    palette256At (16),  palette256At (17),  palette256At (18),  palette256At (19),
    palette256At (20),  palette256At (21),  palette256At (22),  palette256At (23),
    palette256At (24),  palette256At (25),  palette256At (26),  palette256At (27),
    palette256At (28),  palette256At (29),  palette256At (30),  palette256At (31),
    palette256At (32),  palette256At (33),  palette256At (34),  palette256At (35),
    palette256At (36),  palette256At (37),  palette256At (38),  palette256At (39),
    palette256At (40),  palette256At (41),  palette256At (42),  palette256At (43),
    palette256At (44),  palette256At (45),  palette256At (46),  palette256At (47),
    palette256At (48),  palette256At (49),  palette256At (50),  palette256At (51),
    palette256At (52),  palette256At (53),  palette256At (54),  palette256At (55),
    palette256At (56),  palette256At (57),  palette256At (58),  palette256At (59),
    palette256At (60),  palette256At (61),  palette256At (62),  palette256At (63),
    palette256At (64),  palette256At (65),  palette256At (66),  palette256At (67),
    palette256At (68),  palette256At (69),  palette256At (70),  palette256At (71),
    palette256At (72),  palette256At (73),  palette256At (74),  palette256At (75),
    palette256At (76),  palette256At (77),  palette256At (78),  palette256At (79),
    palette256At (80),  palette256At (81),  palette256At (82),  palette256At (83),
    palette256At (84),  palette256At (85),  palette256At (86),  palette256At (87),
    palette256At (88),  palette256At (89),  palette256At (90),  palette256At (91),
    palette256At (92),  palette256At (93),  palette256At (94),  palette256At (95),
    palette256At (96),  palette256At (97),  palette256At (98),  palette256At (99),
    palette256At (100), palette256At (101), palette256At (102), palette256At (103),
    palette256At (104), palette256At (105), palette256At (106), palette256At (107),
    palette256At (108), palette256At (109), palette256At (110), palette256At (111),
    palette256At (112), palette256At (113), palette256At (114), palette256At (115),
    palette256At (116), palette256At (117), palette256At (118), palette256At (119),
    palette256At (120), palette256At (121), palette256At (122), palette256At (123),
    palette256At (124), palette256At (125), palette256At (126), palette256At (127),
    palette256At (128), palette256At (129), palette256At (130), palette256At (131),
    palette256At (132), palette256At (133), palette256At (134), palette256At (135),
    palette256At (136), palette256At (137), palette256At (138), palette256At (139),
    palette256At (140), palette256At (141), palette256At (142), palette256At (143),
    palette256At (144), palette256At (145), palette256At (146), palette256At (147),
    palette256At (148), palette256At (149), palette256At (150), palette256At (151),
    palette256At (152), palette256At (153), palette256At (154), palette256At (155),
    palette256At (156), palette256At (157), palette256At (158), palette256At (159),
    palette256At (160), palette256At (161), palette256At (162), palette256At (163),
    palette256At (164), palette256At (165), palette256At (166), palette256At (167),
    palette256At (168), palette256At (169), palette256At (170), palette256At (171),
    palette256At (172), palette256At (173), palette256At (174), palette256At (175),
    palette256At (176), palette256At (177), palette256At (178), palette256At (179),
    palette256At (180), palette256At (181), palette256At (182), palette256At (183),
    palette256At (184), palette256At (185), palette256At (186), palette256At (187),
    palette256At (188), palette256At (189), palette256At (190), palette256At (191),
    palette256At (192), palette256At (193), palette256At (194), palette256At (195),
    palette256At (196), palette256At (197), palette256At (198), palette256At (199),
    palette256At (200), palette256At (201), palette256At (202), palette256At (203),
    palette256At (204), palette256At (205), palette256At (206), palette256At (207),
    palette256At (208), palette256At (209), palette256At (210), palette256At (211),
    palette256At (212), palette256At (213), palette256At (214), palette256At (215),
    palette256At (216), palette256At (217), palette256At (218), palette256At (219),
    palette256At (220), palette256At (221), palette256At (222), palette256At (223),
    palette256At (224), palette256At (225), palette256At (226), palette256At (227),
    palette256At (228), palette256At (229), palette256At (230), palette256At (231),
    palette256At (232), palette256At (233), palette256At (234), palette256At (235),
    palette256At (236), palette256At (237), palette256At (238), palette256At (239),
    palette256At (240), palette256At (241), palette256At (242), palette256At (243),
    palette256At (244), palette256At (245), palette256At (246), palette256At (247),
    palette256At (248), palette256At (249), palette256At (250), palette256At (251),
    palette256At (252), palette256At (253), palette256At (254), palette256At (255),
};

/** @brief Converts a terminal Color value to a JUCE Colour for rendering.
 *
 *  Extracts the red, green, and blue channels from @p color and constructs
 *  a fully opaque `juce::Colour`. The alpha channel is implicitly 0xFF.
 *
 *  @param color  A Color with Color::rgb mode, typically sourced from PALETTE.
 *  @return       Equivalent `juce::Colour` suitable for use in JUCE paint calls.
 *
 *  @note This is the terminal-side bridge between the palette data model and
 *        the JUCE rendering layer. Call this once per attribute change, not
 *        per pixel.
 */
inline juce::Colour resolvePalette (const Color& color) noexcept
{
    return juce::Colour (color.red, color.green, color.blue);
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Terminal
