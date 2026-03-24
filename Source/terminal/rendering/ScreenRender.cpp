/**
 * @file ScreenRender.cpp
 * @brief Per-cell rendering pipeline: colour resolution, shaping, and glyph cache population.
 *
 * This translation unit implements the core per-frame rendering pipeline for
 * `Screen`.  It is responsible for converting `Grid` cell rows into per-row
 * arrays of `Render::Glyph` and `Render::Background` instances that are later
 * packed into a `Render::Snapshot` by `ScreenSnapshot.cpp`.
 *
 * ## Pipeline overview
 *
 * ```
 * Screen::buildSnapshot()
 *   └─ for each row (every frame):
 *        └─ Screen::processCellForSnapshot()   [per cell]
 *             ├─ resolveCellColors()           [colour resolution]
 *             ├─ emit background quad          [non-default bg]
 *             ├─ Screen::buildBlockRect()      [block elements U+2580–U+2593]
 *             ├─ Screen::buildCellInstance()   [all other glyphs]
 *             │    ├─ BoxDrawing procedural    [U+2500–U+259F]
 *             │    ├─ FontCollection fallback  [O(1) slot lookup]
 *             │    ├─ Screen::tryLigature()    [2–3 char ligatures]
 *             │    └─ Fonts::shapeText/Emoji() [HarfBuzz shaping]
 *             └─ emit selection overlay quad   [ScreenSelection hit test]
 * ```
 *
 * ## Colour resolution
 *
 * `resolveForeground()` and `resolveBackground()` map `Color` values to
 * `juce::Colour` using the active `Theme`.  SGR bold on ANSI colours 0–7
 * maps to the bright variants (indices 8–15).  SGR reverse swaps fg and bg.
 *
 * ## Glyph dispatch
 *
 * `buildCellInstance()` dispatches each cell through a priority chain:
 * 1. **Box-drawing** (U+2500–U+259F): procedural rasterisation via `BoxDrawing`.
 * 2. **FontCollection fallback**: O(1) slot lookup for codepoints not in the
 *    primary font.
 * 3. **Ligatures**: 2–3 character ASCII sequences shaped as a unit when
 *    `ligatureEnabled` is true.
 * 4. **Standard shaping**: single-codepoint or grapheme-cluster shaping via
 *    `Fonts::shapeText()` / `Fonts::shapeEmoji()`.
 *
 * @see Screen.h
 * @see ScreenSnapshot.cpp
 * @see FontCollection
 * @see GlyphAtlas
 * @see BoxDrawing
 * @see GlyphConstraint
 */

#include "Screen.h"
// BoxDrawing is now jreng::Glyph::BoxDrawing, available via JuceHeader → jreng_glyph
// GlyphConstraint is now jreng::Glyph::Constraint, available via JuceHeader → jreng_glyph


namespace Terminal
{ /*____________________________________________________________________________*/

// MESSAGE THREAD

// =========================================================================
// File-scope named constants
// =========================================================================

/// @brief Maximum number of codepoints in a grapheme cluster (1 primary + 7 extras).
static constexpr int maxGraphemeCodepoints { 8 };

/// @brief Exclusive upper bound for the ASCII range tested during shaping dispatch.
static constexpr uint32_t asciiCeiling { 128 };

/// @brief Maximum codepoint sequence length tried during ligature detection.
static constexpr int maxLigatureLength { 3 };

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
    ResolvedColors rc;
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
// Glyph emission helper (file-scope)
// =========================================================================

/**
 * @brief Emits shaped glyph instances into a per-row cache slot.
 *
 * For each `Fonts::Glyph` in @p shapedGlyphs, looks up or rasterises the
 * glyph in @p atlas, then writes a `Render::Glyph` instance into @p slot.
 * Advances `currentX` by `sg.xAdvance` after each glyph.
 *
 * @param shapedGlyphs   Array of shaped glyphs from HarfBuzz.
 * @param shapedCount    Number of elements in @p shapedGlyphs.
 * @param font           Configured jreng::Font with size, style, face context applied.
 * @param constraint     Nerd Font scaling / alignment constraint (inactive = default).
 * @param span           Number of cells the glyph spans horizontally (0 = default).
 * @param cellPixelX     Physical X origin of the cell (col * physCellWidth).
 * @param cellPixelY     Physical Y origin of the cell (row * physCellHeight).
 * @param physBaseline   Physical baseline offset from the cell top in pixels.
 * @param foreground     Resolved foreground colour.
 * @param slot           Pointer to the start of the row's glyph cache slot.
 * @param maxSlots       Maximum number of glyph instances that fit in @p slot.
 * @param count          In/out: current number of instances written; incremented per glyph.
 *
 * @note **MESSAGE THREAD**.
 * @see jreng::Font::getGlyph()
 * @see Screen::buildCellInstance()
 */
static void emitShapedGlyphsToCache (
    const jreng::Typeface::Glyph* shapedGlyphs, int shapedCount,
    jreng::Font& font,
    const jreng::Glyph::Constraint& constraint, uint8_t span,
    float cellPixelX, float cellPixelY,
    int physBaseline,
    const juce::Colour& foreground,
    Render::Glyph* slot, int maxSlots, int& count) noexcept
{
    float currentX { cellPixelX };

    for (int i { 0 }; i < shapedCount and count < maxSlots; ++i)
    {
        const jreng::Typeface::Glyph& sg { shapedGlyphs[i] };

        jreng::Glyph::Region* atlasGlyph { constraint.isActive()
            ? font.getGlyph (static_cast<uint16_t> (sg.glyphIndex), constraint, span)
            : font.getGlyph (static_cast<uint16_t> (sg.glyphIndex)) };

        if (atlasGlyph != nullptr)
        {
            const float ascender { static_cast<float> (physBaseline) };
            const float glyphX { currentX + static_cast<float> (atlasGlyph->bearingX) };
            const float glyphY { cellPixelY + ascender - static_cast<float> (atlasGlyph->bearingY) };

            Render::Glyph& instance { slot[count] };
            instance.screenPosition = juce::Point<float> { glyphX, glyphY };
            instance.glyphSize = juce::Point<float> {
                static_cast<float> (atlasGlyph->widthPixels),
                static_cast<float> (atlasGlyph->heightPixels) };
            instance.textureCoordinates = atlasGlyph->textureCoordinates;
            instance.foregroundColorR = foreground.getFloatRed();
            instance.foregroundColorG = foreground.getFloatGreen();
            instance.foregroundColorB = foreground.getFloatBlue();
            instance.foregroundColorA = foreground.getFloatAlpha();
            ++count;
        }

        currentX += sg.xAdvance;
    }
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
 * @param rowGraphemes Grapheme sidecar row pointer for the current row (may be `nullptr`).
 * @param col          Column index of the cell.
 * @param row          Row index of the cell.
 *
 * @note **MESSAGE THREAD**.
 * @see buildCellInstance()
 * @see buildBlockRect()
 * @see ScreenSelection::contains()
 */
void Screen::processCellForSnapshot (
    const Cell& cell, const Cell* rowCells, const Grapheme* rowGraphemes, int col, int row) noexcept
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

    if (selection != nullptr and effectiveCell.hasContent() and selection->containsCell (col, row))
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
}

// =========================================================================
// Screen::buildSnapshot
// =========================================================================

/**
 * @brief Rebuilds dirty rows in the per-row caches from `Grid` and calls `updateSnapshot()`.
 *
 * Calls `grid.consumeDirtyRows()` to obtain the set of rows that have changed
 * since the last call.  Only rows with their dirty bit set are reprocessed;
 * clean rows retain their previous-frame glyph and background caches.  For
 * each dirty row, reads cells and graphemes directly from `Grid` (scrollback
 * or active, depending on `state.getScrollOffset()`), resets the row's glyph
 * and background counts to zero, resets `ligatureSkip`, and calls
 * `processCellForSnapshot()` for every cell.  After all rows are processed,
 * calls `updateSnapshot()` to pack the caches into a `Render::Snapshot` and
 * publish it.
 *
 * @param state  Current terminal state (provides `getCols()`, `getVisibleRows()`,
 *               `getScrollOffset()`).
 * @param grid   Terminal grid (dirty bits consumed; cells read per dirty row).
 *
 * @note **MESSAGE THREAD**.
 * @see processCellForSnapshot()
 * @see updateSnapshot()
 */
void Screen::buildSnapshot (State& state, Grid& grid) noexcept
{
    const int cols { state.getCols() };
    const int rows { state.getVisibleRows() };
    const int offset { state.getScrollOffset() };
    const int maxGlyphs { cacheCols * 2 };

    // Always consume Grid dirty bits to keep them from accumulating.
    // If a full rebuild is requested, override with all-ones so every row
    // is processed — same initial state as a first-frame render.
    uint64_t dirtyBits[4] {};
    grid.consumeDirtyRows (dirtyBits);

    if (state.consumeFullRebuild())
    {
        std::memset (dirtyBits, 0xFF, sizeof (dirtyBits));
    }

    for (int r { 0 }; r < rows; ++r)
    {
        const int word { r >> 6 };
        const uint64_t bit { static_cast<uint64_t> (1) << (r & 63) };

        if ((dirtyBits[word] & bit) != 0)
        {
            const Cell* rowCells { offset > 0
                ? grid.scrollbackRow (r, offset)
                : grid.activeVisibleRow (r) };
            const Grapheme* rowGraphemes { offset > 0
                ? grid.scrollbackGraphemeRow (r, offset)
                : grid.activeVisibleGraphemeRow (r) };

            monoCount[r]  = 0;
            emojiCount[r] = 0;
            bgCount[r]    = 0;
            ligatureSkip  = 0;

            if (rowCells != nullptr)
            {
                for (int c { 0 }; c < cols; ++c)
                {
                    processCellForSnapshot (rowCells[c], rowCells, rowGraphemes, c, r);
                }
            }
        }
    }

    updateSnapshot (state, rows, maxGlyphs);
}

// =========================================================================
// Codepoint sequence builder (file-scope)
// =========================================================================

/**
 * @brief Builds the codepoint sequence for a cell, including grapheme extras.
 *
 * Writes the cell's primary codepoint to `codepoints[0]`.  If @p grapheme is
 * non-null and has extra codepoints, appends up to 7 of them.  Sets
 * @p codepointCount to the total number of codepoints written.
 *
 * @param cell            Cell providing the primary codepoint.
 * @param grapheme        Optional grapheme cluster; may be `nullptr`.
 * @param codepoints      Output array of at least 8 elements.
 * @param codepointCount  Output: number of codepoints written.
 *
 * @see Screen::buildCellInstance()
 */
static void buildCodepointSequence (const Cell& cell, const Grapheme* grapheme,
                                     uint32_t* codepoints, uint8_t& codepointCount) noexcept
{
    codepoints[0] = cell.codepoint;
    codepointCount = 1;

    if (grapheme != nullptr and grapheme->count > 0)
    {
        for (uint8_t i { 0 }; i < grapheme->count and i < maxGraphemeCodepoints - 1; ++i)
            codepoints[i + 1] = grapheme->extraCodepoints.at (i);
        codepointCount = static_cast<uint8_t> (1 + grapheme->count);
    }
}

// =========================================================================
// Screen::buildCellInstance
// =========================================================================

/**
 * @brief Shapes and rasterises one cell's glyph(s) into the row cache.
 *
 * Dispatches through the following priority chain:
 *
 * 1. **Box-drawing** (U+2500–U+259F via `BoxDrawing::isProcedural()`):
 *    rasterises procedurally via `GlyphAtlas::getOrRasterizeBoxDrawing()` and
 *    emits a mono `Render::Glyph` instance.
 *
 * 2. **FontCollection fallback** (non-emoji, non-box-drawing):
 *    calls `FontCollection::resolve()` to find the fallback slot.  If a slot
 *    is found and `hb_font_get_nominal_glyph()` succeeds, overrides the font
 *    handle and emits via `emitShapedGlyphsToCache()`.
 *
 * 3. **Ligature shaping** (ASCII, ligatures enabled, no FontCollection hit):
 *    calls `tryLigature()`.  If a ligature is found, sets `ligatureSkip` and
 *    returns.  Otherwise falls through to standard shaping.
 *
 * 4. **Standard shaping**: calls `Fonts::shapeText()` or `Fonts::shapeEmoji()`
 *    and emits via `emitShapedGlyphsToCache()` into `cachedMono` or
 *    `cachedEmoji` depending on `cell.isEmoji()`.
 *
 * @param cell       The cell to render.
 * @param grapheme   Optional grapheme cluster; may be `nullptr`.
 * @param rowCells   Pointer to the start of the current row in `Grid` (for lookahead).
 * @param col        Column index.
 * @param row        Row index.
 * @param foreground Resolved foreground colour.
 *
 * @note **MESSAGE THREAD**.
 * @see tryLigature()
 * @see emitShapedGlyphsToCache()
 * @see FontCollection::resolve()
 * @see BoxDrawing::isProcedural()
 */
void Screen::buildCellInstance (const Cell& cell,
                                const Grapheme* grapheme,
                                const Cell* rowCells,
                                int col, int row,
                                const juce::Colour& foreground) noexcept
{
    if (cell.codepoint != 0)
    {
        // Box-drawing uses direct atlas rasterisation (no HarfBuzz shaping, no GlyphRun).
        // This path is intentionally separate from emitShapedGlyphsToCache — merging
        // them would require fabricating a GlyphRun for a codepoint that was never shaped.
        if (jreng::Glyph::BoxDrawing::isProcedural (cell.codepoint))
        {
            jreng::Glyph::Region* atlasGlyph { font.getOrRasterizeBoxDrawing (
                cell.codepoint, physCellWidth, physCellHeight, physBaseline) };

            if (atlasGlyph != nullptr)
            {
                const int maxGlyphs { cacheCols * 2 };
                int& count { monoCount[row] };
                Render::Glyph* slot { cachedMono.get() + row * maxGlyphs };

                if (count < maxGlyphs)
                {
                    const float ascender { static_cast<float> (physBaseline) };
                    Render::Glyph& instance { slot[count] };
                    instance.screenPosition = juce::Point<float> {
                        static_cast<float> (col * physCellWidth),
                        static_cast<float> (row * physCellHeight) + ascender - static_cast<float> (atlasGlyph->bearingY) };
                    instance.glyphSize = juce::Point<float> {
                        static_cast<float> (atlasGlyph->widthPixels),
                        static_cast<float> (atlasGlyph->heightPixels) };
                    instance.textureCoordinates = atlasGlyph->textureCoordinates;
                    instance.foregroundColorR = foreground.getFloatRed();
                    instance.foregroundColorG = foreground.getFloatGreen();
                    instance.foregroundColorB = foreground.getFloatBlue();
                    instance.foregroundColorA = foreground.getFloatAlpha();
                    ++count;
                }
            }
        }
        else
        {
            const jreng::Typeface::Style style { selectFontStyle (cell) };
            void* fontHandle { font.getFontHandle (style) };

            if (fontHandle == nullptr)
            {
                fontHandle = font.getFontHandle (jreng::Typeface::Style::regular);
            }

            if (fontHandle != nullptr)
            {
                uint32_t codepoints[maxGraphemeCodepoints] { 0, 0, 0, 0, 0, 0, 0, 0 };
                uint8_t codepointCount { 0 };
                buildCodepointSequence (cell, grapheme, codepoints, codepointCount);

                const bool isEmoji { cell.isEmoji() };

                const jreng::Glyph::Constraint constraint { jreng::Glyph::getConstraint (cell.codepoint) };
                uint8_t cellSpan { 0 };

                if (constraint.isActive() and not isEmoji)
                {
                    cellSpan = 1;

                    if (constraint.maxCellSpan >= 2 and col + 1 < cacheCols and rowCells != nullptr)
                    {
                        const Cell& next { rowCells[col + 1] };

                        if (not next.hasContent() or next.codepoint == 0x20)
                        {
                            cellSpan = 2;
                        }
                    }
                }

                bool usedFontCollection { false };
                jreng::Typeface::Glyph fcGlyph;

                jreng::Typeface::Registry& fontRegistry { font.registry };

                const bool isBoxDrawing { cell.codepoint >= boxDrawingFirst and cell.codepoint <= boxDrawingLast };

                if (not isEmoji and not isBoxDrawing)
                {
                    const int8_t registrySlot { fontRegistry.resolve (cell.codepoint) };

                    if (registrySlot > 0)
                    {
                        const jreng::Typeface::Registry::Entry* entry { fontRegistry.getEntry (static_cast<int> (registrySlot)) };

                        if (entry != nullptr and entry->hbFont != nullptr)
                        {
                            uint32_t glyphId { 0 };

                            if (hb_font_get_nominal_glyph (entry->hbFont, cell.codepoint, &glyphId))
                            {
#if JUCE_MAC
                                if (entry->ctFont != nullptr)
                                {
                                    fontHandle = entry->ctFont;
                                }
#else
                                if (entry->ftFace != nullptr)
                                {
                                    fontHandle = static_cast<void*> (entry->ftFace);
                                }
#endif
                                fcGlyph.glyphIndex = glyphId;
                                fcGlyph.xOffset = 0.0f;
                                fcGlyph.yOffset = 0.0f;
                                fcGlyph.xAdvance = static_cast<float> (physCellWidth);
                                usedFontCollection = true;
                            }
                        }
                    }
                }

                if (usedFontCollection)
                {
                    const int maxGlyphs { cacheCols * 2 };
                    int& count { monoCount[row] };
                    Render::Glyph* slot { cachedMono.get() + row * maxGlyphs };

                    jreng::Font fontObj (font, baseFontSize, style);
                    jreng::Typeface::GlyphRun registryRun;
                    registryRun.fontHandle = fontHandle;
                    fontObj.applyGlyphRun (registryRun);

                    emitShapedGlyphsToCache (&fcGlyph, 1, fontObj,
                                             constraint, cellSpan,
                                             static_cast<float> (col * physCellWidth),
                                             static_cast<float> (row * physCellHeight),
                                             physBaseline, foreground,
                                             slot, maxGlyphs, count);
                }
                else if (ligatureEnabled and not isEmoji and cell.codepoint > 0 and cell.codepoint < asciiCeiling)
                {
                    const int skip { tryLigature (rowCells, col, row, style, foreground) };

                    if (skip > 0)
                    {
                        ligatureSkip = skip;
                    }
                    else
                    {
                        const jreng::Typeface::GlyphRun shaped { font.shapeText (style, codepoints,
                                                                                  static_cast<size_t> (codepointCount)) };

                        if (shaped.count > 0)
                        {
                            const int maxGlyphs { cacheCols * 2 };
                            int& count { monoCount[row] };
                            Render::Glyph* slot { cachedMono.get() + row * maxGlyphs };

                            jreng::Font fontObj (font, baseFontSize, style);

                            if (shaped.fontHandle != nullptr)
                            {
                                fontObj.applyGlyphRun (shaped);
                            }

                            emitShapedGlyphsToCache (shaped.glyphs, shaped.count, fontObj,
                                                     constraint, cellSpan,
                                                     static_cast<float> (col * physCellWidth),
                                                     static_cast<float> (row * physCellHeight),
                                                     physBaseline, foreground,
                                                     slot, maxGlyphs, count);
                        }
                    }
                }
                else
                {
                    const jreng::Typeface::GlyphRun shaped { isEmoji
                        ? font.shapeEmoji (codepoints, static_cast<size_t> (codepointCount))
                        : font.shapeText (style, codepoints, static_cast<size_t> (codepointCount)) };

                    if (shaped.count > 0)
                    {
                        const int maxGlyphs { cacheCols * 2 };
                        int& count { isEmoji ? emojiCount[row] : monoCount[row] };
                        Render::Glyph* slot { isEmoji
                            ? cachedEmoji.get() + row * maxGlyphs
                            : cachedMono.get() + row * maxGlyphs };

                        jreng::Font fontObj (font, baseFontSize, style);
                        fontObj.setEmoji (isEmoji);

                        if (not isEmoji and shaped.fontHandle != nullptr)
                        {
                            fontObj.applyGlyphRun (shaped);
                        }

                        emitShapedGlyphsToCache (shaped.glyphs, shaped.count, fontObj,
                                                 constraint, cellSpan,
                                                 static_cast<float> (col * physCellWidth),
                                                 static_cast<float> (row * physCellHeight),
                                                 physBaseline, foreground,
                                                 slot, maxGlyphs, count);
                    }
                }
            }
        } // else (not box drawing)
    }
}

// =========================================================================
// Screen::tryLigature
// =========================================================================

/**
 * @brief Attempts to shape a 2- or 3-character ligature starting at @p col.
 *
 * Tries sequence lengths 3 then 2 (longest match first).  For each length,
 * checks that all cells in the run are:
 * - ASCII (codepoint < 128, non-zero).
 * - Same SGR style as the first cell.
 * - Not emoji, not grapheme clusters, not wide-continuation cells.
 *
 * If the run is eligible, calls `Fonts::shapeText()`.  If HarfBuzz produces
 * fewer output glyphs than input codepoints, the sequence is a ligature.
 * The ligature glyphs are emitted with a fixed `xAdvance` of `physCellWidth`
 * per input cell, and the number of cells to skip is returned.
 *
 * @param rowCells    Pointer to the start of the current row in `Grid`.
 * @param col         Starting column.
 * @param row         Row index.
 * @param style       Font style for shaping.
 * @param foreground  Resolved foreground colour.
 * @return            Number of subsequent cells to skip (0 if no ligature found).
 *
 * @note **MESSAGE THREAD**.
 * @see buildCellInstance()
 * @see emitShapedGlyphsToCache()
 */
int Screen::tryLigature (const Cell* rowCells, int col, int row, jreng::Typeface::Style style,
                         const juce::Colour& foreground) noexcept
{
    int result { 0 };

    if (rowCells != nullptr)
    {
        for (int tryLen { maxLigatureLength }; tryLen >= 2 and result == 0; --tryLen)
        {
            if (col + tryLen <= cacheCols)
            {
                bool eligible { true };
                uint32_t codepoints[maxLigatureLength];
                const uint8_t baseStyle { rowCells[col].style };

                for (int i { 0 }; i < tryLen and eligible; ++i)
                {
                    const Cell& c { rowCells[col + i] };

                    if (c.codepoint == 0 or c.codepoint >= asciiCeiling
                        or c.style != baseStyle
                        or c.isEmoji() or c.hasGrapheme() or c.isWideContinuation())
                    {
                        eligible = false;
                    }
                    else
                    {
                        codepoints[i] = c.codepoint;
                    }
                }

                if (eligible)
                {
                    const jreng::Typeface::GlyphRun shaped { font.shapeText (style, codepoints,
                                                                            static_cast<size_t> (tryLen)) };

                    if (shaped.count > 0 and shaped.count < tryLen)
                    {
                        const int maxGlyphs { cacheCols * 2 };
                        int& count { monoCount[row] };
                        Render::Glyph* slot { cachedMono.get() + row * maxGlyphs };

                        jreng::Font fontObj (font, baseFontSize, style);

                        if (shaped.fontHandle != nullptr)
                        {
                            fontObj.applyGlyphRun (shaped);
                        }

                        jreng::Typeface::Glyph fixedGlyphs[maxLigatureLength];

                        for (int i { 0 }; i < shaped.count; ++i)
                        {
                            fixedGlyphs[i] = shaped.glyphs[i];
                            fixedGlyphs[i].xAdvance = static_cast<float> (physCellWidth);
                        }

                        const jreng::Glyph::Constraint noConstraint;

                        emitShapedGlyphsToCache (fixedGlyphs, shaped.count, fontObj,
                                                 noConstraint, 0,
                                                 static_cast<float> (col * physCellWidth),
                                                 static_cast<float> (row * physCellHeight),
                                                 physBaseline, foreground,
                                                 slot, maxGlyphs, count);

                        result = tryLen - 1;
                    }
                }
            }
        }
    }

    return result;
}

// =========================================================================
// Screen::buildBlockRect
// =========================================================================

/**
 * @brief Builds a `Render::Background` quad for a block-element character.
 *
 * Looks up the `BlockGeometry` for @p codepoint in `blockTable` (indexed by
 * `codepoint - blockFirst`) and scales the normalised geometry by the
 * physical cell dimensions.  The fill colour is @p foreground; if the
 * geometry's `alpha` field is non-negative it overrides the foreground alpha.
 *
 * @param codepoint   Block-element codepoint (U+2580–U+2593).
 * @param col         Column index.
 * @param row         Row index.
 * @param foreground  Resolved foreground colour used as the block fill.
 * @return            A fully populated `Render::Background` quad in physical pixel space.
 *
 * @note **MESSAGE THREAD**.
 * @see blockTable
 * @see isBlockChar()
 * @see processCellForSnapshot()
 */
Render::Background Screen::buildBlockRect (uint32_t codepoint, int col, int row, const juce::Colour& foreground) const noexcept
{
    const BlockGeometry& g { blockTable.at (codepoint - blockFirst) };
    const float cx { static_cast<float> (col * physCellWidth) };
    const float cy { static_cast<float> (row * physCellHeight) };
    const float cw { static_cast<float> (physCellWidth) };
    const float ch { static_cast<float> (physCellHeight) };

    Render::Background bg;
    bg.screenBounds = { cx + g.x * cw, cy + g.y * ch, g.w * cw, g.h * ch };
    bg.backgroundColorR = foreground.getFloatRed();
    bg.backgroundColorG = foreground.getFloatGreen();
    bg.backgroundColorB = foreground.getFloatBlue();
    bg.backgroundColorA = (g.alpha >= 0.0f) ? g.alpha : foreground.getFloatAlpha();
    return bg;
}

// =========================================================================
// Screen::selectFontStyle
// =========================================================================

/**
 * @brief Selects the `Fonts::Style` variant for a cell based on its SGR attributes.
 *
 * Maps the cell's bold and italic flags to the corresponding `Fonts::Style`
 * enum value.  Returns `boldItalic` if both flags are set, `bold` if only
 * bold, `italic` if only italic, and `regular` otherwise.
 *
 * @param cell  Cell whose `isBold()` and `isItalic()` flags are tested.
 * @return      `Fonts::Style::boldItalic`, `bold`, `italic`, or `regular`.
 *
 * @see buildCellInstance()
 */
jreng::Typeface::Style Screen::selectFontStyle (const Cell& cell) noexcept
{
    jreng::Typeface::Style result { jreng::Typeface::Style::regular };

    if (cell.isBold() and cell.isItalic())
    {
        result = jreng::Typeface::Style::boldItalic;
    }
    else if (cell.isBold())
    {
        result = jreng::Typeface::Style::bold;
    }
    else if (cell.isItalic())
    {
        result = jreng::Typeface::Style::italic;
    }

    return result;
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Terminal
