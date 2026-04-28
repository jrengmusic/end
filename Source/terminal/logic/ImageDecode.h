/**
 * @file ImageDecode.h
 * @brief Platform-native image decoding and BGRA-to-RGBA conversion.
 *
 * Shared by KittyDecoder and ITerm2Decoder.  The native decode function
 * is implemented in platform-specific translation units:
 * - ImageDecodeMac.mm  (macOS / iOS — CoreGraphics + ImageIO)
 * - ImageDecodeWin.cpp (Windows — WIC)
 *
 * @see KittyDecoder
 * @see ITerm2Decoder
 */

#pragma once

#include <JuceHeader.h>

namespace Terminal
{ /*____________________________________________________________________________*/

/**
 * @brief Decodes PNG/JPEG image data using the platform's native codec.
 *
 * Returns a `juce::Image` in ARGB format (JUCE's internal layout: BGRA in
 * memory on little-endian).  Callers that need straight RGBA should pass the
 * result through `swizzleARGBToRGBA()`.
 *
 * @param data      Pointer to the compressed image bytes (PNG or JPEG).
 * @param dataSize  Number of bytes in `data`.
 * @return A valid `juce::Image` on success, or `juce::Image()` on failure.
 *
 * @note macOS: CGImageSource + CGBitmapContext (premultiplied BGRA).
 * @note Windows: WIC IWICImagingFactory + IWICFormatConverter (BGRA).
 * @note Linux: returns invalid image (no native codec support).
 */
juce::Image loadImageNative (const void* data, size_t dataSize) noexcept;

/**
 * @brief Converts a JUCE ARGB image to straight RGBA in a HeapBlock.
 *
 * Reads the JUCE internal BGRA premultiplied layout, un-premultiplies alpha,
 * and writes straight RGBA suitable for GPU upload.
 *
 * @param image  Source JUCE image (must be valid, ARGB format).
 * @param rgba   Output buffer; allocated to `width * height * 4` bytes.
 * @param[out] width   Image width in pixels.
 * @param[out] height  Image height in pixels.
 * @return `true` if conversion succeeded, `false` if image was invalid.
 */
bool swizzleARGBToRGBA (const juce::Image& image,
                        juce::HeapBlock<uint8_t>& rgba,
                        int& width, int& height) noexcept;

/**
 * @struct ImageSequence
 * @brief All frames of a decoded image, pre-composited and in straight RGBA8.
 *
 * For static images (PNG, JPEG, single-frame GIF): `frameCount == 1`, `delays`
 * is null.  For animated GIFs: `frameCount > 1`, `delays` holds per-frame
 * durations in milliseconds, all frames are pre-composited with GIF disposal
 * methods applied.  Pixels are stored contiguously: frame N starts at offset
 * `N * width * height * 4`.
 */
struct ImageSequence
{
    juce::HeapBlock<uint8_t> pixels;  ///< All frames contiguous, straight RGBA8, row-major.
    juce::HeapBlock<int> delays;      ///< Per-frame delay in ms.  Null for static images.
    int frameCount { 0 };             ///< 1 = static, >1 = animated.
    int width  { 0 };                 ///< Frame width in pixels (canvas size for GIF).
    int height { 0 };                 ///< Frame height in pixels (canvas size for GIF).

    /** @brief True when this sequence holds valid pixel data. */
    bool isValid() const noexcept { return width > 0 and height > 0 and pixels.get() != nullptr; }
};

/**
 * @brief Decodes all frames of an image using the platform's native codec.
 *
 * For GIF: extracts all frames, applies disposal methods (do-not-dispose,
 * restore-to-background, restore-to-previous), and returns pre-composited
 * frames as straight RGBA8.  For PNG/JPEG: returns a single frame.
 *
 * @param data      Pointer to the compressed image bytes.
 * @param dataSize  Number of bytes in `data`.
 * @return An `ImageSequence` with all frames, or an invalid sequence on failure.
 *
 * @note READER THREAD or MESSAGE THREAD.
 */
ImageSequence loadImageSequenceNative (const void* data, size_t dataSize) noexcept;

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Terminal
