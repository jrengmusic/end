/**
 * @file jreng_typeface_shaping.cpp
 * @brief FreeType + HarfBuzz text shaping pipeline for the terminal renderer.
 *
 * Compiled on all platforms **except macOS** (`!JUCE_MAC`).  Implements the
 * shaping methods declared in `jreng_font.h` and the fixed-point / display-scale
 * utilities shared with `jreng_font_metrics.cpp`.
 *
 * ### Shaping pipeline
 *
 * `shapeText()` is the primary entry point.  It dispatches to one of three
 * strategies depending on the input:
 *
 * | Condition                          | Strategy          | Cost      |
 * |------------------------------------|-------------------|-----------|
 * | Single codepoint, U+0000–U+007F    | `shapeASCII()`    | Minimal   |
 * | Multi-codepoint or non-ASCII       | `shapeHarfBuzz()` | Moderate  |
 * | HarfBuzz returns all `.notdef`     | `shapeFallback()` | Higher    |
 *
 * `shapeEmoji()` is a parallel entry point for codepoints already classified
 * as emoji by the cell layout flags; it bypasses the dispatch table and shapes
 * directly with `emojiShapingFont`.
 *
 * ### ASCII fast path
 * For single ASCII codepoints the HarfBuzz pipeline is skipped entirely.
 * `FT_Get_Char_Index` resolves the glyph index and `face->size->metrics.max_advance`
 * provides the advance width — both are O(1) lookups with no buffer allocation.
 * This covers the vast majority of terminal output (printable ASCII text).
 *
 * ### HarfBuzz shaping
 * For non-ASCII or multi-codepoint clusters, the codepoints are fed into a
 * reusable `hb_buffer_t` (`scratchBuffer`).  HarfBuzz guesses the script and
 * direction via `hb_buffer_guess_segment_properties`, then shapes the run.
 * The resulting `hb_glyph_info_t` / `hb_glyph_position_t` arrays are copied
 * into `shapingBuffer` (grown on demand, never shrunk).
 *
 * ### .notdef detection
 * After HarfBuzz shaping, each output glyph's `codepoint` field (which
 * HarfBuzz repurposes to hold the font-internal glyph index) is checked.
 * A value of `0` is the `.notdef` glyph — the font's "missing character" box.
 * If **all** output glyphs are `.notdef`, the run is considered unrenderable
 * and `shapeHarfBuzz()` returns an empty result, triggering `shapeFallback()`.
 *
 * ### Fallback shaping
 * `shapeFallback()` is the last resort on the FreeType backend.  It iterates
 * the codepoints and calls `FT_Get_Char_Index` on the regular face, writing
 * only the codepoints that resolve to a non-zero glyph index.  The advance
 * width is taken from the **primary face's** `max_advance` (cell width),
 * matching `shapeASCII`.  This enforces strict-monospace cell-grid alignment
 * regardless of whether the resolved glyph came from the primary face or a
 * system fallback face.  On macOS the fallback uses CoreText font substitution
 * instead (see `jreng_font.mm`).
 *
 * ### Fixed-point utilities
 * `ceil26_6ToPx` and `roundFloatPxTo26_6` convert between FreeType's 26.6
 * fixed-point format and floating-point pixels.  They are defined here because
 * they are used by both `jreng_font_shaping.cpp` and `jreng_font_metrics.cpp`.
 *
 * @note All methods run on the **MESSAGE THREAD**.
 *
 * @see jreng_font.h
 * @see jreng_font_metrics.cpp
 * @see jreng::Typeface::Registry
 */

// Included via unity build (jreng_font.cpp) — jreng_glyph.h already in scope

#include <algorithm>

#if JUCE_MAC
// CoreText implementation in jreng_font.mm
#else

/**
 * @brief Fast path for single ASCII codepoints (U+0000–U+007F).
 *
 * Bypasses HarfBuzz entirely to minimize per-character overhead for the common
 * case of printable ASCII terminal output.  The implementation:
 *
 * 1. Resolves the glyph index via `FT_Get_Char_Index`.
 * 2. If the glyph exists (index != 0), ensures `shapingBuffer` has capacity
 *    for at least one `Glyph` (allocating 16 slots on first use).
 * 3. Writes a single `Glyph` with zero offsets and the cell advance width
 *    taken from `face->size->metrics.max_advance` (already in 26.6; shifted
 *    right by 6 to convert to pixels).
 *
 * @param codepoint  ASCII codepoint (caller guarantees < 128).
 * @return GlyphRun with `count == 1` on success; `count == 0` if the glyph
 *         is not present in the font (glyph index == 0).
 *
 * @note `max_advance` is used rather than the per-glyph `horiAdvance` because
 *       the terminal grid is fixed-width — every cell occupies the same number
 *       of pixels regardless of the individual glyph's natural advance.
 */
namespace jreng
{
static constexpr int minShapingCapacity { 16 };
} // namespace jreng

jreng::Typeface::GlyphRun jreng::Typeface::shapeASCII (uint32_t codepoint) noexcept
{
    GlyphRun result;
    FT_Face face { getFace (Style::regular) };
    const FT_UInt glyphIndex { FT_Get_Char_Index (face, codepoint) };

    if (glyphIndex != 0)
    {
        if (shapingBufferCapacity < 1)
        {
            shapingBuffer.allocate (jreng::minShapingCapacity, false);
            shapingBufferCapacity = jreng::minShapingCapacity;
        }

        Glyph& g { shapingBuffer[0] };
        g.glyphIndex = glyphIndex;
        g.xOffset = 0.0f;
        g.yOffset = 0.0f;
        g.xAdvance = static_cast<float> (face->size->metrics.max_advance >> 6);

        result = { shapingBuffer.get(), 1 };
    }

    return result;
}

/**
 * @brief Shapes a codepoint sequence using HarfBuzz.
 *
 * Used for non-ASCII text and multi-codepoint clusters (ligatures, combining
 * marks, complex scripts).  The shaping pipeline:
 *
 * 1. **Buffer reset** — `hb_buffer_reset` clears the reusable `scratchBuffer`
 *    without deallocating its internal storage.
 * 2. **Input** — `hb_buffer_add_utf32` feeds the full codepoint array.
 * 3. **Property detection** — `hb_buffer_guess_segment_properties` infers the
 *    Unicode script (e.g. Latin, Arabic, Devanagari) and text direction (LTR/RTL)
 *    from the codepoints, enabling correct shaping for complex scripts.
 * 4. **Shaping** — `hb_shape` applies GSUB/GPOS lookups from the font's OpenType
 *    tables, producing a sequence of positioned glyph IDs.
 * 5. **Extraction** — glyph info and positions are read back via
 *    `hb_buffer_get_glyph_infos` / `hb_buffer_get_glyph_positions`.
 * 6. **`.notdef` check** — if every output glyph has index 0 (the `.notdef`
 *    glyph, meaning the font cannot render any codepoint in the run), the
 *    method returns an empty result so `shapeText()` can try `shapeFallback()`.
 * 7. **Copy** — surviving glyphs are written into `shapingBuffer`, which is
 *    grown (never shrunk) to accommodate the run.  HarfBuzz positions are in
 *    26.6 fixed-point; they are divided by `ftFixedScale` (64) to convert to
 *    floating-point pixels.
 *
 * @param style       Font style — selects the HarfBuzz font via `getHbFont()`.
 * @param codepoints  UTF-32 codepoint array.
 * @param count       Number of codepoints in the array.
 * @return GlyphRun pointing into `shapingBuffer`; `count == 0` if the font
 *         has no glyphs for any codepoint in the run (all `.notdef`).
 *
 * @note HarfBuzz repurposes `hb_glyph_info_t::codepoint` to store the
 *       font-internal glyph index after shaping.  A value of 0 is `.notdef`.
 *
 * @note The returned pointer is invalidated by the next call to any `shape*`
 *       method.
 */
jreng::Typeface::GlyphRun jreng::Typeface::shapeHarfBuzz (Style style,
                                                      const uint32_t* codepoints, size_t count) noexcept
{
    GlyphRun result;
    hb_font_t* font { getHbFont (style) };

    if (font != nullptr and scratchBuffer != nullptr)
    {
        hb_buffer_reset (scratchBuffer);
        hb_buffer_add_utf32 (scratchBuffer, codepoints, static_cast<int> (count), 0, static_cast<int> (count));
        hb_buffer_guess_segment_properties (scratchBuffer);
        hb_shape (font, scratchBuffer, nullptr, 0);

        unsigned int glyphCount { 0 };
        hb_glyph_info_t* glyphInfo { hb_buffer_get_glyph_infos (scratchBuffer, &glyphCount) };
        hb_glyph_position_t* glyphPos { hb_buffer_get_glyph_positions (scratchBuffer, &glyphCount) };

        if (glyphCount > 0)
        {
            // Reject the run if every output glyph is .notdef (index 0).
            // This signals that the primary font cannot render any codepoint
            // in the cluster, and shapeText() should try shapeFallback().
            const bool hasValidGlyph { std::any_of (glyphInfo, glyphInfo + glyphCount,
                [] (const hb_glyph_info_t& info) { return info.codepoint != 0; }) };

            if (hasValidGlyph)
            {
                if (static_cast<int> (glyphCount) > shapingBufferCapacity)
                {
                    shapingBuffer.allocate (static_cast<size_t> (glyphCount), false);
                    shapingBufferCapacity = static_cast<int> (glyphCount);
                }

                for (unsigned int i { 0 }; i < glyphCount; ++i)
                {
                    Glyph& g { shapingBuffer[i] };
                    g.glyphIndex = glyphInfo[i].codepoint;
                    g.xOffset = glyphPos[i].x_offset / static_cast<float> (ftFixedScale);
                    g.yOffset = glyphPos[i].y_offset / static_cast<float> (ftFixedScale);
                    g.xAdvance = glyphPos[i].x_advance / static_cast<float> (ftFixedScale);
                }

                result = { shapingBuffer.get(), static_cast<int> (glyphCount) };
            }
        }
    }

    return result;
}

/**
 * @brief Last-resort fallback shaper with Windows DirectWrite system font discovery.
 *
 * Called when `shapeHarfBuzz()` returns an empty result (all `.notdef`).  For
 * each codepoint the shaper checks:
 *
 * 1. Primary face (`FT_Get_Char_Index`) — if found, use it directly.
 * 2. `fallbackFontCache` — if a cached `FT_Face` (including `nullptr` sentinel)
 *    exists for this codepoint, use or skip accordingly without re-querying.
 * 3. On Windows: `discoverFallbackFace()` — queries DirectWrite to find a system
 *    font covering the codepoint, caches the result (or `nullptr`) for next time.
 * 4. On other platforms: no system fallback; codepoint is skipped.
 *
 * When a fallback face is used, only the glyph index is read from it.  The
 * advance is always sourced from the primary face's cell width — fallback fonts
 * are typically proportional, so using their natural advance breaks the
 * monospace grid.  `GlyphRun::fontHandle` is set to the fallback `FT_Face` (as
 * `void*`) so the renderer passes the correct handle to `Packer::getOrRasterize`.
 *
 * @param codepoints  UTF-32 codepoint array.
 * @param count       Number of codepoints in the array.
 * @return GlyphRun with the subset of codepoints that could be resolved;
 *         `count == 0` if no codepoint could be shaped by any face.
 *
 * @note `fallbackFontCache` maps codepoint → `FT_Face` (or `nullptr` if no
 *       system font was found).  Lifetime of cached faces is managed by
 *       `setSize()` (clears on resize) and the destructor.
 */
jreng::Typeface::GlyphRun jreng::Typeface::shapeFallback (const uint32_t* codepoints, size_t count) noexcept
{
    GlyphRun result;
    FT_Face primaryFace { getFace (Style::regular) };

    if (primaryFace != nullptr)
    {
        const int needed { static_cast<int> (count) };

        if (needed > shapingBufferCapacity)
        {
            shapingBuffer.allocate (static_cast<size_t> (needed), false);
            shapingBufferCapacity = needed;
        }

        int written { 0 };
        void* lastFallbackHandle { nullptr };

        for (size_t i { 0 }; i < count; ++i)
        {
            const uint32_t cp { codepoints[i] };
            const FT_UInt primaryIndex { FT_Get_Char_Index (primaryFace, cp) };

            FT_Face activeFace { nullptr };
            FT_UInt activeIndex { 0 };

            if (primaryIndex != 0)
            {
                activeFace = primaryFace;
                activeIndex = primaryIndex;
            }
            else
            {
                const auto cacheIt { fallbackFontCache.find (cp) };

                if (cacheIt != fallbackFontCache.end())
                {
                    if (cacheIt->second != nullptr)
                    {
                        const FT_UInt fallbackIndex { FT_Get_Char_Index (cacheIt->second, cp) };

                        if (fallbackIndex != 0)
                        {
                            activeFace = cacheIt->second;
                            activeIndex = fallbackIndex;
                            lastFallbackHandle = static_cast<void*> (activeFace);
                        }
                    }
                }
                else
                {
                    #if JUCE_WINDOWS
                    FT_Face discovered { discoverFallbackFace (cp) };
                    fallbackFontCache[cp] = discovered;

                    if (discovered != nullptr)
                    {
                        const FT_UInt fallbackIndex { FT_Get_Char_Index (discovered, cp) };

                        if (fallbackIndex != 0)
                        {
                            activeFace = discovered;
                            activeIndex = fallbackIndex;
                            lastFallbackHandle = static_cast<void*> (activeFace);
                        }
                    }
                    #endif
                }
            }

            if (activeFace != nullptr and activeIndex != 0)
            {
                const float advance { static_cast<float> (primaryFace->size->metrics.max_advance >> 6) };

                Glyph& g { shapingBuffer[written] };
                g.glyphIndex = activeIndex;
                g.xOffset = 0.0f;
                g.yOffset = 0.0f;
                g.xAdvance = advance;
                ++written;
            }
        }

        result = { shapingBuffer.get(), written, lastFallbackHandle };
    }

    return result;
}

/**
 * @brief Shapes a codepoint sequence and returns positioned glyphs.
 *
 * Primary shaping entry point.  Dispatches to the appropriate strategy:
 *
 * @par Dispatch logic
 * @code
 * if (count == 1 && codepoints[0] < 128)
 *     return shapeASCII (codepoints[0]);   // fast path — no HarfBuzz
 *
 * result = shapeHarfBuzz (style, codepoints, count);
 *
 * if (result.count == 0)
 *     result = shapeFallback (codepoints, count);  // all .notdef — try cmap lookup
 * @endcode
 *
 * The guard conditions (`codepoints != nullptr`, `count > 0`, face loaded) are
 * checked once here so the individual shapers can assume valid inputs.
 *
 * @param style       Font style variant to use for shaping.
 * @param codepoints  Pointer to a UTF-32 codepoint array.
 * @param count       Number of codepoints in the array.
 * @return GlyphRun pointing into the internal `shapingBuffer`.
 *         `count == 0` means no glyphs could be produced for any codepoint.
 *
 * @note The returned pointer is invalidated by the next call to any `shape*`
 *       method.  Callers must consume the result before the next shaping call.
 *
 * @see shapeASCII
 * @see shapeHarfBuzz
 * @see shapeFallback
 * @see shapeEmoji
 */
jreng::Typeface::GlyphRun jreng::Typeface::shapeText (Style style,
                                                  const uint32_t* codepoints,
                                                  size_t count) noexcept
{
    GlyphRun result;

    if (codepoints != nullptr and count > 0 and getFace (style) != nullptr)
    {
        if (isMonospace and count == 1 and codepoints[0] < 128)
        {
            result = shapeASCII (codepoints[0]);
        }
        else
        {
            result = shapeHarfBuzz (style, codepoints, count);

            if (result.count == 0)
            {
                result = shapeFallback (codepoints, count);
            }
        }
    }

    return result;
}

/**
 * @brief Shapes a codepoint sequence using the dedicated emoji font.
 *
 * Used for codepoints already classified as emoji by the cell layout flags
 * (e.g. U+1F600 GRINNING FACE, ZWJ sequences, variation selector-16 clusters).
 * Bypasses the `shapeText()` dispatch table and shapes directly with
 * `emojiShapingFont` — no fallback chain.
 *
 * The shaping pipeline mirrors `shapeHarfBuzz()`:
 * 1. Reset `scratchBuffer`.
 * 2. Add codepoints as UTF-32.
 * 3. Guess segment properties (script, direction).
 * 4. Shape with `emojiShapingFont`.
 * 5. Copy glyph info and positions into `shapingBuffer`.
 *
 * Unlike `shapeHarfBuzz()`, there is no `.notdef` check — if the emoji font
 * cannot render the cluster, the renderer will display whatever glyph index
 * HarfBuzz returns (typically `.notdef`).
 *
 * @param codepoints  Pointer to a UTF-32 codepoint array.
 * @param count       Number of codepoints in the array.
 * @return GlyphRun pointing into `shapingBuffer`; `count == 0` if
 *         `emojiShapingFont` is null, `scratchBuffer` is null, or HarfBuzz
 *         produced no output glyphs.
 *
 * @note The returned pointer is invalidated by the next call to any `shape*`
 *       method.
 *
 * @see shapeText
 */
jreng::Typeface::GlyphRun jreng::Typeface::shapeEmoji (const uint32_t* codepoints, size_t count) noexcept
{
    GlyphRun result;

    if (codepoints != nullptr and count > 0 and emojiShapingFont != nullptr and scratchBuffer != nullptr)
    {
        hb_buffer_reset (scratchBuffer);
        hb_buffer_add_utf32 (scratchBuffer, codepoints, static_cast<int> (count), 0, static_cast<int> (count));
        hb_buffer_guess_segment_properties (scratchBuffer);
        hb_shape (emojiShapingFont, scratchBuffer, nullptr, 0);

        unsigned int glyphCount { 0 };
        hb_glyph_info_t* glyphInfo { hb_buffer_get_glyph_infos (scratchBuffer, &glyphCount) };
        hb_glyph_position_t* glyphPos { hb_buffer_get_glyph_positions (scratchBuffer, &glyphCount) };

        if (glyphCount > 0)
        {
            if (static_cast<int> (glyphCount) > shapingBufferCapacity)
            {
                shapingBuffer.allocate (static_cast<size_t> (glyphCount), false);
                shapingBufferCapacity = static_cast<int> (glyphCount);
            }

            for (unsigned int i { 0 }; i < glyphCount; ++i)
            {
                Glyph& g { shapingBuffer[i] };
                g.glyphIndex = glyphInfo[i].codepoint;
                g.xOffset = glyphPos[i].x_offset / static_cast<float> (ftFixedScale);
                g.yOffset = glyphPos[i].y_offset / static_cast<float> (ftFixedScale);
                g.xAdvance = glyphPos[i].x_advance / static_cast<float> (ftFixedScale);
            }

            result = { shapingBuffer.get(), static_cast<int> (glyphCount) };
        }
    }

    return result;
}

/**
 * @brief Converts a 26.6 fixed-point value to pixels, rounding up (ceiling).
 *
 * FreeType stores most metric values in 26.6 fixed-point format: the lower 6
 * bits are the fractional part (1/64 pixel precision) and the upper bits are
 * the integer pixel count.  This function performs a ceiling conversion so that
 * partial pixels are always rounded up, preventing glyph clipping.
 *
 * @par Formula
 * @code
 * pixels = (v26_6 + 63) >> 6   // equivalent to ceil(v26_6 / 64.0)
 * @endcode
 *
 * @param v26_6  Value in FreeType 26.6 fixed-point format.
 * @return Ceiling pixel count (integer).
 *
 * @see roundFloatPxTo26_6
 * @see ftFixedScale
 */
int jreng::Typeface::ceil26_6ToPx (int v26_6) noexcept
{
    return (v26_6 + ftFixedScale - 1) >> 6;
}

/**
 * @brief Converts a floating-point pixel value to 26.6 fixed-point, rounding.
 *
 * Multiplies by `ftFixedScale` (64) and adds 0.5 before truncating to integer,
 * producing the nearest 26.6 representation.  Used to convert a logical pixel
 * height (e.g. from the UI) into the format expected by `FT_Set_Char_Size`.
 *
 * @par Formula
 * @code
 * v26_6 = (int)(px * 64.0f + 0.5f)   // nearest 26.6 integer
 * @endcode
 *
 * @param px  Pixel value in floating-point (logical / CSS pixels).
 * @return Nearest 26.6 fixed-point integer.
 *
 * @see ceil26_6ToPx
 * @see ftFixedScale
 */
int jreng::Typeface::roundFloatPxTo26_6 (float px) noexcept
{
    return static_cast<int> (px * static_cast<float> (ftFixedScale) + 0.5f);
}

/**
 * @brief Returns the device-pixel ratio of the primary display.
 *
 * Queries `juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()` and
 * reads its `scale` field.  Returns `1.0f` if no display is available (e.g.
 * headless build or unit test context).
 *
 * Used by `calcMetrics()` and `loadFaces()` to compute the render DPI:
 * @code
 * FT_UInt renderDpi = baseDpi * getDisplayScale();
 * @endcode
 *
 * On a standard 96 DPI screen `getDisplayScale()` returns `1.0f`; on a HiDPI
 * screen (e.g. 192 DPI) it returns `2.0f`.  Physical pixel dimensions are
 * `logicalPx * displayScale`.
 *
 * @return Display scale factor (e.g. `2.0f` on HiDPI, `1.0f` on standard).
 *
 * @see jreng::Typeface::Metrics
 * @see calcMetrics
 */
float jreng::Typeface::getDisplayScale() noexcept
{
    const auto* display { juce::Desktop::getInstance().getDisplays().getPrimaryDisplay() };
    float scale { 1.0f };

    if (display != nullptr)
    {
        scale = static_cast<float> (display->scale);
    }

    return scale;
}

#endif
