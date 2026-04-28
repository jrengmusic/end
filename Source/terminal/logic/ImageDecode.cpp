/**
 * @file ImageDecode.cpp
 * @brief Platform-independent BGRA-to-RGBA conversion.
 *
 * @see ImageDecode.h
 */

#include "ImageDecode.h"
#include "ImageDecodeGif.h"

namespace Terminal
{ /*____________________________________________________________________________*/

bool swizzleARGBToRGBA (const juce::Image& image,
                        juce::HeapBlock<uint8_t>& rgba,
                        int& width, int& height) noexcept
{
    const int w { image.getWidth() };
    const int h { image.getHeight() };

    if (w > 0 and h > 0)
    {
        const size_t totalBytes { static_cast<size_t> (w) * static_cast<size_t> (h) * 4u };
        rgba.allocate (totalBytes, false);

        const juce::Image::BitmapData bm (image, juce::Image::BitmapData::readOnly);
        uint8_t* dst { rgba.get() };

        for (int row { 0 }; row < h; ++row)
        {
            const uint8_t* src { bm.getLinePointer (row) };

            for (int col { 0 }; col < w; ++col)
            {
                // JUCE ARGB in memory: B(0) G(1) R(2) A(3)
                const uint8_t b { src[0] };
                const uint8_t g { src[1] };
                const uint8_t r { src[2] };
                const uint8_t a { src[3] };

                // Un-premultiply: straight = premultiplied / alpha
                if (a > 0 and a < 255)
                {
                    dst[0] = static_cast<uint8_t> ((static_cast<unsigned> (r) * 255u) / static_cast<unsigned> (a));
                    dst[1] = static_cast<uint8_t> ((static_cast<unsigned> (g) * 255u) / static_cast<unsigned> (a));
                    dst[2] = static_cast<uint8_t> ((static_cast<unsigned> (b) * 255u) / static_cast<unsigned> (a));
                    dst[3] = a;
                }
                else
                {
                    // a == 0: fully transparent; a == 255: already straight
                    dst[0] = r;
                    dst[1] = g;
                    dst[2] = b;
                    dst[3] = a;
                }

                src += 4;
                dst += 4;
            }
        }

        width  = w;
        height = h;
    }

    return w > 0 and h > 0;
}

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Terminal
