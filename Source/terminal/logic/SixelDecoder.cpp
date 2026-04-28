/**
 * @file SixelDecoder.cpp
 * @brief Public API implementation of Terminal::SixelDecoder — decode() entry point.
 *
 * High-level orchestration: raster attribute pre-scan, buffer allocation, palette
 * initialisation, parse loop dispatch, and final crop.  All parsing helpers live in
 * SixelDecoderParse.cpp.
 *
 * @see SixelDecoder.h
 * @see SixelDecoderParse.cpp
 */

#include "SixelDecoder.h"

namespace Terminal
{ /*____________________________________________________________________________*/

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
