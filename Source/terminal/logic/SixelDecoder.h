/**
 * @file SixelDecoder.h
 * @brief Sixel image format decoder and shared decoded-image types.
 *
 * `Terminal::SixelDecoder` converts a raw Sixel byte stream (the DCS payload
 * after the `q` final byte) into an RGBA8 pixel buffer.  `DecodedImage` and
 * `PendingImage` are the shared transfer types used by all three SKiT decoders
 * (Sixel, Kitty, iTerm2) as they pass decoded pixels from the READER THREAD to
 * the MESSAGE THREAD for staging into the `ImageAtlas`.
 *
 * @par Sixel format summary
 * @code
 *   DCS intro: ESC P [P1;P2;P3] q
 *   Body:
 *     "#Pc;Pu;Px;Py;Pz" — define colour register
 *     "#Pc"             — select colour register
 *     "!Pn c"           — repeat character c Pn times
 *     "$"               — carriage return (X=0, same band)
 *     "-"               — newline (X=0, Y+=6)
 *     '"Pan;Pad;Ph;Pv'  — raster attributes (Ph=width, Pv=height)
 *     0x3F–0x7E         — sixel data byte (bits 0–5 = pixels top-to-bottom)
 *   DCS terminator: ESC \
 * @endcode
 *
 * @see Terminal::PendingImage
 * @see Terminal::Grid::storeDecodedImage()
 * @see Terminal::ImageAtlas::stageWithId()
 */

#pragma once

#include <JuceHeader.h>

namespace Terminal
{ /*____________________________________________________________________________*/

/**
 * @struct DecodedImage
 * @brief Result of decoding an inline image: RGBA pixels + dimensions.
 *
 * Produced by decoders on the READER THREAD.  The MESSAGE THREAD consumes
 * the pixel data and stages it into ImageAtlas.
 */
struct DecodedImage
{
    juce::HeapBlock<uint8_t> rgba;  ///< RGBA8 pixel data, row-major, top-to-bottom.
    int width  { 0 };               ///< Image width in pixels.
    int height { 0 };               ///< Image height in pixels.

    /** @brief True when this DecodedImage holds valid pixel data. */
    bool isValid() const noexcept
    {
        return width > 0 and height > 0 and rgba.get() != nullptr;
    }
};

/**
 * @struct PendingImage
 * @brief A decoded image with a pre-reserved image ID awaiting atlas staging.
 *
 * Produced by the READER THREAD after decoding: the decoder reserves an ID via
 * `Grid::reserveImageId()`, writes cells with that ID via `Grid::activeWriteImage()`,
 * and stores this struct via `Grid::storeDecodedImage()`.  The MESSAGE THREAD
 * pulls it from Grid on first atlas encounter in `processCellForSnapshot()` and
 * calls `ImageAtlas::stageWithId()` to register the RGBA pixels.
 */
struct PendingImage
{
    uint32_t imageId { 0 };             ///< Pre-reserved ID written into Grid cells.
    juce::HeapBlock<uint8_t> rgba;      ///< RGBA8 pixel data, row-major, top-to-bottom.
    int width  { 0 };                   ///< Image width in pixels.
    int height { 0 };                   ///< Image height in pixels.
};

/**
 * @class SixelDecoder
 * @brief Decodes a Sixel byte stream into an RGBA pixel buffer.
 *
 * Implements the full Sixel parse loop: raster attributes, palette definitions,
 * repeat commands, carriage-return, newline, and sixel data bytes.  Buffer
 * growth is handled internally when no raster attributes are present.
 *
 * @note Constructed and used on the READER THREAD only.  All methods are
 *       single-threaded; no locking is performed.
 */
class SixelDecoder
{
public:
    SixelDecoder() = default;

    /**
     * @brief Decode a complete Sixel payload into RGBA pixels.
     *
     * @param data    Raw Sixel bytes (after the DCS `q` final byte).
     * @param length  Number of bytes in the payload.
     * @return DecodedImage with RGBA8 pixels, or an invalid DecodedImage on failure.
     *
     * @note READER THREAD only.
     */
    DecodedImage decode (const uint8_t* data, size_t length) noexcept;

private:
    /**
     * @struct Colour
     * @brief RGBA entry in the 256-register Sixel palette.
     */
    struct Colour
    {
        uint8_t r { 0 };
        uint8_t g { 0 };
        uint8_t b { 0 };
        uint8_t a { 255 };
    };

    /** @brief Sixel palette size — 256 colour registers per the specification. */
    static constexpr int maxPalette { 256 };

    /**
     * @brief Default VT340 palette (indices 0–15).
     *
     * Indices 16–255 default to opaque black.
     */
    static const Colour defaultPalette[maxPalette];

    /**
     * @brief Parses one ';'-separated decimal integer sequence starting at @p pos.
     *
     * Reads up to @p maxParams decimal values separated by ';'.  Parsing stops
     * at the first byte that is not a digit or ';'.  On return, @p pos points
     * to the first non-consumed byte.
     *
     * @param data    Payload pointer.
     * @param length  Payload length.
     * @param pos     Current byte offset (in/out).
     * @param params  Output array; must hold at least @p maxParams ints.
     * @param maxParams  Maximum number of parameters to read.
     * @return Number of parameters parsed (>= 0).
     *
     * @note READER THREAD.
     */
    static int parseParams (const uint8_t* data, size_t length, size_t& pos,
                            int* params, int maxParams) noexcept;

    /**
     * @brief Converts HLS (Hue/Lightness/Saturation) to RGB.
     *
     * Standard HLS→RGB algorithm.  Inputs: H in [0,360], L in [0,100],
     * S in [0,100].  Outputs: r,g,b in [0,255].
     *
     * @note READER THREAD — pure, no side effects.
     */
    static void hlsToRgb (int h, int l, int s,
                          uint8_t& r, uint8_t& g, uint8_t& b) noexcept;

    /**
     * @brief Ensures the pixel buffer is large enough to hold (requiredW, requiredH).
     *
     * If the buffer is too small, allocates a new buffer that is at least
     * double the exceeded dimension, copies existing pixels, and updates
     * @p bufferW / @p bufferH.
     *
     * @param rgba     Current buffer HeapBlock.
     * @param bufferW  Current buffer width (in/out).
     * @param bufferH  Current buffer height (in/out).
     * @param requiredW  Required width in pixels.
     * @param requiredH  Required height in pixels.
     * @return `true` if the buffer is ready; `false` on allocation failure.
     *
     * @note READER THREAD.
     */
    static bool growBuffer (juce::HeapBlock<uint8_t>& rgba,
                            int& bufferW, int& bufferH,
                            int requiredW, int requiredH) noexcept;

    /**
     * @brief Writes one sixel data character at (x, y) using @p colour.
     *
     * Extracts 6 bits from @p sixelByte (bit 0 = top pixel) and writes each set
     * bit as an RGBA pixel.  Silently skips pixels outside [bufferW, bufferH).
     *
     * @param rgba      Pixel buffer.
     * @param bufferW   Buffer width in pixels.
     * @param bufferH   Buffer height in pixels.
     * @param x         X coordinate (column).
     * @param y         Y coordinate of the top pixel in the sixel band.
     * @param sixelByte Raw sixel character (0x3F–0x7E).
     * @param colour    Colour to write for set bits.
     *
     * @note READER THREAD.
     */
    static void writeSixelByte (uint8_t* rgba, int bufferW, int bufferH,
                                int x, int y, uint8_t sixelByte,
                                const Colour& colour) noexcept;
};

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Terminal
