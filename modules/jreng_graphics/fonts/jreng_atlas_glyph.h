/**
 * @file jreng_atlas_glyph.h
 * @brief Rasterized glyph descriptor: atlas location and bearing metrics.
 */

#pragma once

namespace jreng::Glyph
{

/**
 * @struct Region
 * @brief Rasterized glyph descriptor: atlas location and bearing metrics.
 *
 * Returned by `LRUCache::get()` and `LRUCache::insert()`.  The
 * renderer uses `textureCoordinates` to sample the atlas texture and
 * `bearingX` / `bearingY` to position the quad relative to the baseline.
 *
 * @see GlyphAtlas::getOrRasterize()
 * @see LRUCache
 */
struct Region
{
    /**
     * @brief Normalized UV rectangle within the atlas texture.
     *
     * Origin is top-left; x/y are the UV offset, width/height are the UV
     * extent.  Divide pixel coordinates by `atlasSize` (4096) to obtain these.
     */
    juce::Rectangle<float> textureCoordinates;

    /** @brief Width of the rasterized bitmap in physical pixels. */
    int widthPixels { 0 };

    /** @brief Height of the rasterized bitmap in physical pixels. */
    int heightPixels { 0 };

    /**
     * @brief Horizontal bearing: pixels from the pen origin to the left edge
     *        of the bitmap.
     *
     * Corresponds to `FT_GlyphSlot::bitmap_left` (FreeType) or
     * `CGRect::origin.x` (CoreText).  May be negative for glyphs that extend
     * left of the pen.
     */
    int bearingX { 0 };

    /**
     * @brief Vertical bearing: pixels from the baseline to the top edge of
     *        the bitmap.
     *
     * Corresponds to `FT_GlyphSlot::bitmap_top` (FreeType) or
     * `CGRect::origin.y + height` (CoreText).  Positive values place the
     * bitmap above the baseline.
     */
    int bearingY { 0 };
};

} // namespace jreng::Glyph
