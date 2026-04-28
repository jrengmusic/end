/**
 * @file ImageDecodeMac.mm
 * @brief macOS native image decoding via CoreGraphics + ImageIO.
 *
 * @see ImageDecode.h
 */

#include "ImageDecode.h"

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

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Terminal

#endif // JUCE_MAC || JUCE_IOS
