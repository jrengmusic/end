/**
 * @file jreng_staged_bitmap.h
 * @brief Pixel data queued for upload to an OpenGL atlas texture.
 */

#pragma once

namespace jreng::Glyph
{

/**
 * @struct StagedBitmap
 * @brief Pixel data queued for upload to an OpenGL atlas texture.
 *
 * Produced on the **MESSAGE THREAD** by `GlyphAtlas::stageForUpload()` and
 * consumed on the **GL THREAD** by `GlyphAtlas::consumeStagedBitmaps()`.
 * Ownership of `pixelData` transfers to the consumer via `std::move`.
 *
 * @see GlyphAtlas::stageForUpload()
 * @see GlyphAtlas::consumeStagedBitmaps()
 */
struct StagedBitmap
{
    /**
     * @brief Identifies which atlas texture this bitmap targets.
     *
     * - `mono`  — R8 grayscale atlas (monochrome glyphs, box drawing).
     * - `emoji` — RGBA8 color atlas (color emoji).
     */
    enum class AtlasKind { mono, emoji };

    /** @brief Target atlas for this upload. */
    AtlasKind kind;

    /**
     * @brief Destination rectangle within the atlas texture, in texels.
     *
     * Passed directly to `glTexSubImage2D` as the x/y offset and width/height.
     */
    juce::Rectangle<int> region;

    /**
     * @brief Heap-allocated pixel data.
     *
     * Layout depends on `kind`:
     * - `mono`  — 1 byte per pixel (R8, linear grey).
     * - `emoji` — 4 bytes per pixel (BGRA on FreeType, premultiplied BGRA on
     *             CoreText with `kCGBitmapByteOrder32Host`).
     */
    juce::HeapBlock<uint8_t> pixelData;

    /** @brief Total byte count of `pixelData` (width × height × bpp). */
    size_t pixelDataSize { 0 };

    /**
     * @brief OpenGL pixel format constant for `glTexSubImage2D`.
     *
     * - `GL_RED`  for mono bitmaps.
     * - `GL_BGRA` for emoji bitmaps.
     */
    int format { 0 };
};

} // namespace jreng::Glyph
