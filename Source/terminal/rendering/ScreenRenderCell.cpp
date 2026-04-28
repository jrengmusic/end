/**
 * @file ScreenRenderCell.cpp
 * @brief Per-cell rendering pipeline: colour resolution and cell dispatch.
 *
 * This translation unit implements `Screen::processCellForSnapshot()` and the
 * file-scope colour-resolution helpers it depends on.  It handles image cells,
 * hint-label overlay substitution, background quad emission, block-element
 * rendering, selection overlay, and link underlines.  Glyph shaping is
 * delegated to `buildCellInstance()` (implemented in `ScreenRenderGlyph.cpp`).
 *
 * ## Pipeline position
 *
 * ```
 * Screen::buildSnapshot()              [ScreenRender.cpp]
 *   └─ Screen::processCellForSnapshot()  ← THIS FILE
 *        ├─ resolveCellColors()
 *        ├─ emit background quad
 *        ├─ Screen::buildBlockRect()   [ScreenRenderGlyph.cpp]
 *        └─ Screen::buildCellInstance()  [ScreenRenderGlyph.cpp]
 * ```
 *
 * ## Colour resolution
 *
 * `resolveForeground()` and `resolveBackground()` map `Color` values to
 * `juce::Colour` using the active `Theme`.  SGR bold on ANSI colours 0–7
 * maps to the bright variants (indices 8–15).  SGR reverse swaps fg and bg.
 *
 * @see Screen.h
 * @see ScreenRender.cpp
 * @see ScreenRenderGlyph.cpp
 */

#include "Screen.h"
#include "../logic/SixelDecoder.h"


namespace Terminal
{ /*____________________________________________________________________________*/

// MESSAGE THREAD

// =========================================================================
// File-scope named constants
// =========================================================================

/// @brief Underline thickness as a fraction of the physical cell height.
static constexpr float underlineThicknessFraction { 0.06f };

/// @brief Brightness multiplier applied to the foreground colour of dim cells.
static constexpr float dimBrightnessFactor { 0.5f };

// =========================================================================
// Colour resolution helpers (file-scope)
// =========================================================================

/**
 * @brief Resolves the foreground colour of @p cell against @p theme.
 *
 * Applies the following rules in order:
 * - Default: `theme.defaultForeground`.
 * - Palette mode: ANSI index 0–7 is promoted to 8–15 when the cell is bold.
 *   Indices 0–15 map to `theme.ansi`; indices 16–255 map to `PALETTE`.
 * - RGB mode: direct `juce::Colour` from the cell's RGB components.
 *
 * @param cell   Cell whose `fg` colour and `isBold()` flag are used.
 * @param theme  Active colour theme.
 * @return       Resolved `juce::Colour` for the foreground.
 *
 * @see resolveCellColors()
 */
static juce::Colour resolveForeground (const Cell& cell, const Theme& theme) noexcept
{
    const Color& color { cell.fg };
    juce::Colour result { theme.defaultForeground };

    if (color.mode == Color::palette)
    {
        uint8_t idx { color.paletteIndex() };

        if (cell.isBold() and idx < 8)
        {
            idx = static_cast<uint8_t> (idx + 8);
        }

        if (idx < 16)
        {
            result = theme.ansi.at (idx);
        }
        else
        {
            result = resolvePalette (PALETTE.at (idx));
        }
    }
    else if (color.mode == Color::rgb)
    {
        result = juce::Colour (color.red, color.green, color.blue);
    }

    return result;
}

/**
 * @brief Resolves a background `Color` value against @p theme.
 *
 * Applies the following rules:
 * - Default: `theme.defaultBackground`.
 * - Palette mode: indices 0–15 map to `theme.ansi`; 16–255 map to `PALETTE`.
 * - RGB mode: direct `juce::Colour` from the colour's RGB components.
 *
 * @note Unlike `resolveForeground()`, bold does not affect background colour.
 *
 * @param color  Background `Color` value from the cell.
 * @param theme  Active colour theme.
 * @return       Resolved `juce::Colour` for the background.
 *
 * @see resolveCellColors()
 */
static juce::Colour resolveBackground (const Color& color, const Theme& theme) noexcept
{
    juce::Colour result { theme.defaultBackground };

    if (color.mode == Color::palette)
    {
        const uint8_t idx { color.paletteIndex() };

        if (idx < 16)
        {
            result = theme.ansi.at (idx);
        }
        else
        {
            result = resolvePalette (PALETTE.at (idx));
        }
    }
    else if (color.mode == Color::rgb)
    {
        result = juce::Colour (color.red, color.green, color.blue);
    }

    return result;
}

/**
 * @struct ResolvedColors
 * @brief Pair of resolved foreground and background colours for one cell.
 *
 * Produced by `resolveCellColors()` after applying SGR reverse-video.
 *
 * @see resolveCellColors()
 */
struct ResolvedColors
{
    juce::Colour fg; ///< Resolved foreground colour (after reverse-video swap if applicable).
    juce::Colour bg; ///< Resolved background colour (after reverse-video swap if applicable).
};

/**
 * @brief Resolves both foreground and background colours for @p cell.
 *
 * Calls `resolveForeground()` and `resolveBackground()`, then swaps them if
 * the cell has the SGR reverse-video attribute set.
 *
 * @param cell   Cell to resolve colours for.
 * @param theme  Active colour theme.
 * @return       `ResolvedColors` with fg and bg after reverse-video.
 */
static ResolvedColors resolveCellColors (const Cell& cell, const Theme& theme) noexcept
{
    ResolvedColors rc {};
    rc.fg = resolveForeground (cell, theme);
    rc.bg = resolveBackground (cell.bg, theme);

    if (cell.isInverse())
    {
        const juce::Colour temp { rc.fg };
        rc.fg = rc.bg;
        rc.bg = temp;
    }

    if (cell.isDim())
    {
        rc.fg = rc.fg.withMultipliedBrightness (dimBrightnessFactor);
    }

    return rc;
}

// =========================================================================
// Screen::processCellForSnapshot
// =========================================================================

/**
 * @brief Processes one cell and appends its contributions to the row caches.
 *
 * This is the per-cell entry point called by `buildSnapshot()` for every cell
 * in a dirty row.  It performs the following steps:
 *
 * 1. Resolves foreground and background colours via `resolveCellColors()`.
 * 2. If the background is non-default, emits a `Render::Background` quad.
 * 3. If `ligatureSkip > 0`, decrements it and skips glyph rendering (the
 *    cell is part of a ligature already emitted for a previous column).
 * 4. If the cell has content:
 *    - Block elements (U+2580–U+2593): emits a `Render::Background` quad via
 *      `buildBlockRect()`.
 *    - All other glyphs: calls `buildCellInstance()`.
 * 5. If the cell is within the active `ScreenSelection`, emits a selection
 *    overlay `Render::Background` quad using `Theme::selectionColour`.
 *
 * @param cell         The cell to render (read directly from `Grid`).
 * @param rowCells     Pointer to the start of the current row in `Grid` (for lookahead).
 * @param rowGraphemes Grapheme sidecar row pointer for the current row (may be `nullptr`).
 * @param col          Column index of the cell.
 * @param row          Row index of the cell.
 * @param grid         Terminal grid; used for image staging.
 *
 * @note **MESSAGE THREAD**.
 * @see buildCellInstance()
 * @see buildBlockRect()
 * @see ScreenSelection::contains()
 */
template <typename Renderer>
void Screen<Renderer>::processCellForSnapshot (
    const Cell& cell, const Cell* rowCells, const Grapheme* rowGraphemes,
    int col, int row, Grid& grid) noexcept
{
    // ---------------------------------------------------------------------------
    // Dispatch on cell layout type.
    //
    // Image cells (LAYOUT_IMAGE / LAYOUT_IMAGE_CONT) bypass all text shaping.
    // All other cells go through the full hint-override + shaping pipeline.
    // ---------------------------------------------------------------------------

    if (cell.isImageContinuation())
    {
        // Continuation cell: emit background quad only (if non-default bg).
        const juce::Colour bg { resolveBackground (cell.bg, resources.terminalColors) };

        if (bg != resources.terminalColors.defaultBackground)
        {
            const int bgIdx { row * bgCacheCols + bgCount[row] };
            Render::Background& bgQuad { cachedBg[bgIdx] };
            bgQuad.screenBounds = juce::Rectangle<float> {
                static_cast<float> (col * physCellWidth),
                static_cast<float> (row * physCellHeight),
                static_cast<float> (physCellWidth),
                static_cast<float> (physCellHeight) };
            bgQuad.backgroundColorR = bg.getFloatRed();
            bgQuad.backgroundColorG = bg.getFloatGreen();
            bgQuad.backgroundColorB = bg.getFloatBlue();
            bgQuad.backgroundColorA = bg.getFloatAlpha();
            ++bgCount[row];
        }
    }
    else if (cell.isImage())
    {
        // Head cell: emit background quad (if non-default bg) + full-image ImageQuad.
        const juce::Colour bg { resolveBackground (cell.bg, resources.terminalColors) };

        if (bg != resources.terminalColors.defaultBackground)
        {
            const int bgIdx { row * bgCacheCols + bgCount[row] };
            Render::Background& bgQuad { cachedBg[bgIdx] };
            bgQuad.screenBounds = juce::Rectangle<float> {
                static_cast<float> (col * physCellWidth),
                static_cast<float> (row * physCellHeight),
                static_cast<float> (physCellWidth),
                static_cast<float> (physCellHeight) };
            bgQuad.backgroundColorR = bg.getFloatRed();
            bgQuad.backgroundColorG = bg.getFloatGreen();
            bgQuad.backgroundColorB = bg.getFloatBlue();
            bgQuad.backgroundColorA = bg.getFloatAlpha();
            ++bgCount[row];
        }

        const uint32_t imageId { cell.codepoint };
        const ImageRegion* region { imageAtlas.lookup (imageId) };

        if (region == nullptr)
        {
            PendingImage* pending { grid.getDecodedImage (imageId) };

            if (pending != nullptr)
            {
                imageAtlas.stageWithId (pending->imageId, pending->rgba.get(),
                                        pending->width, pending->height);
                grid.releaseDecodedImage (imageId);
                region = imageAtlas.lookup (imageId);
            }
        }

        if (region != nullptr and imageCacheCount[row] < cacheCols)
        {
            const int imgIdx { row * cacheCols + imageCacheCount[row] };
            Render::ImageQuad& iq { cachedImages[imgIdx] };
            iq.screenBounds = juce::Rectangle<float> {
                static_cast<float> (col * physCellWidth),
                static_cast<float> (row * physCellHeight),
                static_cast<float> (region->widthPx),
                static_cast<float> (region->heightPx) };
            iq.uvRect = region->uv;

            ++imageCacheCount[row];
        }
    }
    else
    {

    // ---------------------------------------------------------------------------
    // Hint label override: substitute cell content at hint positions.
    //
    // When Open File mode is active, `hintOverlay` points to the `LinkSpan`
    // array.  For each cell that falls within a hint label's one- or two-
    // character badge, we build a local `Cell` copy with the hint character
    // as the codepoint and the theme hint colours as direct-RGB fg/bg.  The
    // remainder of `processCellForSnapshot` uses this copy transparently —
    // no additional draw calls or GL arrays are required.
    // ---------------------------------------------------------------------------

    Cell effectiveCell { cell };

    if (hintOverlay != nullptr)
    {
        for (int h { 0 }; h < hintOverlayCount; ++h)
        {
            const LinkSpan& span { hintOverlay[h] };

            if (span.hintLabel[0] != '\0'
                and row == span.row
                and col == span.labelCol)
            {
                effectiveCell.codepoint = static_cast<uint32_t> (span.hintLabel[0]);
                effectiveCell.width     = 1;
                effectiveCell.style     = Cell::BOLD;
                effectiveCell.layout    = 0;

                const juce::Colour& hintFg { resources.terminalColors.hintLabelFg };
                const juce::Colour& hintBg { resources.terminalColors.hintLabelBg };

                effectiveCell.fg.setRGB (
                    static_cast<uint8_t> (hintFg.getRed()),
                    static_cast<uint8_t> (hintFg.getGreen()),
                    static_cast<uint8_t> (hintFg.getBlue()));

                effectiveCell.bg.setRGB (
                    static_cast<uint8_t> (hintBg.getRed()),
                    static_cast<uint8_t> (hintBg.getGreen()),
                    static_cast<uint8_t> (hintBg.getBlue()));

                break;
            }
        }
    }

    const Grapheme* grapheme { nullptr };

    if (effectiveCell.hasGrapheme() and rowGraphemes != nullptr)
    {
        grapheme = &rowGraphemes[col];
    }

    const ResolvedColors resolved { resolveCellColors (effectiveCell, resources.terminalColors) };

    if (resolved.bg != resources.terminalColors.defaultBackground)
    {
        const int bgIdx { row * bgCacheCols + bgCount[row] };
        Render::Background& bg { cachedBg[bgIdx] };
        bg.screenBounds = juce::Rectangle<float> {
            static_cast<float> (col * physCellWidth),
            static_cast<float> (row * physCellHeight),
            static_cast<float> (physCellWidth),
            static_cast<float> (physCellHeight) };
        bg.backgroundColorR = resolved.bg.getFloatRed();
        bg.backgroundColorG = resolved.bg.getFloatGreen();
        bg.backgroundColorB = resolved.bg.getFloatBlue();
        bg.backgroundColorA = resolved.bg.getFloatAlpha();
        ++bgCount[row];
    }

    if (ligatureSkip > 0)
    {
        --ligatureSkip;
    }
    else if (effectiveCell.hasContent())
    {
        if (isBlockChar (effectiveCell.codepoint))
        {
            const int bgIdx { row * bgCacheCols + bgCount[row] };
            cachedBg[bgIdx] = buildBlockRect (effectiveCell.codepoint, col, row, resolved.fg);
            ++bgCount[row];
        }
        else
        {
            buildCellInstance (effectiveCell, grapheme, rowCells, col, row, resolved.fg);
        }
    }

    if (selection != nullptr and selection->containsCell (col, row))
    {
        const int bgIdx { row * bgCacheCols + bgCount[row] };
        const juce::Colour& sel { resources.terminalColors.selectionColour };
        Render::Background& bg { cachedBg[bgIdx] };
        bg.screenBounds = juce::Rectangle<float> {
            static_cast<float> (col * physCellWidth),
            static_cast<float> (row * physCellHeight),
            static_cast<float> (physCellWidth),
            static_cast<float> (physCellHeight) };
        bg.backgroundColorR = sel.getFloatRed();
        bg.backgroundColorG = sel.getFloatGreen();
        bg.backgroundColorB = sel.getFloatBlue();
        bg.backgroundColorA = sel.getFloatAlpha();
        ++bgCount[row];
    }

    if (linkUnderlay != nullptr)
    {
        for (int h { 0 }; h < linkUnderlayCount; ++h)
        {
            const LinkSpan& span { linkUnderlay[h] };

            if (row == span.row and col >= span.col and col < span.col + span.length)
            {
                const int bgIdx { row * bgCacheCols + bgCount[row] };
                const float thickness { std::max (1.0f, static_cast<float> (physCellHeight) * underlineThicknessFraction) };
                const float y { static_cast<float> (row * physCellHeight)
                                + static_cast<float> (physCellHeight) - thickness };

                Render::Background& bg { cachedBg[bgIdx] };
                bg.screenBounds = juce::Rectangle<float> {
                    static_cast<float> (col * physCellWidth),
                    y,
                    static_cast<float> (physCellWidth),
                    thickness };
                bg.backgroundColorR = resolved.fg.getFloatRed();
                bg.backgroundColorG = resolved.fg.getFloatGreen();
                bg.backgroundColorB = resolved.fg.getFloatBlue();
                bg.backgroundColorA = 0.5f;
                ++bgCount[row];
                break;
            }
        }
    }

    } // else: normal text / glyph cell (not LAYOUT_IMAGE / LAYOUT_IMAGE_CONT)
}

template void Screen<jam::Glyph::GLContext>::processCellForSnapshot (const Cell&, const Cell*, const Grapheme*, int, int, Grid&) noexcept;
template void Screen<jam::Glyph::GraphicsContext>::processCellForSnapshot (const Cell&, const Cell*, const Grapheme*, int, int, Grid&) noexcept;

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Terminal
