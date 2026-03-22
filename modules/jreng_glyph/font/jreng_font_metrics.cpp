/**
 * @file jreng_font_metrics.cpp
 * @brief FreeType cell geometry calculation for the terminal renderer.
 *
 * Compiled on all platforms **except macOS** (`!JUCE_MAC`).  Implements
 * `jreng::Font::calcMetrics()`, which derives the logical and physical cell
 * dimensions used to lay out the terminal grid.
 *
 * ### Cell geometry overview
 *
 * A terminal cell is a fixed-size rectangle that holds exactly one character
 * (or two for wide/CJK characters).  Its dimensions are determined by the
 * active font at a given point size:
 *
 * | Dimension      | Source                                              |
 * |----------------|-----------------------------------------------------|
 * | Cell width     | Maximum advance width across printable ASCII glyphs |
 * | Cell height    | `face->size->metrics.height` (ascender + descender) |
 * | Baseline       | `face->size->metrics.ascender` (distance from top)  |
 *
 * ### Coordinate spaces
 *
 * `calcMetrics()` populates three coordinate spaces in `jreng::Font::Metrics`:
 *
 * - **Logical** (CSS pixels) — used for layout, grid sizing, and UI.
 * - **Physical** (device pixels) — logical × `displayScale`; used for
 *   rasterization and GPU atlas uploads.
 * - **Fixed** (26.6 fixed-point) — raw FreeType values; used for
 *   `FT_Set_Char_Size` and HarfBuzz position arithmetic.
 *
 * ### 26.6 fixed-point arithmetic
 *
 * FreeType stores metric values in 26.6 fixed-point format: the lower 6 bits
 * represent the fractional part (1/64 pixel precision) and the upper bits hold
 * the integer pixel count.  `ftFixedScale` (64) is the scale factor:
 *
 * @code
 * // 26.6 → pixels (ceiling)
 * int px = (v26_6 + 63) >> 6;   // ceil26_6ToPx()
 *
 * // pixels → 26.6 (nearest)
 * int v26_6 = (int)(px * 64.0f + 0.5f);   // roundFloatPxTo26_6()
 * @endcode
 *
 * ### Display scale handling
 *
 * `calcMetrics()` sets the FreeType character size **twice**:
 *
 * 1. **Logical size** — `FT_Set_Char_Size (face, 0, height26_6, baseDpi, baseDpi)`.
 *    Metrics are read at this size to compute logical cell dimensions.
 * 2. **Render size** — `FT_Set_Char_Size (face, 0, height26_6, renderDpi, renderDpi)`
 *    where `renderDpi = baseDpi * displayScale`.  The face is left at this size
 *    after the call so that subsequent rasterization (`rasterizeToImage`) uses
 *    the correct physical resolution.
 *
 * Physical pixel values are derived by multiplying the logical values by
 * `displayScale` rather than re-reading FreeType metrics at the render DPI,
 * ensuring consistent rounding between the two coordinate spaces.
 *
 * ### Cell width measurement
 *
 * The cell width is the maximum horizontal advance across all printable ASCII
 * glyphs (U+0020–U+007F).  This is computed by `measureMaxCellWidth()`, a
 * file-local helper that iterates the ASCII range, loads each glyph with
 * `FT_Load_Glyph`, and tracks the maximum `horiAdvance`.  If no ASCII glyph
 * is found (unusual), `face->size->metrics.max_advance` is used as a fallback.
 *
 * @note All methods run on the **MESSAGE THREAD**.
 *
 * @see jreng::Font::Metrics
 * @see jreng_font.h
 * @see jreng_font_shaping.cpp
 */

// Included via unity build (jreng_font.cpp) — jreng_glyph.h already in scope

#include <cmath>

#if JUCE_MAC
// CoreText implementation in jreng_font.mm
#else

namespace jreng
{

/**
 * @struct MaxCellMeasure
 * @brief Result of scanning ASCII glyphs for the maximum advance width.
 *
 * Holds both the pixel-rounded cell width and the raw 26.6 fixed-point advance
 * so that `calcMetrics()` can store both the logical and fixed representations
 * without re-converting.
 */
struct MaxCellMeasure { int maxCellW; FT_Pos maxW26_6; };

/**
 * @brief Scans printable ASCII glyphs to find the maximum horizontal advance.
 *
 * Iterates codepoints U+0020 (SPACE) through U+007F (DEL) inclusive.  For each
 * codepoint that has a glyph in the face's cmap, loads the glyph metrics via
 * `FT_Load_Glyph` (without rendering) and records the maximum `horiAdvance`.
 *
 * The result is used by `calcMetrics()` to set the terminal cell width.  Using
 * the maximum across all ASCII glyphs (rather than just the space advance)
 * ensures that no glyph overflows its cell in a fixed-width terminal grid.
 *
 * @par Why ASCII only?
 * The terminal grid is defined by the primary monospace font's ASCII metrics.
 * Non-ASCII characters (CJK, emoji) are handled separately as wide cells
 * (2× width) or rendered into oversized atlas slots.
 *
 * @param face  FreeType face to measure; must have a character size set.
 * @return `MaxCellMeasure` with the maximum cell width in pixels and 26.6
 *         fixed-point.  Both fields are 0 if no ASCII glyph was found.
 */
static MaxCellMeasure measureMaxCellWidth (FT_Face face) noexcept
{
    MaxCellMeasure result { 0, 0 };

    for (int code { 32 }; code <= 127; ++code)
    {
        const FT_UInt glyphIndex { FT_Get_Char_Index (face, static_cast<FT_UInt> (code)) };

        if (glyphIndex != 0)
        {
            const FT_Error loadError { FT_Load_Glyph (face, glyphIndex, FT_LOAD_DEFAULT) };

            if (loadError == 0)
            {
                const FT_Pos horiAdvance26_6 { face->glyph->metrics.horiAdvance };
                const int cellW { static_cast<int> (ceilf (static_cast<float> (horiAdvance26_6) / static_cast<float> (jreng::Font::ftFixedScale))) };

                if (cellW > result.maxCellW)
                {
                    result.maxCellW = cellW;
                    result.maxW26_6 = horiAdvance26_6;
                }
            }
        }
    }

    return result;
}

} // namespace jreng

/**
 * @brief Calculates cell geometry metrics for a given cell height.
 *
 * Derives the logical, physical, and fixed-point cell dimensions from the
 * active FreeType face.  Called by the renderer whenever the font size changes
 * (zoom in/out) or on first layout.
 *
 * ### Computation sequence
 *
 * 1. **Display scale** — `getDisplayScale()` reads the primary display's
 *    device-pixel ratio (e.g. `2.0f` on HiDPI).
 * 2. **Logical size** — `FT_Set_Char_Size` is called with `baseDpi` (96 on
 *    Linux/Windows) to obtain logical metrics.  `height26_6` is the requested
 *    cell height converted to 26.6 fixed-point via `roundFloatPxTo26_6`.
 * 3. **Ascender / height** — read from `face->size->metrics`:
 *    - `ascender` — distance from baseline to top of cell (26.6).
 *    - `height` — total line height including leading (26.6).
 * 4. **Cell width** — `measureMaxCellWidth()` scans ASCII glyphs for the
 *    maximum advance.  If no ASCII glyph is found, `max_advance` is used.
 * 5. **Fixed fields** — stored directly as 26.6 values for use by HarfBuzz
 *    and `FT_Set_Char_Size` callers.
 * 6. **Logical fields** — converted from 26.6 to pixels via `ceil26_6ToPx`.
 * 7. **Physical fields** — logical values multiplied by `displayScale` and
 *    truncated to integer device pixels.
 * 8. **Render size restore** — `FT_Set_Char_Size` is called again with
 *    `renderDpi = baseDpi * displayScale` so the face is left at the correct
 *    physical resolution for subsequent rasterization calls.
 *
 * @par Example (96 DPI, 2× HiDPI, 16px cell height)
 * @code
 * Metrics m = font.calcMetrics (16.0f);
 * // m.logicalCellW  = 9   (CSS pixels)
 * // m.physCellW     = 18  (device pixels at 2× scale)
 * // m.fixedCellWidth = 576 (9 * 64, in 26.6)
 * @endcode
 *
 * @param heightPx  Desired cell height in logical (CSS) pixels.
 * @return Populated `Metrics` struct.  `isValid()` returns `false` if the
 *         regular face is null or `FT_Set_Char_Size` fails.
 *
 * @note The face is left at the **render** (physical) size after this call.
 *       Callers that need logical metrics must not rely on the face state.
 *
 * @see jreng::Font::Metrics
 * @see measureMaxCellWidth
 * @see ceil26_6ToPx
 * @see roundFloatPxTo26_6
 * @see getDisplayScale
 */
jreng::Font::Metrics jreng::Font::calcMetrics (float heightPx) noexcept
{
    Metrics metrics;
    FT_Face face { getFace (Style::regular) };

    if (face != nullptr)
    {
        const float displayScale { getDisplayScale() };
        const FT_UInt renderDpi { static_cast<FT_UInt> (static_cast<float> (baseDpi) * displayScale) };
        const int height26_6 { roundFloatPxTo26_6 (heightPx) };

        // Set logical size (baseDpi) to read layout metrics.
        const FT_Error logicalSizeError { FT_Set_Char_Size (face, 0, height26_6, baseDpi, baseDpi) };

        if (logicalSizeError == 0)
        {
            const FT_Pos ascender26_6 { face->size->metrics.ascender };
            const FT_Pos h26_6 { face->size->metrics.height };

            const auto [maxCellW, maxW26_6] { jreng::measureMaxCellWidth (face) };

            if (maxCellW > 0)
            {
                // Primary path: use the measured maximum ASCII advance width.
                metrics.fixedCellWidth = maxW26_6;
                metrics.fixedCellHeight = h26_6;
                metrics.fixedBaseline = ascender26_6;
                metrics.logicalCellW = maxCellW;
                metrics.logicalCellH = ceil26_6ToPx (h26_6);
                metrics.logicalBaseline = ceil26_6ToPx (ascender26_6);
            }
            else
            {
                // Fallback: no ASCII glyph found — use max_advance from face metrics.
                const FT_Pos fallback26_6 { face->size->metrics.max_advance };
                metrics.fixedCellWidth = fallback26_6;
                metrics.fixedCellHeight = h26_6;
                metrics.fixedBaseline = ascender26_6;
                metrics.logicalCellW = ceil26_6ToPx (fallback26_6);
                metrics.logicalCellH = ceil26_6ToPx (h26_6);
                metrics.logicalBaseline = ceil26_6ToPx (ascender26_6);
            }

            // Scale logical → physical by the display device-pixel ratio.
            metrics.physCellW = static_cast<int> (static_cast<float> (metrics.logicalCellW) * displayScale);
            metrics.physCellH = static_cast<int> (static_cast<float> (metrics.logicalCellH) * displayScale);
            metrics.physBaseline = static_cast<int> (static_cast<float> (metrics.logicalBaseline) * displayScale);

            // Restore the face to render DPI so rasterizeToImage() uses physical resolution.
            FT_Set_Char_Size (face, 0, height26_6, renderDpi, renderDpi);
        }
    }

    return metrics;
}

#endif
