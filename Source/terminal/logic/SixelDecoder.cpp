/**
 * @file SixelDecoder.cpp
 * @brief Implementation of Terminal::SixelDecoder.
 *
 * @see SixelDecoder.h
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

// ============================================================================
// SixelDecoder::decode
// ============================================================================

/**
 * @brief Decode a complete Sixel payload into RGBA pixels.
 *
 * ### Algorithm
 * 1. First-pass scan: locate raster attributes `"Ph;Pv` for pre-allocation.
 *    If none, start with a default 800×600 buffer.
 * 2. Initialise palette from VT340 defaults.
 * 3. Allocate RGBA buffer (zero-filled = transparent black).
 * 4. Parse loop: dispatch on byte:
 *    - `#`  → colour define/select
 *    - `!`  → repeat
 *    - `$`  → carriage return
 *    - `-`  → newline (+6 rows)
 *    - `"`  → raster attributes (skip, already scanned)
 *    - 0x3F–0x7E → sixel data; grow buffer if needed
 * 5. Crop result to actual content bounds (cursorX, cursorY + 6).
 *
 * @return DecodedImage with RGBA8 pixels, or invalid on allocation failure.
 * @note READER THREAD only.
 */
DecodedImage SixelDecoder::decode (const uint8_t* data, size_t length) noexcept
{
    DecodedImage result;

    if (data == nullptr or length == 0)
    {
        return result;
    }

    // ------------------------------------------------------------------
    // First pass: scan for raster attributes '"Pan;Pad;Ph;Pv' to get
    // declared image dimensions for pre-allocation.
    // ------------------------------------------------------------------

    int declaredWidth  { 0 };
    int declaredHeight { 0 };

    for (size_t i { 0 }; i < length; ++i)
    {
        if (data[i] == '"')
        {
            size_t scanPos { i + 1 };
            int rasterParams[4] { 0, 0, 0, 0 };
            const int rasterCount { parseParams (data, length, scanPos, rasterParams, 4) };

            if (rasterCount >= 4)
            {
                declaredWidth  = rasterParams[2];
                declaredHeight = rasterParams[3];
            }

            break;
        }
    }

    // ------------------------------------------------------------------
    // Allocate working buffer.  Default 800×600 when no raster attrs.
    // ------------------------------------------------------------------

    static constexpr int defaultWidth  { 800 };
    static constexpr int defaultHeight { 600 };

    int bufferW { declaredWidth  > 0 ? declaredWidth  : defaultWidth  };
    int bufferH { declaredHeight > 0 ? declaredHeight : defaultHeight };

    juce::HeapBlock<uint8_t> rgba;
    rgba.calloc (static_cast<size_t> (bufferW) * static_cast<size_t> (bufferH) * 4u);

    if (rgba.get() == nullptr)
    {
        return result;
    }

    // ------------------------------------------------------------------
    // Initialise palette from VT340 defaults.
    // ------------------------------------------------------------------

    Colour palette[maxPalette];

    for (int i { 0 }; i < maxPalette; ++i)
    {
        if (i < 16)
        {
            palette[i] = defaultPalette[i];
        }
        else
        {
            palette[i] = Colour { 0, 0, 0, 255 };
        }
    }

    // ------------------------------------------------------------------
    // Parse loop.
    // ------------------------------------------------------------------

    int cursorX        { 0 };
    int cursorY        { 0 };
    int currentColour  { 0 };
    int maxX           { 0 };
    int maxY           { 0 };

    size_t pos { 0 };

    while (pos < length)
    {
        const uint8_t byte { data[pos] };

        if (byte == '#')
        {
            ++pos;
            int colParams[5] { 0, 0, 0, 0, 0 };
            const int colCount { parseParams (data, length, pos, colParams, 5) };

            const int register_ { colParams[0] };

            if (register_ >= 0 and register_ < maxPalette)
            {
                if (colCount >= 5)
                {
                    // Colour definition
                    const int model { colParams[1] };
                    const int px    { colParams[2] };
                    const int py    { colParams[3] };
                    const int pz    { colParams[4] };

                    if (model == 1)
                    {
                        // HLS: px=Hue 0-360, py=Lightness 0-100, pz=Saturation 0-100
                        uint8_t r { 0 };
                        uint8_t g { 0 };
                        uint8_t b { 0 };
                        hlsToRgb (px, py, pz, r, g, b);
                        palette[register_] = { r, g, b, 255 };
                    }
                    else if (model == 2)
                    {
                        // RGB: values 0-100 scaled to 0-255
                        palette[register_] =
                        {
                            static_cast<uint8_t> (juce::jlimit (0, 255, px * 255 / 100)),
                            static_cast<uint8_t> (juce::jlimit (0, 255, py * 255 / 100)),
                            static_cast<uint8_t> (juce::jlimit (0, 255, pz * 255 / 100)),
                            255
                        };
                    }

                    currentColour = register_;
                }
                else if (colCount >= 1)
                {
                    // Colour selection only
                    currentColour = register_;
                }
            }
        }
        else if (byte == '!')
        {
            // Repeat: "!Pn c"
            ++pos;
            int repeatParams[1] { 0 };
            parseParams (data, length, pos, repeatParams, 1);

            const int repeatCount { repeatParams[0] };

            if (pos < length)
            {
                const uint8_t sixelChar { data[pos] };
                ++pos;

                if (sixelChar >= 0x3F and sixelChar <= 0x7E)
                {
                    const int needed { cursorX + repeatCount };

                    if (needed > bufferW or cursorY + 6 > bufferH)
                    {
                        const bool grew { growBuffer (rgba, bufferW, bufferH,
                                                      juce::jmax (bufferW, needed + 1),
                                                      juce::jmax (bufferH, cursorY + 6 + 1)) };

                        if (not grew)
                        {
                            break;
                        }
                    }

                    for (int rep { 0 }; rep < repeatCount; ++rep)
                    {
                        writeSixelByte (rgba.get(), bufferW, bufferH,
                                        cursorX + rep, cursorY,
                                        sixelChar, palette[currentColour]);
                    }

                    cursorX += repeatCount;

                    if (cursorX > maxX) maxX = cursorX;
                    if (cursorY + 6 > maxY) maxY = cursorY + 6;
                }
            }
        }
        else if (byte == '$')
        {
            cursorX = 0;
            ++pos;
        }
        else if (byte == '-')
        {
            cursorX  = 0;
            cursorY += 6;
            ++pos;
        }
        else if (byte == '"')
        {
            // Raster attributes: already scanned, just skip the params
            ++pos;
            int rasterParams[4] { 0, 0, 0, 0 };
            parseParams (data, length, pos, rasterParams, 4);
        }
        else if (byte >= 0x3F and byte <= 0x7E)
        {
            // Sixel data byte
            if (cursorX + 1 > bufferW or cursorY + 6 > bufferH)
            {
                const bool grew { growBuffer (rgba, bufferW, bufferH,
                                              juce::jmax (bufferW, cursorX + 2),
                                              juce::jmax (bufferH, cursorY + 7)) };

                if (not grew)
                {
                    break;
                }
            }

            writeSixelByte (rgba.get(), bufferW, bufferH,
                            cursorX, cursorY, byte, palette[currentColour]);
            ++cursorX;

            if (cursorX > maxX)  maxX = cursorX;
            if (cursorY + 6 > maxY) maxY = cursorY + 6;

            ++pos;
        }
        else
        {
            ++pos;
        }
    }

    // ------------------------------------------------------------------
    // Determine actual content bounds.
    // ------------------------------------------------------------------

    const int finalW { declaredWidth  > 0 ? declaredWidth  : maxX };
    const int finalH { declaredHeight > 0 ? declaredHeight : maxY };

    if (finalW <= 0 or finalH <= 0)
    {
        return result;
    }

    // ------------------------------------------------------------------
    // Crop to finalW × finalH if smaller than working buffer.
    // ------------------------------------------------------------------

    if (finalW < bufferW or finalH < bufferH)
    {
        juce::HeapBlock<uint8_t> cropped;
        cropped.calloc (static_cast<size_t> (finalW) * static_cast<size_t> (finalH) * 4u);

        if (cropped.get() == nullptr)
        {
            return result;
        }

        for (int row { 0 }; row < finalH; ++row)
        {
            const int srcOff  { row * bufferW * 4 };
            const int destOff { row * finalW  * 4 };
            std::memcpy (cropped.get() + destOff,
                         rgba.get()   + srcOff,
                         static_cast<size_t> (finalW) * 4u);
        }

        rgba = std::move (cropped);
    }

    result.rgba   = std::move (rgba);
    result.width  = finalW;
    result.height = finalH;

    return result;
}

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Terminal
