/**
 * @file jreng_glyph_renderer.h
 * @brief Abstract rendering target for glyph and background drawing.
 *
 * Mirrors the `juce::LowLevelGraphicsContext::drawGlyphs` pattern:
 * font state is set per-run via `setFont()`, then `drawGlyphs()` receives
 * parallel spans of glyph codes and positions.
 *
 * Concrete implementations:
 * - **GL backend** (`jreng_opengl`): instanced quad rendering with atlas textures.
 * - **CPU backend** (Plan 3): `juce::Graphics` blit rendering.
 *
 * @see jreng::TextLayout::draw()
 */

#pragma once

namespace jreng
{
    struct Typeface;
}

namespace jreng::Glyph
{

/**
 * @struct Renderer
 * @brief Abstract rendering target for positioned glyphs and backgrounds.
 *
 * The layout layer (`TextLayout`) iterates its lines and runs, calling
 * `setFont()` once per run and `drawGlyphs()` with parallel spans of
 * glyph codes and baseline positions.  The renderer implementation
 * handles caching, atlas lookup, and draw submission internally.
 *
 * `drawBackgrounds()` receives parallel spans of rectangles and colours
 * for background fills behind text runs.
 *
 * @par Thread context
 * Determined by the concrete implementation.  GL implementations
 * are typically called on the **GL THREAD**; CPU implementations
 * on the **MESSAGE THREAD** inside `paint()`.
 */
struct Renderer
{
    virtual ~Renderer() = default;

    /**
     * @brief Set the font context for the next `drawGlyphs()` call.
     *
     * Must be called before each batch of `drawGlyphs()`.  The renderer
     * caches the font reference, style, and emoji flag for atlas lookup
     * and texture binding.
     *
     * @param font    Font instance providing rasterization and metrics.
     * @param style   Font style variant for this run.
     * @param isEmoji `true` if this run uses the emoji atlas.
     */
    virtual void setFont (jreng::Typeface& typeface, Typeface::Style style, bool isEmoji) = 0;

    /**
     * @brief Draw a batch of positioned glyphs.
     *
     * Receives parallel spans: `glyphCodes[i]` is the font-internal glyph
     * index to draw at `positions[i]`.  The renderer handles atlas lookup,
     * cache miss rasterization, and draw submission.
     *
     * @param glyphCodes  Array of glyph indices (TrueType/OpenType, max 65535).
     * @param positions   Array of baseline anchor points in pixel coordinates.
     * @param count       Number of elements in both arrays.
     */
    virtual void drawGlyphs (const uint16_t* glyphCodes,
                              const juce::Point<float>* positions,
                              int count) = 0;

    /**
     * @brief Draw a batch of background rectangles.
     *
     * Receives parallel spans: `areas[i]` is filled with `colours[i]`.
     *
     * @param areas    Array of rectangles in pixel coordinates.
     * @param colours  Array of fill colours, one per rectangle.
     * @param count    Number of elements in both arrays.
     */
    virtual void drawBackgrounds (const juce::Rectangle<float>* areas,
                                   const juce::Colour* colours,
                                   int count) = 0;
};

} // namespace jreng::Glyph
