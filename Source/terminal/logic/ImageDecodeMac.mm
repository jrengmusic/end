/**
 * @file ImageDecodeMac.mm
 * @brief macOS native image decoding via CoreGraphics + ImageIO.
 *
 * @see ImageDecode.h
 */

#include "ImageDecode.h"
#include "ImageDecodeGif.h"

#if JUCE_MAC || JUCE_IOS

#include <CoreGraphics/CoreGraphics.h>
#include <ImageIO/ImageIO.h>

namespace Terminal
{ /*____________________________________________________________________________*/

juce::Image loadImageNative (const void* data, size_t dataSize) noexcept
{
    juce::Image result;

    CFDataRef cfData { CFDataCreateWithBytesNoCopy (kCFAllocatorDefault,
                                                     static_cast<const UInt8*> (data),
                                                     static_cast<CFIndex> (dataSize),
                                                     kCFAllocatorNull) };

    if (cfData != nullptr)
    {
        CGImageSourceRef source { CGImageSourceCreateWithData (cfData, nullptr) };

        if (source != nullptr)
        {
            CGImageRef cgImage { CGImageSourceCreateImageAtIndex (source, 0, nullptr) };

            if (cgImage != nullptr)
            {
                const int w { static_cast<int> (CGImageGetWidth (cgImage)) };
                const int h { static_cast<int> (CGImageGetHeight (cgImage)) };

                if (w > 0 and h > 0)
                {
                    result = juce::Image (juce::Image::ARGB, w, h, true);
                    juce::Image::BitmapData bm (result, juce::Image::BitmapData::writeOnly);

                    CGColorSpaceRef colorSpace { CGColorSpaceCreateDeviceRGB() };
                    CGContextRef ctx { CGBitmapContextCreate (
                        bm.getLinePointer (0),
                        static_cast<size_t> (w),
                        static_cast<size_t> (h),
                        8,
                        static_cast<size_t> (bm.lineStride),
                        colorSpace,
                        kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Little) };

                    if (ctx != nullptr)
                    {
                        CGContextDrawImage (ctx, CGRectMake (0, 0, w, h), cgImage);
                        CGContextRelease (ctx);
                    }

                    CGColorSpaceRelease (colorSpace);
                }

                CGImageRelease (cgImage);
            }

            CFRelease (source);
        }

        CFRelease (cfData);
    }

    return result;
}

ImageSequence loadImageSequenceNative (const void* data, size_t dataSize) noexcept
{
    ImageSequence seq;

    const bool isGif { dataSize >= 6
                       and static_cast<const char*> (data)[0] == 'G'
                       and static_cast<const char*> (data)[1] == 'I'
                       and static_cast<const char*> (data)[2] == 'F' };

    if (not isGif)
    {
        // Non-GIF: single frame via existing path
        juce::Image img { loadImageNative (data, dataSize) };

        if (img.isValid())
        {
            seq.width      = img.getWidth();
            seq.height     = img.getHeight();
            seq.frameCount = 1;
            int w { 0 };
            int h { 0 };
            swizzleARGBToRGBA (img, seq.pixels, w, h);
        }

        return seq;
    }

    // GIF path: parse metadata then compose frames
    const GifMetadata meta { parseGifMetadata (static_cast<const uint8_t*> (data), dataSize) };

    if (meta.isValid and not meta.frames.empty())
    {
        CFDataRef cfData { CFDataCreateWithBytesNoCopy (kCFAllocatorDefault,
                                                         static_cast<const UInt8*> (data),
                                                         static_cast<CFIndex> (dataSize),
                                                         kCFAllocatorNull) };

        if (cfData != nullptr)
        {
            CGImageSourceRef source { CGImageSourceCreateWithData (cfData, nullptr) };

            if (source != nullptr)
            {
                const size_t nativeFrameCount { CGImageSourceGetCount (source) };
                const int frameCount { static_cast<int> (
                    juce::jmin (nativeFrameCount, meta.frames.size())) };

                const int canvasW { meta.canvasWidth };
                const int canvasH { meta.canvasHeight };
                const size_t frameBytes { static_cast<size_t> (canvasW)
                                          * static_cast<size_t> (canvasH) * 4u };
                const size_t totalBytes { frameBytes * static_cast<size_t> (frameCount) };

                seq.pixels.allocate (totalBytes, true);   // zero-init = transparent canvas
                seq.delays.allocate (static_cast<size_t> (frameCount), false);

                // Working canvas — represents the composed image between frames
                juce::HeapBlock<uint8_t> canvas (frameBytes, true);
                // Saved canvas for disposal-3 (restore-to-previous)
                juce::HeapBlock<uint8_t> previousCanvas (frameBytes, true);

                for (int i { 0 }; i < frameCount; ++i)
                {
                    const GifFrameInfo& prev { meta.frames[static_cast<size_t> (i > 0 ? i - 1 : 0)] };

                    // Apply disposal of the previous frame before drawing this one
                    if (i > 0)
                    {
                        if (prev.disposal == 2)
                        {
                            // Restore-to-background: clear the previous frame rect on canvas
                            const int clearLeft   { juce::jlimit (0, canvasW, prev.left) };
                            const int clearTop    { juce::jlimit (0, canvasH, prev.top) };
                            const int clearRight  { juce::jlimit (0, canvasW, prev.left + prev.width) };
                            const int clearBottom { juce::jlimit (0, canvasH, prev.top + prev.height) };

                            for (int row { clearTop }; row < clearBottom; ++row)
                            {
                                uint8_t* dst { canvas.get()
                                               + static_cast<size_t> (row * canvasW + clearLeft) * 4u };
                                std::memset (dst, 0, static_cast<size_t> (clearRight - clearLeft) * 4u);
                            }
                        }
                        else if (prev.disposal == 3)
                        {
                            // Restore-to-previous: swap back the saved canvas
                            std::memcpy (canvas.get(), previousCanvas.get(), frameBytes);
                        }
                        // disposal 0 or 1: leave canvas as-is
                    }

                    const GifFrameInfo& fi { meta.frames[static_cast<size_t> (i)] };

                    // Save canvas before this frame if disposal is restore-to-previous
                    if (fi.disposal == 3)
                        std::memcpy (previousCanvas.get(), canvas.get(), frameBytes);

                    // Extract sub-frame from CGImageSource
                    CGImageRef cgImage { CGImageSourceCreateImageAtIndex (
                        source, static_cast<size_t> (i), nullptr) };

                    if (cgImage != nullptr)
                    {
                        const int subW { static_cast<int> (CGImageGetWidth (cgImage)) };
                        const int subH { static_cast<int> (CGImageGetHeight (cgImage)) };

                        if (subW > 0 and subH > 0)
                        {
                            // Render sub-frame to a temp JUCE image via CGBitmapContext
                            juce::Image subImg (juce::Image::ARGB, subW, subH, true);
                            juce::Image::BitmapData bm (subImg, juce::Image::BitmapData::writeOnly);

                            CGColorSpaceRef colorSpace { CGColorSpaceCreateDeviceRGB() };
                            CGContextRef ctx { CGBitmapContextCreate (
                                bm.getLinePointer (0),
                                static_cast<size_t> (subW),
                                static_cast<size_t> (subH),
                                8,
                                static_cast<size_t> (bm.lineStride),
                                colorSpace,
                                kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Little) };

                            if (ctx != nullptr)
                            {
                                CGContextDrawImage (ctx, CGRectMake (0, 0, subW, subH), cgImage);
                                CGContextRelease (ctx);
                            }

                            CGColorSpaceRelease (colorSpace);

                            // Swizzle sub-frame to straight RGBA
                            juce::HeapBlock<uint8_t> subRgba;
                            int swW { 0 };
                            int swH { 0 };
                            swizzleARGBToRGBA (subImg, subRgba, swW, swH);

                            // Blit sub-frame onto canvas at (fi.left, fi.top)
                            // Transparent pixels (alpha == 0) do not overwrite canvas
                            const int blitLeft   { juce::jlimit (0, canvasW, fi.left) };
                            const int blitTop    { juce::jlimit (0, canvasH, fi.top) };
                            const int blitRight  { juce::jlimit (0, canvasW, fi.left + subW) };
                            const int blitBottom { juce::jlimit (0, canvasH, fi.top + subH) };

                            for (int row { blitTop }; row < blitBottom; ++row)
                            {
                                const int srcRow { row - fi.top };
                                const uint8_t* src { subRgba.get()
                                                     + static_cast<size_t> (srcRow * subW
                                                                             + (blitLeft - fi.left)) * 4u };
                                uint8_t* dst { canvas.get()
                                               + static_cast<size_t> (row * canvasW + blitLeft) * 4u };

                                for (int col { blitLeft }; col < blitRight; ++col)
                                {
                                    if (src[3] > 0)
                                    {
                                        dst[0] = src[0];
                                        dst[1] = src[1];
                                        dst[2] = src[2];
                                        dst[3] = src[3];
                                    }

                                    src += 4;
                                    dst += 4;
                                }
                            }
                        }

                        CGImageRelease (cgImage);
                    }

                    // Copy composed canvas to output frame slot
                    std::memcpy (seq.pixels.get() + static_cast<size_t> (i) * frameBytes,
                                 canvas.get(),
                                 frameBytes);

                    seq.delays[i] = fi.delayMs;
                }

                seq.width      = canvasW;
                seq.height     = canvasH;
                seq.frameCount = frameCount;

                CFRelease (source);
            }

            CFRelease (cfData);
        }
    }

    return seq;
}

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Terminal

#endif // JUCE_MAC || JUCE_IOS
