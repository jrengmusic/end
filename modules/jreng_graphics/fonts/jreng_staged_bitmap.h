/**
 * @file jreng_staged_bitmap.h
 * @brief Pixel data queued for renderer upload.
 */

#pragma once

namespace jreng::Glyph
{

/**
 * @brief Identifies which atlas texture a glyph targets.
 *
 * - `mono`  — R8 grayscale atlas (monochrome glyphs, box drawing).
 * - `emoji` — RGBA8 color atlas (color emoji).
 */
enum class Type { mono, emoji };

/**
 * @struct StagedBitmap
 * @brief Pixel data queued for renderer upload.
 *
 * Produced on the **MESSAGE THREAD** by `Packer::stageForUpload()` and
 * consumed by the renderer at frame start.  Ownership of `pixelData`
 * transfers to the consumer via `std::move`.
 *
 * @see Packer::stageForUpload()
 * @see Packer::consumeStagedBitmaps()
 */
struct StagedBitmap
{
    /** @brief Target atlas for this bitmap. */
    Type type;

    /**
     * @brief Destination rectangle within the atlas texture, in texels.
     */
    juce::Rectangle<int> region;

    /**
     * @brief Heap-allocated pixel data.
     *
     * Layout depends on `type`:
     * - `mono`  — 1 byte per pixel (R8, linear grey).
     * - `emoji` — 4 bytes per pixel (BGRA on FreeType, premultiplied BGRA on
     *             CoreText with `kCGBitmapByteOrder32Host`).
     */
    juce::HeapBlock<uint8_t> pixelData;

    /** @brief Total byte count of `pixelData` (width * height * bpp). */
    size_t pixelDataSize { 0 };
};

} // namespace jreng::Glyph
