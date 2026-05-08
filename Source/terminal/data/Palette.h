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
 * All tables are `inline const` (juce::Colour lacks constexpr constructors).
 * Helper functions are `inline`. No heap allocation occurs.
 *
 * @note Include this header wherever SGR color attribute 38;5;n or 48;5;n
 *       sequences must be resolved to an RGB triple. Use `palette256At(n)` for
 *       index lookup.
 */

#pragma once

#include <JuceHeader.h>
#include <array>

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
inline std::array<juce::Colour, 16> ANSI_16
{{
    juce::Colour (0x00, 0x00, 0x00),
    juce::Colour (0xCD, 0x00, 0x00),
    juce::Colour (0x00, 0xCD, 0x00),
    juce::Colour (0xCD, 0xCD, 0x00),
    juce::Colour (0x00, 0x00, 0xEE),
    juce::Colour (0xCD, 0x00, 0xCD),
    juce::Colour (0x00, 0xCD, 0xCD),
    juce::Colour (0xE5, 0xE5, 0xE5),
    juce::Colour (0x7F, 0x7F, 0x7F),
    juce::Colour (0xFF, 0x00, 0x00),
    juce::Colour (0x00, 0xFF, 0x00),
    juce::Colour (0xFF, 0xFF, 0x00),
    juce::Colour (0x5C, 0x5C, 0xFF),
    juce::Colour (0xFF, 0x00, 0xFF),
    juce::Colour (0x00, 0xFF, 0xFF),
    juce::Colour (0xFF, 0xFF, 0xFF),
}};

/** @brief Updates a single ANSI-16 colour for hot-reload from user config.
 *
 *  Called by LookAndFeel when the terminal colour scheme changes.
 *  The parser reads ANSI_16 via palette256At() on every SGR colour
 *  attribute, so changes take effect on the next terminal output.
 *
 *  @param index  ANSI colour index in the range [0, 15].
 *  @param colour New colour value from user configuration.
 */
inline void setAnsi16Colour (int index, juce::Colour colour) noexcept
{
    ANSI_16[static_cast<size_t> (index)] = colour;
}

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

/** @brief Constructs a single juce::Colour from 6×6×6 cube coordinates.
 *
 *  Each axis value is independently mapped through `cubeComponent()`.
 *
 *  @param r  Red axis index [0, 5].
 *  @param g  Green axis index [0, 5].
 *  @param b  Blue axis index [0, 5].
 *  @return   juce::Colour with the corresponding RGB triple.
 */
inline juce::Colour cubeEntry (int r, int g, int b) noexcept
{
    return juce::Colour (cubeComponent (r), cubeComponent (g), cubeComponent (b));
}

/** @brief The 216-entry 6×6×6 RGB color cube (xterm indices 16–231).
 *
 *  Entries are ordered with blue as the fastest-varying axis, then green,
 *  then red — i.e. index 0 = (r=0,g=0,b=0), index 1 = (r=0,g=0,b=1), …,
 *  index 215 = (r=5,g=5,b=5). Access via `COLOR_CUBE.at(index - CUBE_START)`.
 *
 */
inline const std::array<juce::Colour, CUBE_SIZE> COLOR_CUBE
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
 */
inline const std::array<juce::Colour, GRAY_SIZE> GRAY_RAMP
{{
    juce::Colour (grayComponent (232), grayComponent (232), grayComponent (232)),
    juce::Colour (grayComponent (233), grayComponent (233), grayComponent (233)),
    juce::Colour (grayComponent (234), grayComponent (234), grayComponent (234)),
    juce::Colour (grayComponent (235), grayComponent (235), grayComponent (235)),
    juce::Colour (grayComponent (236), grayComponent (236), grayComponent (236)),
    juce::Colour (grayComponent (237), grayComponent (237), grayComponent (237)),
    juce::Colour (grayComponent (238), grayComponent (238), grayComponent (238)),
    juce::Colour (grayComponent (239), grayComponent (239), grayComponent (239)),
    juce::Colour (grayComponent (240), grayComponent (240), grayComponent (240)),
    juce::Colour (grayComponent (241), grayComponent (241), grayComponent (241)),
    juce::Colour (grayComponent (242), grayComponent (242), grayComponent (242)),
    juce::Colour (grayComponent (243), grayComponent (243), grayComponent (243)),
    juce::Colour (grayComponent (244), grayComponent (244), grayComponent (244)),
    juce::Colour (grayComponent (245), grayComponent (245), grayComponent (245)),
    juce::Colour (grayComponent (246), grayComponent (246), grayComponent (246)),
    juce::Colour (grayComponent (247), grayComponent (247), grayComponent (247)),
    juce::Colour (grayComponent (248), grayComponent (248), grayComponent (248)),
    juce::Colour (grayComponent (249), grayComponent (249), grayComponent (249)),
    juce::Colour (grayComponent (250), grayComponent (250), grayComponent (250)),
    juce::Colour (grayComponent (251), grayComponent (251), grayComponent (251)),
    juce::Colour (grayComponent (252), grayComponent (252), grayComponent (252)),
    juce::Colour (grayComponent (253), grayComponent (253), grayComponent (253)),
    juce::Colour (grayComponent (254), grayComponent (254), grayComponent (254)),
    juce::Colour (grayComponent (255), grayComponent (255), grayComponent (255)),
}};

/** @brief Resolves a single xterm-256 palette index to its juce::Colour.
 *
 *  Dispatches to the appropriate sub-table based on the index range:
 *  - [0, 15]    → ANSI_16
 *  - [16, 231]  → COLOR_CUBE
 *  - [232, 255] → GRAY_RAMP
 *
 *  @param index  xterm palette index in the range [0, 255].
 *  @return       Corresponding juce::Colour.
 *
 */
inline juce::Colour palette256At (int index) noexcept
{
    juce::Colour result (0, 0, 0);

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

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Terminal
