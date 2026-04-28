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

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Terminal
