/**
 * @file ScreenRenderGlyph.cpp
 * @brief Glyph shaping and rasterisation: box-drawing, ligatures, and HarfBuzz dispatch.
 *
 * This translation unit implements the glyph-level rendering methods of `Screen`:
 * `buildCellInstance()`, `tryLigature()`, `buildBlockRect()`, and
 * `selectFontStyle()`, together with the file-scope helpers they share
 * (`emitShapedGlyphsToCache()`, `buildCodepointSequence()`).
 *
 * ## Pipeline position
 *
 * ```
 * Screen::processCellForSnapshot()     [ScreenRenderCell.cpp]
 *   ├─ Screen::buildBlockRect()          ← THIS FILE
 *   └─ Screen::buildCellInstance()       ← THIS FILE
 *        ├─ buildCodepointSequence()
 *        ├─ Screen::tryLigature()        ← THIS FILE
 *        │    └─ emitShapedGlyphsToCache()
 *        └─ emitShapedGlyphsToCache()
 * ```
 *
 * ## Glyph dispatch priority
 *
 * 1. **Box-drawing** (U+2500–U+259F): procedural rasterisation via
 *    `jam::Glyph::BoxDrawing`.
 * 2. **FontCollection fallback**: O(1) slot lookup for codepoints not in the
 *    primary font.
 * 3. **Ligatures**: 2–3 character ASCII sequences shaped as a unit when
 *    `ligatureEnabled` is true.
 * 4. **Standard shaping**: single-codepoint or grapheme-cluster shaping via
 *    `Fonts::shapeText()` / `Fonts::shapeEmoji()`.
 *
 * @see Screen.h
 * @see ScreenRenderCell.cpp
 * @see FontCollection
 * @see gl::GlyphAtlas
 * @see BoxDrawing
 */

#include "Screen.h"
#include "../logic/SixelDecoder.h"


namespace Terminal
{ /*____________________________________________________________________________*/

// File-scope named constants

/// @brief Maximum number of codepoints in a grapheme cluster (1 primary + 7 extras).
static constexpr int maxGraphemeCodepoints { 8 };

/// @brief Exclusive upper bound for the ASCII range tested during shaping dispatch.
static constexpr uint32_t asciiCeiling { 128 };

/// @brief Maximum codepoint sequence length tried during ligature detection.
static constexpr int maxLigatureLength { 3 };

// Glyph emission helper (file-scope)

/**
 * @brief Emits shaped glyph instances into a per-row cache slot.
 *
 * For each `Typeface::Glyph` in @p shapedGlyphs, looks up or rasterises the
 * glyph in @p packer, then writes a `Render::Glyph` instance into @p slot.
 * Advances `currentX` by `sg.xAdvance` after each glyph.
 *
 * @param shapedGlyphs   Array of shaped glyphs from HarfBuzz.
 * @param shapedCount    Number of elements in @p shapedGlyphs.
 * @param packer         Glyph packer owning the atlas and rasterization.
 * @param fontHandle     Opaque platform font handle (CTFontRef / FT_Face).
 * @param isEmoji        True to rasterize into the RGBA emoji atlas.
 * @param fontSize       Logical font size in points; used as Key::fontSize.
 * @param constraint     Nerd Font scaling / alignment constraint (inactive = default).
 * @param span           Number of cells the glyph spans horizontally (0 = default).
 * @param cellPixelX     Physical X origin of the cell (col * physCellWidth).
 * @param cellPixelY     Physical Y origin of the cell (row * physCellHeight).
 * @param physCellWidth  Physical cell width in pixels.
 * @param physCellHeight Physical cell height in pixels.
 * @param physBaseline   Physical baseline offset from the cell top in pixels.
 * @param foreground     Resolved foreground colour.
 * @param slot           Pointer to the start of the row's glyph cache slot.
 * @param maxSlots       Maximum number of glyph instances that fit in @p slot.
 * @param count          In/out: current number of instances written; incremented per glyph.
 *
 * @note **MESSAGE THREAD**.
 * @see jam::Glyph::Packer::getOrRasterize()
 * @see Screen::buildCellInstance()
 */
static void emitShapedGlyphsToCache (
    const jam::Typeface::Glyph* shapedGlyphs, int shapedCount,
    jam::Glyph::Packer& packer,
    void* fontHandle, bool isEmoji, float fontSize,
    const jam::Glyph::Constraint& constraint, uint8_t span,
    float cellPixelX, float cellPixelY,
    int physCellWidth, int physCellHeight, int physBaseline,
    const juce::Colour& foreground,
    Render::Glyph* slot, int maxSlots, int& count) noexcept
{
    float currentX { cellPixelX };

    for (int i { 0 }; i < shapedCount and count < maxSlots; ++i)
    {
        const jam::Typeface::Glyph& sg { shapedGlyphs[i] };

        const uint8_t effectiveSpan { (constraint.isActive() or span > 1)
            ? std::max (span, static_cast<uint8_t> (1))
            : static_cast<uint8_t> (0) };

        jam::Glyph::Key key;
        key.glyphIndex = static_cast<uint32_t> (sg.glyphIndex);
        key.fontFace   = fontHandle;
        key.fontSize   = fontSize;
        key.span       = effectiveSpan;

        jam::Glyph::Region* atlasGlyph { packer.getOrRasterize (key, fontHandle, isEmoji,
                                                                 constraint,
                                                                 physCellWidth, physCellHeight,
                                                                 physBaseline) };

        if (atlasGlyph != nullptr)
        {
            const float ascender { static_cast<float> (physBaseline) };
            const float glyphX { currentX + sg.xOffset + static_cast<float> (atlasGlyph->bearingX) };
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

// Codepoint sequence builder (file-scope)

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

// Screen::buildCellInstance

/**
 * @brief Shapes and rasterises one cell's glyph(s) into the row cache.
 *
 * Dispatches through the following priority chain:
 *
 * 1. **Box-drawing** (U+2500–U+259F via `BoxDrawing::isProcedural()`):
 *    rasterises procedurally via `gl::GlyphAtlas::getOrRasterizeBoxDrawing()` and
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
template <typename Renderer>
void Screen<Renderer>::buildCellInstance (const Cell& cell,
                                const Grapheme* grapheme,
                                const Cell* rowCells,
                                int col, int row,
                                const juce::Colour& foreground) noexcept
{
    if (cell.codepoint != 0)
    {
        if (jam::Glyph::BoxDrawing::isProcedural (cell.codepoint))
        {
            jam::Glyph::Region* atlasGlyph { packer.getOrRasterizeBoxDrawing (
                cell.codepoint, physCellWidth, physCellHeight, physBaseline) };

            if (atlasGlyph != nullptr)
            {
                int& count { monoCount[row] };
                Render::Glyph* slot { cachedMono.get() + row * maxGlyphsPerRow };

                if (count < maxGlyphsPerRow)
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
            const jam::Typeface::Style style { selectFontStyle (cell) };
            void* fontHandle { font.getResolvedTypeface()->getFontHandle (style) };

            if (fontHandle == nullptr)
            {
                fontHandle = font.getResolvedTypeface()->getFontHandle (jam::Typeface::Style::regular);
            }

            if (fontHandle != nullptr)
            {
                uint32_t codepoints[maxGraphemeCodepoints] { 0, 0, 0, 0, 0, 0, 0, 0 };
                uint8_t codepointCount { 0 };
                buildCodepointSequence (cell, grapheme, codepoints, codepointCount);

                const bool isEmoji { cell.isEmoji() };

                const jam::Glyph::Constraint constraint { jam::Glyph::getConstraint (cell.codepoint) };
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

                if (ligatureEnabled and not isEmoji and cell.codepoint > 0 and cell.codepoint < asciiCeiling)
                {
                    const int skip { tryLigature (rowCells, col, row, style, foreground) };

                    if (skip > 0)
                    {
                        ligatureSkip = skip;
                    }
                    else
                    {
                        const jam::Typeface::GlyphRun shaped { font.getResolvedTypeface()->shapeText (style, codepoints,
                                                                                  static_cast<size_t> (codepointCount)) };

                        if (shaped.count > 0)
                        {
                            int& count { monoCount[row] };
                            Render::Glyph* slot { cachedMono.get() + row * maxGlyphsPerRow };

                            void* effectiveFontHandle { shaped.fontHandle != nullptr
                                ? shaped.fontHandle : fontHandle };

                            emitShapedGlyphsToCache (shaped.glyphs, shaped.count, packer,
                                                     effectiveFontHandle, false, baseFontSize,
                                                     constraint, cellSpan,
                                                     static_cast<float> (col * physCellWidth),
                                                     static_cast<float> (row * physCellHeight),
                                                     physCellWidth, physCellHeight,
                                                     physBaseline, foreground,
                                                     slot, maxGlyphsPerRow, count);
                        }
                    }
                }
                else
                {
                    const jam::Typeface::GlyphRun shaped { isEmoji
                        ? font.getResolvedTypeface()->shapeEmoji (codepoints, static_cast<size_t> (codepointCount))
                        : font.getResolvedTypeface()->shapeText (style, codepoints, static_cast<size_t> (codepointCount)) };

                    if (shaped.count > 0)
                    {
                        int& count { isEmoji ? emojiCount[row] : monoCount[row] };
                        Render::Glyph* slot { isEmoji
                            ? cachedEmoji.get() + row * maxGlyphsPerRow
                            : cachedMono.get() + row * maxGlyphsPerRow };

                        void* shapeHandle { isEmoji
                            ? font.getResolvedTypeface()->getEmojiFontHandle()
                            : (shaped.fontHandle != nullptr ? shaped.fontHandle : fontHandle) };

                        emitShapedGlyphsToCache (shaped.glyphs, shaped.count, packer,
                                                 shapeHandle, isEmoji, baseFontSize,
                                                 constraint, cellSpan,
                                                 static_cast<float> (col * physCellWidth),
                                                 static_cast<float> (row * physCellHeight),
                                                 physCellWidth, physCellHeight,
                                                 physBaseline, foreground,
                                                 slot, maxGlyphsPerRow, count);
                    }
                }
            }
        } // else (not box drawing)
    }
}

// Screen::tryLigature

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
template <typename Renderer>
int Screen<Renderer>::tryLigature (const Cell* rowCells, int col, int row, jam::Typeface::Style style,
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
                    const jam::Typeface::GlyphRun shaped { font.getResolvedTypeface()->shapeText (style, codepoints,
                                                                            static_cast<size_t> (tryLen)) };

                    if (shaped.count == 1)
                    {
                        int& count { monoCount[row] };
                        Render::Glyph* slot { cachedMono.get() + row * maxGlyphsPerRow };

                        void* ligFontHandle { shaped.fontHandle != nullptr
                            ? shaped.fontHandle
                            : font.getResolvedTypeface()->getFontHandle (style) };

                        jam::Typeface::Glyph fixedGlyphs[maxLigatureLength];

                        for (int i { 0 }; i < shaped.count; ++i)
                        {
                            fixedGlyphs[i] = shaped.glyphs[i];
                            fixedGlyphs[i].xAdvance = static_cast<float> (physCellWidth);
                        }

                        const jam::Glyph::Constraint noConstraint;

                        emitShapedGlyphsToCache (fixedGlyphs, shaped.count, packer,
                                                 ligFontHandle, false, baseFontSize,
                                                 noConstraint, static_cast<uint8_t> (tryLen),
                                                 static_cast<float> (col * physCellWidth),
                                                 static_cast<float> (row * physCellHeight),
                                                 physCellWidth, physCellHeight,
                                                 physBaseline, foreground,
                                                 slot, maxGlyphsPerRow, count);

                        result = tryLen - 1;
                    }
                }
            }
        }
    }

    return result;
}

// Screen::buildBlockRect

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
template <typename Renderer>
Render::Background Screen<Renderer>::buildBlockRect (uint32_t codepoint, int col, int row, const juce::Colour& foreground) const noexcept
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

// Screen::selectFontStyle

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
template <typename Renderer>
jam::Typeface::Style Screen<Renderer>::selectFontStyle (const Cell& cell) noexcept
{
    jam::Typeface::Style result { jam::Typeface::Style::regular };

    if (cell.isBold() and cell.isItalic())
    {
        result = jam::Typeface::Style::boldItalic;
    }
    else if (cell.isBold())
    {
        result = jam::Typeface::Style::bold;
    }
    else if (cell.isItalic())
    {
        result = jam::Typeface::Style::italic;
    }

    return result;
}

template void Screen<jam::Glyph::GLContext>::buildCellInstance (const Cell&, const Grapheme*, const Cell*, int, int, const juce::Colour&) noexcept;
template void Screen<jam::Glyph::GraphicsContext>::buildCellInstance (const Cell&, const Grapheme*, const Cell*, int, int, const juce::Colour&) noexcept;

template int Screen<jam::Glyph::GLContext>::tryLigature (const Cell*, int, int, jam::Typeface::Style, const juce::Colour&) noexcept;
template int Screen<jam::Glyph::GraphicsContext>::tryLigature (const Cell*, int, int, jam::Typeface::Style, const juce::Colour&) noexcept;

template Render::Background Screen<jam::Glyph::GLContext>::buildBlockRect (uint32_t, int, int, const juce::Colour&) const noexcept;
template Render::Background Screen<jam::Glyph::GraphicsContext>::buildBlockRect (uint32_t, int, int, const juce::Colour&) const noexcept;

template jam::Typeface::Style Screen<jam::Glyph::GLContext>::selectFontStyle (const Cell&) noexcept;
template jam::Typeface::Style Screen<jam::Glyph::GraphicsContext>::selectFontStyle (const Cell&) noexcept;

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Terminal
