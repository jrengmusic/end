/**
 * @file SixelDecoderParse.cpp
 * @brief Sixel parsing helpers for Terminal::SixelDecoder.
 *
 * Contains: VT340 default palette definition, parameter tokeniser, HLS→RGB
 * conversion, pixel buffer growth, and single-byte sixel pixel writer.
 * The public API entry point (decode()) lives in SixelDecoder.cpp.
 *
 * @see SixelDecoder.h
 * @see SixelDecoder.cpp
 */

#include "SixelDecoder.h"

namespace Terminal
{ /*____________________________________________________________________________*/

// ============================================================================
// SixelDecoder — VT340 default palette
// ============================================================================

/// @brief VT340-compatible default palette (16 standard colours; rest black).
const SixelDecoder::Colour SixelDecoder::defaultPalette[maxPalette]
{
    { 0,   0,   0,   255 },  // 0  Black
    { 51,  51,  204, 255 },  // 1  Blue
    { 204, 51,  51,  255 },  // 2  Red
    { 51,  204, 51,  255 },  // 3  Green
    { 204, 51,  204, 255 },  // 4  Magenta
    { 51,  204, 204, 255 },  // 5  Cyan
    { 204, 204, 51,  255 },  // 6  Yellow
    { 147, 147, 147, 255 },  // 7  Gray 50%
    { 77,  77,  77,  255 },  // 8  Gray 25%
    { 51,  51,  255, 255 },  // 9  Bright Blue
    { 255, 51,  51,  255 },  // 10 Bright Red
    { 51,  255, 51,  255 },  // 11 Bright Green
    { 255, 51,  255, 255 },  // 12 Bright Magenta
    { 51,  255, 255, 255 },  // 13 Bright Cyan
    { 255, 255, 51,  255 },  // 14 Bright Yellow
    { 255, 255, 255, 255 },  // 15 White
    // Indices 16–255: zero-initialised (opaque black via Colour{} default)
};

// ============================================================================
// SixelDecoder helpers
// ============================================================================

/**
 * @brief Parses up to @p maxParams ';'-separated decimal integers from @p data.
 *
 * Advances @p pos past each consumed digit and ';' byte.  Stops when the
 * byte at @p pos is not a digit or ';', or when @p pos >= @p length.
 *
 * @return Number of params written into @p params (0 if none found).
 */
int SixelDecoder::parseParams (const uint8_t* data, size_t length, size_t& pos,
                               int* params, int maxParams) noexcept
{
    int count { 0 };
    bool inNumber { false };

    while (pos < length and count < maxParams)
    {
        const uint8_t byte { data[pos] };

        if (byte >= '0' and byte <= '9')
        {
            if (not inNumber)
            {
                params[count] = 0;
                inNumber = true;
            }

            params[count] = params[count] * 10 + static_cast<int> (byte - '0');
            ++pos;
        }
        else if (byte == ';')
        {
            if (not inNumber)
            {
                params[count] = 0;
            }

            ++count;
            inNumber = false;
            ++pos;
        }
        else
        {
            break;
        }
    }

    if (inNumber)
    {
        ++count;
    }

    return count;
}

/**
 * @brief Standard HLS→RGB conversion.
 *
 * H in [0,360], L in [0,100], S in [0,100].  Output r,g,b in [0,255].
 * Uses the two-step hue-to-RGB algorithm (HLS cone model).
 */
void SixelDecoder::hlsToRgb (int h, int l, int s,
                              uint8_t& r, uint8_t& g, uint8_t& b) noexcept
{
    const float lf { static_cast<float> (l) / 100.0f };
    const float sf { static_cast<float> (s) / 100.0f };

    if (sf == 0.0f)
    {
        const uint8_t grey { static_cast<uint8_t> (juce::roundToInt (lf * 255.0f)) };
        r = grey;
        g = grey;
        b = grey;
    }
    else
    {
        const float q { lf < 0.5f
            ? lf * (1.0f + sf)
            : lf + sf - lf * sf };
        const float p  { 2.0f * lf - q };
        const float hf { static_cast<float> (h) / 360.0f };

        auto hue2rgb = [p, q] (float t) noexcept -> float
        {
            float tt { t };

            if (tt < 0.0f) tt += 1.0f;
            if (tt > 1.0f) tt -= 1.0f;

            float result { p };

            if (tt < 1.0f / 6.0f)      result = p + (q - p) * 6.0f * tt;
            else if (tt < 1.0f / 2.0f) result = q;
            else if (tt < 2.0f / 3.0f) result = p + (q - p) * (2.0f / 3.0f - tt) * 6.0f;

            return result;
        };

        r = static_cast<uint8_t> (juce::roundToInt (hue2rgb (hf + 1.0f / 3.0f) * 255.0f));
        g = static_cast<uint8_t> (juce::roundToInt (hue2rgb (hf)               * 255.0f));
        b = static_cast<uint8_t> (juce::roundToInt (hue2rgb (hf - 1.0f / 3.0f) * 255.0f));
    }
}

/**
 * @brief Ensures the pixel buffer is at least @p requiredW × @p requiredH pixels.
 *
 * Doubles the exceeded dimension(s), allocates a new zero-filled buffer, and
 * copies existing pixels row-by-row from the old buffer.  On allocation failure,
 * returns false and leaves the original buffer intact.
 */
bool SixelDecoder::growBuffer (juce::HeapBlock<uint8_t>& rgba,
                               int& bufferW, int& bufferH,
                               int requiredW, int requiredH) noexcept
{
    const int newW { requiredW > bufferW ? juce::jmax (requiredW, bufferW * 2) : bufferW };
    const int newH { requiredH > bufferH ? juce::jmax (requiredH, bufferH * 2) : bufferH };

    juce::HeapBlock<uint8_t> newBuf;
    newBuf.calloc (static_cast<size_t> (newW) * static_cast<size_t> (newH) * 4u);

    if (newBuf.get() == nullptr)
    {
        return false;
    }

    const int copyW { juce::jmin (bufferW, newW) };
    const int copyH { juce::jmin (bufferH, newH) };

    for (int row { 0 }; row < copyH; ++row)
    {
        const int srcOff  { row * bufferW * 4 };
        const int destOff { row * newW    * 4 };
        std::memcpy (newBuf.get() + destOff,
                     rgba.get()  + srcOff,
                     static_cast<size_t> (copyW) * 4u);
    }

    rgba    = std::move (newBuf);
    bufferW = newW;
    bufferH = newH;

    return true;
}

/**
 * @brief Writes the 6 pixels encoded in @p sixelByte at column @p x, row @p y.
 *
 * Bit 0 of @p sixelByte maps to the top pixel; bit 5 to the bottom.  Pixels
 * beyond [0, bufferW) × [0, bufferH) are silently skipped.
 */
void SixelDecoder::writeSixelByte (uint8_t* rgba, int bufferW, int bufferH,
                                   int x, int y, uint8_t sixelByte,
                                   const Colour& colour) noexcept
{
    const int bits { sixelByte - 0x3F };

    for (int bit { 0 }; bit < 6; ++bit)
    {
        if ((bits & (1 << bit)) != 0)
        {
            const int py { y + bit };

            if (py >= 0 and py < bufferH and x >= 0 and x < bufferW)
            {
                const int offset { (py * bufferW + x) * 4 };
                rgba[offset + 0] = colour.r;
                rgba[offset + 1] = colour.g;
                rgba[offset + 2] = colour.b;
                rgba[offset + 3] = colour.a;
            }
        }
    }
}

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Terminal
