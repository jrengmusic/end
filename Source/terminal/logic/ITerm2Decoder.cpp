/**
 * @file ITerm2Decoder.cpp
 * @brief Implementation of Terminal::ITerm2Decoder.
 *
 * @see ITerm2Decoder.h
 */

#include "ITerm2Decoder.h"

#if JUCE_MAC || JUCE_IOS
#include <CoreGraphics/CoreGraphics.h>
#include <ImageIO/ImageIO.h>
#endif

#if JUCE_WINDOWS
#include <wincodec.h>
#include <combaseapi.h>
#endif

namespace Terminal
{ /*____________________________________________________________________________*/

// ============================================================================
// loadImageNative — platform-native image decode fallback
// ============================================================================

/**
 * @brief Decode image bytes using platform-native APIs.
 *
 * Used as a fallback when JUCE's `ImageFileFormat::loadFrom()` fails.
 * Handles formats JUCE does not support: TIFF, BMP, WebP, HEIC, ICO, etc.
 *
 * The returned juce::Image is in ARGB format with premultiplied alpha,
 * matching JUCE's internal layout so the existing swizzle code applies
 * without modification.
 *
 * @param data      Pointer to raw image file bytes.
 * @param dataSize  Number of bytes.
 * @return Valid juce::Image on success, or invalid image on failure.
 *
 * @note macOS: CoreGraphics/ImageIO — CGBitmapContextCreate with
 *       kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Little
 *       produces the same BGRA premultiplied layout JUCE uses for ARGB.
 * @note Windows: WIC with GUID_WICPixelFormat32bppBGRA matches JUCE's
 *       internal ARGB layout on little-endian.
 */

#if JUCE_MAC || JUCE_IOS

static juce::Image loadImageNative (const void* data, size_t dataSize) noexcept
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

#elif JUCE_WINDOWS

static juce::Image loadImageNative (const void* data, size_t dataSize) noexcept
{
    juce::Image result;

    IWICImagingFactory* factory { nullptr };
    HRESULT hr { CoCreateInstance (CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                    IID_PPV_ARGS (&factory)) };

    if (SUCCEEDED (hr) and factory != nullptr)
    {
        IStream* stream { SHCreateMemStream (static_cast<const BYTE*> (data),
                                              static_cast<UINT> (dataSize)) };

        if (stream != nullptr)
        {
            IWICBitmapDecoder* decoder { nullptr };
            hr = factory->CreateDecoderFromStream (stream, nullptr,
                                                    WICDecodeMetadataCacheOnDemand, &decoder);

            if (SUCCEEDED (hr) and decoder != nullptr)
            {
                IWICBitmapFrameDecode* frame { nullptr };
                hr = decoder->GetFrame (0, &frame);

                if (SUCCEEDED (hr) and frame != nullptr)
                {
                    UINT w { 0 };
                    UINT h { 0 };
                    frame->GetSize (&w, &h);

                    if (w > 0 and h > 0)
                    {
                        IWICFormatConverter* converter { nullptr };
                        hr = factory->CreateFormatConverter (&converter);

                        if (SUCCEEDED (hr) and converter != nullptr)
                        {
                            hr = converter->Initialize (frame, GUID_WICPixelFormat32bppBGRA,
                                                        WICBitmapDitherTypeNone, nullptr,
                                                        0.0, WICBitmapPaletteTypeCustom);

                            if (SUCCEEDED (hr))
                            {
                                result = juce::Image (juce::Image::ARGB, static_cast<int> (w),
                                                      static_cast<int> (h), true);
                                juce::Image::BitmapData bm (result, juce::Image::BitmapData::writeOnly);

                                converter->CopyPixels (nullptr,
                                                        static_cast<UINT> (bm.lineStride),
                                                        static_cast<UINT> (bm.lineStride) * h,
                                                        reinterpret_cast<BYTE*> (bm.getLinePointer (0)));
                            }

                            converter->Release();
                        }
                    }

                    frame->Release();
                }

                decoder->Release();
            }

            stream->Release();
        }

        factory->Release();
    }

    return result;
}

#else

static juce::Image loadImageNative (const void*, size_t) noexcept
{
    return {};
}

#endif

// ============================================================================
// ITerm2Decoder::decode
// ============================================================================

/**
 * @brief Decode an OSC 1337 File= payload into RGBA pixels.
 *
 * Parse steps:
 *  1. Validate and skip the "File=" prefix.
 *  2. Scan key=value params up to the ':' delimiter, extracting `inline`.
 *  3. Return invalid when `inline != 1`.
 *  4. Base64-decode everything after ':'.
 *  5. Load image via JUCE auto-detection (PNG / JPEG / GIF).
 *  6. Convert to ARGB format.
 *  7. Swizzle JUCE BGRA memory layout → straight RGBA8.
 *  8. Return populated DecodedImage.
 *
 * @param data    Raw OSC payload bytes after "1337;".
 * @param length  Number of bytes.
 * @return DecodedImage with RGBA8 pixels, or invalid on any failure.
 *
 * @note READER THREAD only.
 */
DecodedImage ITerm2Decoder::decode (const uint8_t* data, int length) noexcept
{
    DecodedImage result;

    // -------------------------------------------------------------------------
    // 1. Validate and skip "File=" prefix
    // -------------------------------------------------------------------------
    static constexpr uint8_t filePrefix[] { 'F', 'i', 'l', 'e', '=' };
    static constexpr int filePrefixLen { 5 };

    if (length > filePrefixLen and std::memcmp (data, filePrefix, static_cast<size_t> (filePrefixLen)) == 0)
    {
        int pos { filePrefixLen };

        // -------------------------------------------------------------------------
        // 2. Parse key=value params up to ':' delimiter
        // -------------------------------------------------------------------------
        bool inlineDisplay { false };

        while (pos < length and data[pos] != ':')
        {
            // Find '=' for this key
            int keyStart { pos };
            while (pos < length and data[pos] != '=' and data[pos] != ':')
                ++pos;

            if (pos < length and data[pos] == '=')
            {
                const int keyLen { pos - keyStart };
                ++pos; // skip '='

                // Find end of value (';' or ':')
                const int valueStart { pos };
                while (pos < length and data[pos] != ';' and data[pos] != ':')
                    ++pos;

                const int valueLen { pos - valueStart };

                // Check for "inline" key
                static constexpr char inlineKey[] { 'i', 'n', 'l', 'i', 'n', 'e' };
                static constexpr int inlineKeyLen { 6 };

                if (keyLen == inlineKeyLen
                    and std::memcmp (data + keyStart, inlineKey, static_cast<size_t> (inlineKeyLen)) == 0
                    and valueLen > 0)
                {
                    inlineDisplay = (data[valueStart] == '1');
                }

                // Skip past ';' separator if present
                if (pos < length and data[pos] == ';')
                    ++pos;
            }
            else
            {
                // Malformed param (no '=' found before ':' or end) — skip to next delimiter
                if (pos < length and data[pos] != ':')
                    ++pos;
            }
        }

        // -------------------------------------------------------------------------
        // 3. Return invalid when inline != 1
        // -------------------------------------------------------------------------
        if (inlineDisplay and pos < length and data[pos] == ':')
        {
            ++pos; // skip ':'

            // -------------------------------------------------------------------------
            // 4. Base64-decode everything after ':'
            // -------------------------------------------------------------------------
            const int base64Len { length - pos };

            if (base64Len > 0)
            {
                const juce::String base64 (reinterpret_cast<const char*> (data + pos),
                                           static_cast<size_t> (base64Len));
                juce::MemoryOutputStream decoded;

                const bool base64Ok { juce::Base64::convertFromBase64 (decoded, base64) and decoded.getDataSize() > 0 };

                if (base64Ok)
                {
                    // -------------------------------------------------------------------------
                    // 5. Load image via JUCE auto-detection (PNG / JPEG / GIF)
                    // -------------------------------------------------------------------------

                    juce::Image image { juce::ImageFileFormat::loadFrom (decoded.getData(),
                                                                         decoded.getDataSize()) };

                    // Platform-native fallback for formats JUCE doesn't handle (TIFF, BMP, WebP, etc.)
                    if (not image.isValid())
                        image = loadImageNative (decoded.getData(), decoded.getDataSize());

                    if (image.isValid())
                    {
                        // -------------------------------------------------------------------------
                        // 6. Convert to ARGB format
                        // -------------------------------------------------------------------------
                        juce::Image argbImage { image.convertedToFormat (juce::Image::ARGB) };

                        const int w { argbImage.getWidth() };
                        const int h { argbImage.getHeight() };

                        if (w > 0 and h > 0)
                        {
                            // -------------------------------------------------------------------------
                            // 7. Swizzle JUCE BGRA memory layout -> straight RGBA8
                            //
                            // JUCE PixelARGB on little-endian: memory layout = B, G, R, A (packed 0xAARRGGBB).
                            // GL_RGBA + GL_UNSIGNED_BYTE expects: R, G, B, A.
                            // Un-premultiply alpha during swizzle.
                            // -------------------------------------------------------------------------
                            const size_t totalBytes { static_cast<size_t> (w) * static_cast<size_t> (h) * 4u };
                            result.rgba.allocate (totalBytes, false);

                            const juce::Image::BitmapData bm (argbImage, juce::Image::BitmapData::readOnly);

                            uint8_t* dst { result.rgba.get() };

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

                            // -------------------------------------------------------------------------
                            // 8. Populate result
                            // -------------------------------------------------------------------------
                            result.width  = w;
                            result.height = h;
                        }
                    }
                }
            }
        }
    }

    return result;
}

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Terminal
