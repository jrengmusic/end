#pragma once
#include <JuceHeader.h>

namespace Terminal
{ /*____________________________________________________________________________*/

/**
 * @struct CellMetrics
 * @brief Terminal-specific cell dimensions computed from generic font metrics.
 *
 * Derives the fixed-size cell rectangle used for monospace grid layout from
 * jam::Typeface's generic (size-independent) metrics.  Two coordinate spaces:
 * - Logical (CSS pixels) — for layout.
 * - Physical (device pixels) — for rasterization.
 *
 * @see jam::Typeface::Metrics
 * @see CellMetrics::compute
 */
struct CellMetrics
{
    int cellWidth      { 0 }; ///< Cell width in logical pixels.
    int cellHeight     { 0 }; ///< Cell height in logical pixels.
    int baseline       { 0 }; ///< Ascent from top of cell in logical pixels.
    int physCellWidth  { 0 }; ///< Cell width in physical (device) pixels.
    int physCellHeight { 0 }; ///< Cell height in physical (device) pixels.

    /** @brief Returns true when the cell dimensions are non-zero. */
    bool isValid() const noexcept { return cellWidth > 0 and cellHeight > 0; }

    /**
     * @brief Computes cell metrics from a typeface at the given font size.
     *
     * Derives cell width as the maximum advance across printable ASCII
     * (U+0020–U+007F), and cell height as ascent + descent + leading.
     * Falls back to @p fontSize for cell width when no ASCII glyph advance
     * is available.
     *
     * @param typeface      The resolved typeface to measure.
     * @param fontSize      Desired font size in CSS/point pixels.
     * @param displayScale  Physical-to-logical pixel ratio (e.g. 2.0f on Retina).
     * @return Computed cell metrics; `isValid()` returns false if typeface metrics
     *         are unavailable.
     */
    static CellMetrics compute (jam::Typeface& typeface,
                                float fontSize,
                                float displayScale) noexcept
    {
        CellMetrics result;
        const auto metrics { typeface.getMetrics() };

        if (metrics.isValid())
        {
            const float ascent  { metrics.ascent  * fontSize };
            const float descent { metrics.descent * fontSize };
            const float leading { metrics.leading * fontSize };
            const float lineH   { ascent + descent + leading };

            float maxAdvance { 0.0f };

            for (uint32_t code { 32 }; code <= 127; ++code)
            {
                const float advance { typeface.getAdvanceWidth (code) * fontSize };

                if (advance > maxAdvance)
                {
                    maxAdvance = advance;
                }
            }

            if (maxAdvance <= 0.0f)
            {
                maxAdvance = fontSize;
            }

            result.cellWidth      = static_cast<int> (std::ceil (maxAdvance));
            result.cellHeight     = static_cast<int> (std::ceil (lineH));
            result.baseline       = static_cast<int> (std::ceil (ascent));
            result.physCellWidth  = static_cast<int> (static_cast<float> (result.cellWidth)  * displayScale);
            result.physCellHeight = static_cast<int> (static_cast<float> (result.cellHeight) * displayScale);
        }

        return result;
    }
};

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Terminal
