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
 * @see gl::GlyphAtlas
 * @see BoxDrawing
 * @see GlyphConstraint
 */

#include "Screen.h"
#include "../logic/SixelDecoder.h"


namespace Terminal
{ /*____________________________________________________________________________*/

// MESSAGE THREAD

// =========================================================================
// File-scope named constants
// =========================================================================

/// @brief Maximum number of codepoints in a grapheme cluster (1 primary + 7 extras).
static constexpr int maxGraphemeCodepoints { 8 };

/// @brief Kitty unicode placeholder codepoint (U+10EEEE).
static constexpr uint32_t kittyPlaceholderChar { 0x10EEEE };

// =========================================================================
// Kitty diacritic mapping
// =========================================================================

/**
 * @brief Maps a Unicode combining diacritic codepoint to its Kitty row/column number.
 *
 * Translates the full diacritic table from kitty/rowcolumn-diacritics.c.
 * Returns 0 when @p cp is not a known diacritic (0 is never a valid Kitty
 * row/column encoding — the table starts at 1 for U+0305).
 *
 * @param cp  Unicode codepoint of the combining diacritic.
 * @return    1-based Kitty row/column number, or 0 if not a known diacritic.
 */
static int kittyDiacriticToNum (uint32_t cp) noexcept
{
    int result { 0 };

    if      (cp >= 0x0305 and cp <= 0x0306) result = static_cast<int> (cp - 0x0305) + 1;
    else if (cp >= 0x030D and cp <= 0x030F) result = static_cast<int> (cp - 0x030D) + 2;
    else if (cp >= 0x0310 and cp <= 0x0311) result = static_cast<int> (cp - 0x0310) + 4;
    else if (cp >= 0x0312 and cp <= 0x0313) result = static_cast<int> (cp - 0x0312) + 5;
    else if (cp >= 0x033D and cp <= 0x0340) result = static_cast<int> (cp - 0x033D) + 6;
    else if (cp >= 0x0346 and cp <= 0x0347) result = static_cast<int> (cp - 0x0346) + 9;
    else if (cp >= 0x034A and cp <= 0x034D) result = static_cast<int> (cp - 0x034A) + 10;
    else if (cp >= 0x0350 and cp <= 0x0353) result = static_cast<int> (cp - 0x0350) + 13;
    else if (cp >= 0x0357 and cp <= 0x0358) result = static_cast<int> (cp - 0x0357) + 16;
    else if (cp >= 0x035B and cp <= 0x035C) result = static_cast<int> (cp - 0x035B) + 17;
    else if (cp >= 0x0363 and cp <= 0x0370) result = static_cast<int> (cp - 0x0363) + 18;
    else if (cp >= 0x0483 and cp <= 0x0488) result = static_cast<int> (cp - 0x0483) + 31;
    else if (cp >= 0x0592 and cp <= 0x0596) result = static_cast<int> (cp - 0x0592) + 36;
    else if (cp >= 0x0597 and cp <= 0x059A) result = static_cast<int> (cp - 0x0597) + 40;
    else if (cp >= 0x059C and cp <= 0x05A2) result = static_cast<int> (cp - 0x059C) + 43;
    else if (cp >= 0x05A8 and cp <= 0x05AA) result = static_cast<int> (cp - 0x05A8) + 49;
    else if (cp >= 0x05AB and cp <= 0x05AD) result = static_cast<int> (cp - 0x05AB) + 51;
    else if (cp >= 0x05AF and cp <= 0x05B0) result = static_cast<int> (cp - 0x05AF) + 53;
    else if (cp >= 0x05C4 and cp <= 0x05C5) result = static_cast<int> (cp - 0x05C4) + 54;
    else if (cp >= 0x0610 and cp <= 0x0618) result = static_cast<int> (cp - 0x0610) + 55;
    else if (cp >= 0x0657 and cp <= 0x065C) result = static_cast<int> (cp - 0x0657) + 63;
    else if (cp >= 0x065D and cp <= 0x065F) result = static_cast<int> (cp - 0x065D) + 68;
    else if (cp >= 0x06D6 and cp <= 0x06DD) result = static_cast<int> (cp - 0x06D6) + 70;
    else if (cp >= 0x06DF and cp <= 0x06E3) result = static_cast<int> (cp - 0x06DF) + 77;
    else if (cp >= 0x06E4 and cp <= 0x06E5) result = static_cast<int> (cp - 0x06E4) + 81;
    else if (cp >= 0x06E7 and cp <= 0x06E9) result = static_cast<int> (cp - 0x06E7) + 82;
    else if (cp >= 0x06EB and cp <= 0x06ED) result = static_cast<int> (cp - 0x06EB) + 84;
    else if (cp >= 0x0730 and cp <= 0x0731) result = static_cast<int> (cp - 0x0730) + 86;
    else if (cp >= 0x0732 and cp <= 0x0734) result = static_cast<int> (cp - 0x0732) + 87;
    else if (cp >= 0x0735 and cp <= 0x0737) result = static_cast<int> (cp - 0x0735) + 89;
    else if (cp >= 0x073A and cp <= 0x073B) result = static_cast<int> (cp - 0x073A) + 91;
    else if (cp >= 0x073D and cp <= 0x073E) result = static_cast<int> (cp - 0x073D) + 92;
    else if (cp >= 0x073F and cp <= 0x0742) result = static_cast<int> (cp - 0x073F) + 93;
    else if (cp >= 0x0743 and cp <= 0x0744) result = static_cast<int> (cp - 0x0743) + 96;
    else if (cp >= 0x0745 and cp <= 0x0746) result = static_cast<int> (cp - 0x0745) + 97;
    else if (cp >= 0x0747 and cp <= 0x0748) result = static_cast<int> (cp - 0x0747) + 98;
    else if (cp >= 0x0749 and cp <= 0x074B) result = static_cast<int> (cp - 0x0749) + 99;
    else if (cp >= 0x07EB and cp <= 0x07F2) result = static_cast<int> (cp - 0x07EB) + 101;
    else if (cp >= 0x07F3 and cp <= 0x07F4) result = static_cast<int> (cp - 0x07F3) + 108;
    else if (cp >= 0x0816 and cp <= 0x081A) result = static_cast<int> (cp - 0x0816) + 109;
    else if (cp >= 0x081B and cp <= 0x0824) result = static_cast<int> (cp - 0x081B) + 113;
    else if (cp >= 0x0825 and cp <= 0x0828) result = static_cast<int> (cp - 0x0825) + 122;
    else if (cp >= 0x0829 and cp <= 0x082E) result = static_cast<int> (cp - 0x0829) + 125;
    else if (cp >= 0x0951 and cp <= 0x0952) result = static_cast<int> (cp - 0x0951) + 130;
    else if (cp >= 0x0953 and cp <= 0x0955) result = static_cast<int> (cp - 0x0953) + 131;
    else if (cp >= 0x0F82 and cp <= 0x0F84) result = static_cast<int> (cp - 0x0F82) + 133;
    else if (cp >= 0x0F86 and cp <= 0x0F88) result = static_cast<int> (cp - 0x0F86) + 135;
    else if (cp >= 0x135D and cp <= 0x1360) result = static_cast<int> (cp - 0x135D) + 137;
    else if (cp >= 0x17DD and cp <= 0x17DE) result = static_cast<int> (cp - 0x17DD) + 140;
    else if (cp >= 0x193A and cp <= 0x193B) result = static_cast<int> (cp - 0x193A) + 141;
    else if (cp >= 0x1A17 and cp <= 0x1A18) result = static_cast<int> (cp - 0x1A17) + 142;
    else if (cp >= 0x1A75 and cp <= 0x1A7D) result = static_cast<int> (cp - 0x1A75) + 143;
    else if (cp >= 0x1B6B and cp <= 0x1B6C) result = static_cast<int> (cp - 0x1B6B) + 151;
    else if (cp >= 0x1B6D and cp <= 0x1B74) result = static_cast<int> (cp - 0x1B6D) + 152;
    else if (cp >= 0x1CD0 and cp <= 0x1CD3) result = static_cast<int> (cp - 0x1CD0) + 159;
    else if (cp >= 0x1CDA and cp <= 0x1CDC) result = static_cast<int> (cp - 0x1CDA) + 162;
    else if (cp >= 0x1CE0 and cp <= 0x1CE1) result = static_cast<int> (cp - 0x1CE0) + 164;
    else if (cp >= 0x1DC0 and cp <= 0x1DC2) result = static_cast<int> (cp - 0x1DC0) + 165;
    else if (cp >= 0x1DC3 and cp <= 0x1DCA) result = static_cast<int> (cp - 0x1DC3) + 167;
    else if (cp >= 0x1DCB and cp <= 0x1DCD) result = static_cast<int> (cp - 0x1DCB) + 174;
    else if (cp >= 0x1DD1 and cp <= 0x1DE7) result = static_cast<int> (cp - 0x1DD1) + 176;
    else if (cp >= 0x1DFE and cp <= 0x1DFF) result = static_cast<int> (cp - 0x1DFE) + 198;
    else if (cp >= 0x20D0 and cp <= 0x20D2) result = static_cast<int> (cp - 0x20D0) + 199;
    else if (cp >= 0x20D4 and cp <= 0x20D8) result = static_cast<int> (cp - 0x20D4) + 201;
    else if (cp >= 0x20DB and cp <= 0x20DD) result = static_cast<int> (cp - 0x20DB) + 205;
    else if (cp >= 0x20E1 and cp <= 0x20E2) result = static_cast<int> (cp - 0x20E1) + 207;
    else if (cp >= 0x20E7 and cp <= 0x20E8) result = static_cast<int> (cp - 0x20E7) + 208;
    else if (cp >= 0x20E9 and cp <= 0x20EA) result = static_cast<int> (cp - 0x20E9) + 209;
    else if (cp >= 0x20F0 and cp <= 0x20F1) result = static_cast<int> (cp - 0x20F0) + 210;
    else if (cp >= 0x2CEF and cp <= 0x2CF2) result = static_cast<int> (cp - 0x2CEF) + 211;
    else if (cp >= 0x2DE0 and cp <= 0x2E00) result = static_cast<int> (cp - 0x2DE0) + 214;
    else if (cp >= 0xA66F and cp <= 0xA670) result = static_cast<int> (cp - 0xA66F) + 246;
    else if (cp >= 0xA67C and cp <= 0xA67E) result = static_cast<int> (cp - 0xA67C) + 247;
    else if (cp >= 0xA6F0 and cp <= 0xA6F2) result = static_cast<int> (cp - 0xA6F0) + 249;
    else if (cp >= 0xA8E0 and cp <= 0xA8F2) result = static_cast<int> (cp - 0xA8E0) + 251;
    else if (cp >= 0xAAB0 and cp <= 0xAAB1) result = static_cast<int> (cp - 0xAAB0) + 269;
    else if (cp >= 0xAAB2 and cp <= 0xAAB4) result = static_cast<int> (cp - 0xAAB2) + 270;
    else if (cp >= 0xAAB7 and cp <= 0xAAB9) result = static_cast<int> (cp - 0xAAB7) + 272;
    else if (cp >= 0xAABE and cp <= 0xAAC0) result = static_cast<int> (cp - 0xAABE) + 274;
    else if (cp >= 0xAAC1 and cp <= 0xAAC2) result = static_cast<int> (cp - 0xAAC1) + 276;
    else if (cp >= 0xFE20 and cp <= 0xFE27) result = static_cast<int> (cp - 0xFE20) + 277;
    else if (cp >= 0x10A0F and cp <= 0x10A10) result = static_cast<int> (cp - 0x10A0F) + 284;
    else if (cp >= 0x10A38 and cp <= 0x10A39) result = static_cast<int> (cp - 0x10A38) + 285;
    else if (cp >= 0x1D185 and cp <= 0x1D18A) result = static_cast<int> (cp - 0x1D185) + 286;
    else if (cp >= 0x1D1AA and cp <= 0x1D1AE) result = static_cast<int> (cp - 0x1D1AA) + 291;
    else if (cp >= 0x1D242 and cp <= 0x1D245) result = static_cast<int> (cp - 0x1D242) + 295;

    return result;
}

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
// Glyph emission helper (file-scope)
// =========================================================================

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
template <typename Renderer>
void Screen<Renderer>::processCellForSnapshot (
    const Cell& cell, const Cell* rowCells, const Grapheme* rowGraphemes,
    const ImageCell* rowImageCells, int col, int row, Grid& grid) noexcept
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
    else if (effectiveCell.codepoint == kittyPlaceholderChar)
    {
        // -------------------------------------------------------------------------
        // Kitty unicode placeholder (U+10EEEE)
        //
        // Extract image ID from fg color:
        //   palette mode: fg.red holds the 8-bit ID directly
        //   rgb mode:     (red << 16) | (green << 8) | blue is the 24-bit ID
        //
        // Extract row/col from grapheme diacritics (indices 0 = row, 1 = col, 2 = high byte).
        // Diacritics are 1-based in kittyDiacriticToNum; subtract 1 to get 0-based row/col.
        //
        // Missing diacritics → inherit from left neighbour (handled by the app writing
        // explicit diacritics; we pass through without inheritance for simplicity).
        // -------------------------------------------------------------------------

        uint32_t imageId { 0 };

        if (effectiveCell.fg.isPalette())
        {
            imageId = static_cast<uint32_t> (effectiveCell.fg.paletteIndex());
        }
        else if (effectiveCell.fg.isRGB())
        {
            imageId = (static_cast<uint32_t> (effectiveCell.fg.red)   << 16)
                    | (static_cast<uint32_t> (effectiveCell.fg.green) << 8)
                    |  static_cast<uint32_t> (effectiveCell.fg.blue);
        }

        int placeholderRow { -1 };
        int placeholderCol { -1 };

        if (grapheme != nullptr and grapheme->count > 0)
        {
            const int rowNum { kittyDiacriticToNum (grapheme->extraCodepoints.at (0)) };
            placeholderRow = rowNum > 0 ? rowNum - 1 : 0;

            if (grapheme->count > 1)
            {
                const int colNum { kittyDiacriticToNum (grapheme->extraCodepoints.at (1)) };
                placeholderCol = colNum > 0 ? colNum - 1 : 0;

                if (grapheme->count > 2)
                {
                    // High byte of image ID (3rd diacritic)
                    const int highNum { kittyDiacriticToNum (grapheme->extraCodepoints.at (2)) };
                    const uint32_t highByte { highNum > 0 ? static_cast<uint32_t> (highNum - 1) : 0u };
                    imageId = (imageId & 0x00FFFFFFu) | (highByte << 24);
                }
            }
        }

        if (placeholderRow < 0)
            placeholderRow = 0;

        if (placeholderCol < 0)
            placeholderCol = 0;

        // TODO: virtual placement lookup needs migration to State.
        // getVirtualPlacement was removed from ImageAtlas (wrong owner).
        // The Kitty unicode placeholder path (U+10EEEE) is disabled until
        // VirtualPlacement storage is re-homed to an appropriate owner.
        juce::ignoreUnused (imageId, placeholderRow, placeholderCol);
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
template <typename Renderer>
void Screen<Renderer>::buildSnapshot (State& state, Grid& grid) noexcept
{
    const int cols { state.getCols() };
    const int rows { state.getVisibleRows() };
    const int offset { state.getScrollOffset() };
    // Always consume Grid dirty bits to keep them from accumulating.
    // If a full rebuild is requested, override with all-ones so every row
    // is processed — same initial state as a first-frame render.
    uint64_t dirtyBits[4] {};
    grid.consumeDirtyRows (dirtyBits);

    const bool fullRebuild { state.consumeFullRebuild() };

    if (fullRebuild)
    {
        std::memset (dirtyBits, 0xFF, sizeof (dirtyBits));
    }

    frameScrollDelta = grid.consumeScrollDelta();

    // Scroll shifts visible content — all rows now show different data.
    // Force all-dirty so every row is rebuilt from current grid content.
    // Without this, stale cached quads overwrite memmove'd pixels.
    if (frameScrollDelta > 0)
    {
        std::memset (dirtyBits, 0xFF, sizeof (dirtyBits));
    }

    for (int i { 0 }; i < 4; ++i)
    {
        frameDirtyBits[i] |= dirtyBits[i];
    }

    // Pull hint overlay from State (written by InputHandler via State::setHintOverlay).
    hintOverlay      = state.getHintOverlayData();
    hintOverlayCount = state.getHintOverlayCount();

    for (int r { 0 }; r < rows; ++r)
    {
        const int word { r >> 6 };
        const uint64_t bit { static_cast<uint64_t> (1) << (r & 63) };

        if ((dirtyBits[word] & bit) != 0)
        {
            const Cell* rowCells { offset > 0
                ? grid.scrollbackRow (r, offset)
                : grid.activeVisibleRow (r) };

            if (rowCells != nullptr)
            {
                Cell* prevRow { previousCells.get()
                                + static_cast<size_t> (r) * static_cast<size_t> (cacheCols) };

                // Row-level memcmp: if all cells unchanged and no overlay rebuild
                // needed, skip entirely. Previous frame's cached quads remain valid.
                // fullRebuild bypasses skip — selection/hint overlays need regeneration.
                if (fullRebuild
                    or std::memcmp (rowCells, prevRow, static_cast<size_t> (cols) * sizeof (Cell)) != 0)
                {
                    const Grapheme* rowGraphemes { offset > 0
                        ? grid.scrollbackGraphemeRow (r, offset)
                        : grid.activeVisibleGraphemeRow (r) };

                    const ImageCell* rowImageCells { offset > 0
                        ? grid.scrollbackImageRow (r, offset)
                        : grid.activeVisibleImageRow (r) };

                    monoCount[r]        = 0;
                    emojiCount[r]       = 0;
                    bgCount[r]          = 0;
                    imageCacheCount[r]  = 0;
                    ligatureSkip        = 0;

                    // Leading/trailing blank trim
                    int firstCol { 0 };

                    while (firstCol < cols
                           and rowCells[firstCol].codepoint == 0
                           and rowCells[firstCol].bg.isTheme()
                           and rowCells[firstCol].style == 0)
                    {
                        ++firstCol;
                    }

                    int lastCol { cols - 1 };

                    while (lastCol >= firstCol
                           and rowCells[lastCol].codepoint == 0
                           and rowCells[lastCol].bg.isTheme()
                           and rowCells[lastCol].style == 0)
                    {
                        --lastCol;
                    }

                    for (int c { firstCol }; c <= lastCol; ++c)
                    {
                        processCellForSnapshot (rowCells[c], rowCells, rowGraphemes, rowImageCells, c, r, grid);
                    }

                    // Store current row for next-frame comparison.
                    std::memcpy (prevRow, rowCells, static_cast<size_t> (cols) * sizeof (Cell));
                }
            }
        }
    }

    // Mark previous cursor row dirty — clears stale cursor overlay pixels
    // from the persistent CPU render target. Cost: one row clear per frame.
    if (previousCursorRow >= 0 and previousCursorRow < rows)
    {
        const int word { previousCursorRow >> 6 };
        const uint64_t bit { static_cast<uint64_t> (1) << (previousCursorRow & 63) };
        frameDirtyBits[word] |= bit;
    }

    previousCursorRow = state.getCursorRow();

    updateSnapshot (state, grid, rows, maxGlyphsPerRow);
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
        // Box-drawing uses direct atlas rasterisation (no HarfBuzz shaping, no GlyphRun).
        // This path is intentionally separate from emitShapedGlyphsToCache — merging
        // them would require fabricating a GlyphRun for a codepoint that was never shaped.
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

template class Screen<jam::Glyph::GLContext>;
template class Screen<jam::Glyph::GraphicsContext>;

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Terminal
