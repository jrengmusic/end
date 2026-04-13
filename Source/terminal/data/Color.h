/**
 * @file Color.h
 * @brief Terminal color value with three-tier resolution: theme, palette, and direct RGB.
 *
 * A Color encodes one of three color modes in exactly 4 bytes, matching the
 * layout required for trivially-copyable, lock-free transfer between the audio
 * and UI threads.
 *
 * Resolution pipeline:
 *   1. @b theme  — renderer queries the active UI theme for the final RGB value.
 *   2. @b palette — renderer looks up the index in the 256-entry ANSI/xterm
 *                   palette table to obtain an RGB value.
 *   3. @b rgb    — the red/green/blue fields are used directly; no lookup needed.
 */

#pragma once

#include <cstdint>
#include <type_traits>

namespace Terminal
{ /*____________________________________________________________________________*/

/**
 * @struct Color
 * @brief A 4-byte terminal color that supports theme-relative, palette-indexed,
 *        and direct RGB color modes.
 *
 * The four bytes are laid out as:
 * @code
 *   [ red (1B) | green (1B) | blue (1B) | mode (1B) ]
 * @endcode
 *
 * When @c mode == @c palette the @c red field doubles as the palette index and
 * @c green / @c blue are zeroed.  When @c mode == @c theme all three channel
 * fields are zeroed and the final color is resolved at render time from the
 * active UI theme.
 *
 * @note Color satisfies @c std::is_trivially_copyable, which is enforced by the
 *       @c static_assert below.  Do not add virtual methods, non-trivial
 *       constructors, or non-trivial destructors.
 */
struct Color
{
    /**
     * @brief Selects how the color value is interpreted at render time.
     *
     * The resolution pipeline is:
     *   - @c theme   (0) — delegate to the active UI theme.
     *   - @c palette (1) — look up @c red as an index into the 256-entry
     *                      ANSI/xterm palette table.
     *   - @c rgb     (2) — use @c red, @c green, @c blue directly.
     */
    enum Mode : uint8_t
    {
        theme   = 0, ///< Color is resolved from the active UI theme at render time.
        palette = 1, ///< Color is a 256-entry palette index stored in the @c red field.
        rgb     = 2  ///< Color is a direct 24-bit RGB value.
    };

    uint8_t red   { 0 };     ///< Red channel (RGB mode), or palette index (palette mode).
    uint8_t green { 0 };     ///< Green channel (RGB mode); zeroed in palette and theme modes.
    uint8_t blue  { 0 };     ///< Blue channel (RGB mode); zeroed in palette and theme modes.
    Mode    mode  { theme }; ///< Active color mode; determines how the other fields are read.

    /**
     * @brief Sets a direct 24-bit RGB color.
     *
     * Stores @p r, @p g, @p b in the corresponding channel fields and sets
     * @c mode to @c rgb.  The renderer will use these values without any
     * palette or theme lookup.
     *
     * @param r  Red channel value   [0, 255].
     * @param g  Green channel value [0, 255].
     * @param b  Blue channel value  [0, 255].
     */
    void setRGB (uint8_t r, uint8_t g, uint8_t b) noexcept
    {
        red = r; green = g; blue = b; mode = rgb;
    }

    /**
     * @brief Sets a palette-indexed color.
     *
     * Stores @p index in the @c red field (the other channel fields are zeroed)
     * and sets @c mode to @c palette.  The renderer resolves the final RGB value
     * by looking up @p index in the 256-entry ANSI/xterm palette table.
     *
     * @param index  Palette index in the range [0, 255].
     */
    void setPalette (uint8_t index) noexcept
    {
        red = index; green = 0; blue = 0; mode = palette;
    }

    /**
     * @brief Resets the color to theme mode.
     *
     * Zeroes all channel fields and sets @c mode to @c theme.  The renderer
     * will query the active UI theme to obtain the final RGB value.
     */
    void setTheme() noexcept
    {
        red = 0; green = 0; blue = 0; mode = theme;
    }

    /**
     * @brief Returns @c true when the color is in direct RGB mode.
     * @return @c true if @c mode == @c rgb.
     */
    bool isRGB()     const noexcept { return mode == rgb; }

    /**
     * @brief Returns @c true when the color is in palette-index mode.
     * @return @c true if @c mode == @c palette.
     */
    bool isPalette() const noexcept { return mode == palette; }

    /**
     * @brief Returns @c true when the color defers to the active UI theme.
     * @return @c true if @c mode == @c theme.
     */
    bool isTheme()   const noexcept { return mode == theme; }

    /**
     * @brief Returns the raw red channel value in RGB mode.
     *
     * Only meaningful when @c isRGB() is @c true.  In palette mode this field
     * holds the palette index; call @c paletteIndex() instead.
     *
     * @return The red channel byte [0, 255].
     */
    uint8_t getRGB() const noexcept { return red; }

    /**
     * @brief Returns the palette index stored in the @c red field.
     *
     * Only meaningful when @c isPalette() is @c true.  The value is an index
     * into the 256-entry ANSI/xterm palette table.
     *
     * @return Palette index in the range [0, 255].
     */
    uint8_t paletteIndex() const noexcept
    {
        return red;
    }
};

static_assert (std::is_trivially_copyable_v<Color>);
static_assert (sizeof (Color) == 4);

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Terminal
