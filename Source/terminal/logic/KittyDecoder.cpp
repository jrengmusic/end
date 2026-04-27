/**
 * @file KittyDecoder.cpp
 * @brief Implementation of Terminal::KittyDecoder.
 *
 * @see KittyDecoder.h
 */

#include "KittyDecoder.h"

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
// Parsed Kitty command key=value params
// ============================================================================

/**
 * @struct KittyParams
 * @brief Parsed Kitty Graphics Protocol key=value parameters.
 *
 * All fields default to the spec-mandated defaults.  `parseKittyParams()`
 * fills this struct by scanning the key=value region before the `;` delimiter.
 */
struct KittyParams
{
    char action { 'T' };           ///< a= : t, T (default), p, d, q
    int format { 32 };             ///< f= : 24, 32 (default), 100
    int pixelWidth { 0 };          ///< s=
    int pixelHeight { 0 };         ///< v=
    uint32_t imageId { 0 };        ///< i=
    int more { 0 };                ///< m= : 0 (default) = final chunk, 1 = more coming
    bool compressed { false };     ///< o= : 'z' => true
    int quiet { 0 };               ///< q= : 0 (default), 1 = suppress OK, 2 = suppress all
    int virtualPlacement { 0 };    ///< U= : 1 = virtual placement (unicode placeholder mode)
    int placementCols { 0 };       ///< c= : virtual placement column count
    int placementRows { 0 };       ///< r= : virtual placement row count
    int payloadStart { 0 };        ///< Byte offset of the base64 payload (after ';')
};

/**
 * @brief Parses the key=value region of a Kitty APC payload.
 *
 * Scans from byte 0 looking for comma-separated `key=value` pairs.  Stops
 * when a `;` delimiter is found (which marks the start of the base64 payload)
 * or when `length` is exhausted.  Single-character keys only; value is every
 * character up to the next `,` or `;`.
 *
 * @param data    Pointer to APC payload bytes (everything after 'G').
 * @param length  Number of bytes.
 * @return Populated KittyParams.  `payloadStart` is set to the index after
 *         the `;` delimiter, or `length` if no `;` was found.
 *
 * @note Pure function — no side effects.
 * @note READER THREAD only.
 */
static KittyParams parseKittyParams (const uint8_t* data, int length) noexcept
{
    KittyParams p;

    int i { 0 };

    while (i < length and data[i] != ';')
    {
        // Expect single-char key
        const char key { static_cast<char> (data[i]) };
        ++i;

        if (i < length and data[i] == '=')
        {
            ++i; // skip '='

            // Collect value up to ',' or ';'
            const int valueStart { i };

            while (i < length and data[i] != ',' and data[i] != ';')
                ++i;

            const int valueLen { i - valueStart };

            if (valueLen > 0)
            {
                // Parse integer value from ASCII digits
                int intVal { 0 };

                for (int v { valueStart }; v < valueStart + valueLen; ++v)
                {
                    if (data[v] >= '0' and data[v] <= '9')
                        intVal = intVal * 10 + (data[v] - '0');
                }

                const char firstChar { static_cast<char> (data[valueStart]) };

                switch (key)
                {
                    case 'a': p.action           = firstChar;                      break;
                    case 'f': p.format           = intVal;                         break;
                    case 's': p.pixelWidth       = intVal;                         break;
                    case 'v': p.pixelHeight      = intVal;                         break;
                    case 'i': p.imageId          = static_cast<uint32_t> (intVal); break;
                    case 'm': p.more             = intVal;                         break;
                    case 'o': p.compressed       = (firstChar == 'z');             break;
                    case 'q': p.quiet            = intVal;                         break;
                    case 'U': p.virtualPlacement = intVal;                         break;
                    case 'c': p.placementCols    = intVal;                         break;
                    case 'r': p.placementRows    = intVal;                         break;
                    default:                                                        break;
                }
            }
        }

        // Skip past ',' separator if present
        if (i < length and data[i] == ',')
            ++i;
    }

    // i now points at ';' or is at length
    if (i < length and data[i] == ';')
        p.payloadStart = i + 1;
    else
        p.payloadStart = length;

    return p;
}

/**
 * @brief Builds a Kitty OK response string for the given image ID and quiet level.
 *
 * Returns an empty string when `quiet >= 2` (suppress all).  Returns an empty
 * string when `quiet == 1` and `suppressOk` is true (suppress OK response).
 * Otherwise returns the APC-framed OK response.
 *
 * @param imageId    Kitty image ID to embed in the response.
 * @param quiet      Quiet level from the `q=` parameter.
 * @param suppressOk True to suppress OK-level responses (quiet=1 case).
 * @return APC-framed response string, or empty string if suppressed.
 *
 * @note Pure function — no side effects.
 */
static juce::String buildKittyResponse (uint32_t imageId, int quiet, bool suppressOk) noexcept
{
    juce::String result;

    if (quiet < 2)
    {
        if (not suppressOk or quiet == 0)
        {
            result = juce::String ("\x1b_Gi=")
                   + juce::String (static_cast<int> (imageId))
                   + juce::String (";OK\x1b\\");
        }
    }

    return result;
}

/**
 * @brief Builds a Kitty ERROR response string for the given image ID and quiet level.
 *
 * Returns an empty string when `quiet >= 2` (suppress all).
 *
 * @param imageId  Kitty image ID to embed in the response.
 * @param quiet    Quiet level from the `q=` parameter.
 * @return APC-framed error response string, or empty string if suppressed.
 *
 * @note Pure function — no side effects.
 */
static juce::String buildKittyErrorResponse (uint32_t imageId, int quiet) noexcept
{
    juce::String result;

    if (quiet < 2)
    {
        result = juce::String ("\x1b_Gi=")
               + juce::String (static_cast<int> (imageId))
               + juce::String (";ERROR\x1b\\");
    }

    return result;
}

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
// KittyDecoder::decodePayload
// ============================================================================

/**
 * @brief Decode a Kitty image payload to RGBA8 pixels.
 *
 * Steps:
 *  1. Base64-decode the raw bytes via `juce::MemoryBlock::fromBase64Encoding()`.
 *  2. If compressed (o=z): decompress with `GZIPDecompressorInputStream` (zlib).
 *  3. Auto-detect PNG/JPEG: if format is f=32 or f=24 AND dimensions are unknown
 *     (pixelWidth==0 or pixelHeight==0), inspect the magic bytes and override
 *     format to 100 so JUCE handles the container decode.
 *  4. Dispatch on format:
 *     - f=100 (PNG/JPEG/auto): JUCE image load → ARGB convert → BGRA→RGBA swizzle + un-premultiply.
 *     - f=32  (RGBA): raw pixels, copy directly.
 *     - f=24  (RGB): raw pixels, expand to RGBA (alpha=255).
 *  5. Return populated DecodedImage, or invalid on any failure.
 *
 * @param base64Data    Pointer to base64-encoded bytes.
 * @param base64Length  Number of base64 bytes.
 * @param format        Image format: 24=RGB, 32=RGBA, 100=PNG.
 * @param pixelWidth    Declared pixel width (required for f=24/32).
 * @param pixelHeight   Declared pixel height (required for f=24/32).
 * @param compressed    True if payload is zlib-compressed before base64.
 * @return Populated DecodedImage on success, or invalid on any failure.
 *
 * @note READER THREAD only.
 */
DecodedImage KittyDecoder::decodePayload (const uint8_t* base64Data, int base64Length,
                                          int format, int pixelWidth, int pixelHeight,
                                          bool compressed) noexcept
{
    DecodedImage result;

    // -------------------------------------------------------------------------
    // 1. Base64 decode
    // -------------------------------------------------------------------------
    const juce::String base64 (reinterpret_cast<const char*> (base64Data),
                                static_cast<size_t> (base64Length));
    juce::MemoryOutputStream decoded;

    if (juce::Base64::convertFromBase64 (decoded, base64) and decoded.getDataSize() > 0)
    {
        // -------------------------------------------------------------------------
        // 2. Optional zlib decompression
        // -------------------------------------------------------------------------
        juce::MemoryBlock decompressed;

        if (compressed)
        {
            juce::MemoryInputStream compressedStream (decoded.getData(), decoded.getDataSize(), false);
            juce::GZIPDecompressorInputStream decompressor (&compressedStream,
                                                            false,
                                                            juce::GZIPDecompressorInputStream::zlibFormat);

            // Read in chunks until stream exhausted
            static constexpr int chunkSize { 65536 };
            juce::HeapBlock<uint8_t> chunkBuf;
            chunkBuf.allocate (static_cast<size_t> (chunkSize), false);

            int bytesRead { decompressor.read (chunkBuf.get(), chunkSize) };

            while (bytesRead > 0)
            {
                decompressed.append (chunkBuf.get(), static_cast<size_t> (bytesRead));
                bytesRead = decompressor.read (chunkBuf.get(), chunkSize);
            }
        }
        else
        {
            decompressed.append (decoded.getData(), decoded.getDataSize());
        }

        const uint8_t* pixelData { static_cast<const uint8_t*> (decompressed.getData()) };
        const size_t   pixelBytes { decompressed.getSize() };

        if (pixelBytes > 0)
        {
            // -------------------------------------------------------------------------
            // 3. Auto-detect PNG / JPEG when format is raw (f=32 or f=24) but
            //    dimensions were not provided — chafa omits s= and v= on data chunks.
            // -------------------------------------------------------------------------
            if ((format == 32 or format == 24) and (pixelWidth == 0 or pixelHeight == 0))
            {
                static constexpr uint8_t pngMagic[]  { 0x89, 0x50, 0x4E, 0x47 };
                static constexpr uint8_t jpegMagic[] { 0xFF, 0xD8 };

                const bool looksLikePng  { pixelBytes >= 4
                                           and std::memcmp (pixelData, pngMagic,  4) == 0 };
                const bool looksLikeJpeg { pixelBytes >= 2
                                           and std::memcmp (pixelData, jpegMagic, 2) == 0 };

                if (looksLikePng or looksLikeJpeg)
                    format = 100; // JUCE auto-detects PNG / JPEG / GIF
            }

            // -------------------------------------------------------------------------
            // 4. Dispatch on format
            // -------------------------------------------------------------------------
            if (format == 100)
            {
                // PNG/JPEG/auto: JUCE image load → ARGB → BGRA swizzle + un-premultiply → straight RGBA8
                juce::Image image { juce::ImageFileFormat::loadFrom (pixelData, pixelBytes) };

                // Platform-native fallback for formats JUCE doesn't handle (TIFF, BMP, WebP, etc.)
                if (not image.isValid())
                {
                    jam::debug::Log::write ("KittyDecoder: JUCE load failed, trying platform-native decode");
                    image = loadImageNative (pixelData, pixelBytes);
                    jam::debug::Log::write ("KittyDecoder: native decode valid=" + juce::String (image.isValid() ? "yes" : "no"));
                }

                if (image.isValid())
                {
                    juce::Image argbImage { image.convertedToFormat (juce::Image::ARGB) };

                    const int w { argbImage.getWidth() };
                    const int h { argbImage.getHeight() };

                    if (w > 0 and h > 0)
                    {
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

                        result.width  = w;
                        result.height = h;
                    }
                }
            }
            else if (format == 32)
            {
                // f=32 RGBA: raw 4-byte-per-pixel, copy directly
                if (pixelWidth > 0 and pixelHeight > 0)
                {
                    const size_t expectedBytes { static_cast<size_t> (pixelWidth)
                                                 * static_cast<size_t> (pixelHeight) * 4u };

                    if (pixelBytes >= expectedBytes)
                    {
                        result.rgba.allocate (expectedBytes, false);
                        std::memcpy (result.rgba.get(), pixelData, expectedBytes);
                        result.width  = pixelWidth;
                        result.height = pixelHeight;
                    }
                }
            }
            else if (format == 24)
            {
                // f=24 RGB: raw 3-byte-per-pixel, expand to RGBA (alpha = 255)
                if (pixelWidth > 0 and pixelHeight > 0)
                {
                    const size_t expectedRgbBytes { static_cast<size_t> (pixelWidth)
                                                    * static_cast<size_t> (pixelHeight) * 3u };

                    if (pixelBytes >= expectedRgbBytes)
                    {
                        const size_t rgbaBytes { static_cast<size_t> (pixelWidth)
                                                 * static_cast<size_t> (pixelHeight) * 4u };
                        result.rgba.allocate (rgbaBytes, false);

                        const uint8_t* src { pixelData };
                        uint8_t* dst { result.rgba.get() };

                        const int pixelCount { pixelWidth * pixelHeight };

                        for (int px { 0 }; px < pixelCount; ++px)
                        {
                            dst[0] = src[0]; // R
                            dst[1] = src[1]; // G
                            dst[2] = src[2]; // B
                            dst[3] = 255;    // A = opaque

                            src += 3;
                            dst += 4;
                        }

                        result.width  = pixelWidth;
                        result.height = pixelHeight;
                    }
                }
            }
        }
    }

    return result;
}

// ============================================================================
// KittyDecoder::process
// ============================================================================

/**
 * @brief Process a complete APC G payload.
 *
 * Parse steps:
 *  1. Parse key=value params from the region before `;`.
 *  2. Extract base64 payload from the region after `;`.
 *  3. Dispatch on `a=` action:
 *     - q  (query):            Return isQuery + response.
 *     - d  (delete):           Clear storedImages, return isDelete.
 *     - t  (transmit-only):    Accumulate chunks; on m=0, decode and store.
 *     - T  (transmit+display): Accumulate chunks; on m=0, decode and return with shouldDisplay.
 *     - p  (display stored):   Look up storedImages[i], return shouldDisplay.
 *
 * @param data    Raw APC payload bytes (everything after 'G').
 * @param length  Number of bytes.
 * @return Result for this payload.  Defaults (shouldDisplay=false) for
 *         mid-sequence chunks and unrecognised actions.
 *
 * @note READER THREAD only.
 */
KittyDecoder::Result KittyDecoder::process (const uint8_t* data, int length) noexcept
{
    Result result;

    if (length > 0)
    {
        const KittyParams p { parseKittyParams (data, length) };

        jam::debug::Log::write ("KittyDecoder: action=" + juce::String::charToString (static_cast<juce::juce_wchar> (p.action))
            + " f=" + juce::String (p.format)
            + " s=" + juce::String (p.pixelWidth)
            + " v=" + juce::String (p.pixelHeight)
            + " i=" + juce::String (static_cast<int> (p.imageId))
            + " m=" + juce::String (p.more)
            + " o=" + juce::String (p.compressed ? "z" : "-")
            + " q=" + juce::String (p.quiet)
            + " U=" + juce::String (p.virtualPlacement)
            + " c=" + juce::String (p.placementCols)
            + " r=" + juce::String (p.placementRows)
            + " payloadStart=" + juce::String (p.payloadStart)
            + " payloadLen=" + juce::String (length - p.payloadStart)
            + " totalLen=" + juce::String (length));

        const uint8_t* payloadPtr { data + p.payloadStart };
        const int      payloadLen { length - p.payloadStart };

        // -------------------------------------------------------------------------
        // a=q — query: respond with OK to advertise Kitty support
        // -------------------------------------------------------------------------
        if (p.action == 'q')
        {
            result.isQuery      = true;
            result.kittyImageId = p.imageId;

            if (p.quiet < 2)
            {
                result.response = juce::String ("\x1b_Gi=")
                                + juce::String (static_cast<int> (p.imageId))
                                + juce::String (";OK\x1b\\");
            }
        }

        // -------------------------------------------------------------------------
        // a=d — delete: clear all stored images
        // -------------------------------------------------------------------------
        else if (p.action == 'd')
        {
            storedImages.clear();
            result.isDelete     = true;
            result.kittyImageId = p.imageId;
        }

        // -------------------------------------------------------------------------
        // a=t — transmit only: accumulate chunks, store on final
        // -------------------------------------------------------------------------
        else if (p.action == 't')
        {
            ChunkAccumulator& acc { chunks[p.imageId] };

            if (p.more == 1)
            {
                // Mid-sequence: accumulate and record metadata on first chunk
                if (not acc.hasMetadata)
                {
                    acc.format      = p.format;
                    acc.pixelWidth  = p.pixelWidth;
                    acc.pixelHeight = p.pixelHeight;
                    acc.compressed  = p.compressed;
                    acc.quiet       = p.quiet;
                    acc.hasMetadata = true;
                }

                if (payloadLen > 0)
                    acc.data.append (payloadPtr, static_cast<size_t> (payloadLen));
            }
            else
            {
                // Final chunk: append remaining payload, decode, store
                if (not acc.hasMetadata)
                {
                    // Single-chunk path: metadata comes from this packet
                    acc.format      = p.format;
                    acc.pixelWidth  = p.pixelWidth;
                    acc.pixelHeight = p.pixelHeight;
                    acc.compressed  = p.compressed;
                    acc.quiet       = p.quiet;
                    acc.hasMetadata = true;
                }

                if (payloadLen > 0)
                    acc.data.append (payloadPtr, static_cast<size_t> (payloadLen));

                const size_t totalBase64 { acc.data.getSize() };

                if (totalBase64 > 0)
                {
                    jam::debug::Log::write ("KittyDecoder: finalizing id=" + juce::String (static_cast<int> (p.imageId))
                        + " accSize=" + juce::String (static_cast<int> (acc.data.getSize()))
                        + " accFormat=" + juce::String (acc.format)
                        + " accW=" + juce::String (acc.pixelWidth)
                        + " accH=" + juce::String (acc.pixelHeight));

                    DecodedImage image { decodePayload (static_cast<const uint8_t*> (acc.data.getData()),
                                                        static_cast<int> (totalBase64),
                                                        acc.format,
                                                        acc.pixelWidth,
                                                        acc.pixelHeight,
                                                        acc.compressed) };

                    if (image.isValid())
                    {
                        StoredImage stored;
                        stored.width  = image.width;
                        stored.height = image.height;
                        stored.rgba   = std::move (image.rgba);
                        storedImages[p.imageId] = std::move (stored);

                        result.kittyImageId = p.imageId;
                        result.response     = buildKittyResponse (p.imageId, acc.quiet, false);
                    }
                    else
                    {
                        result.response = buildKittyErrorResponse (p.imageId, acc.quiet);
                    }
                }

                chunks.erase (p.imageId);
            }
        }

        // -------------------------------------------------------------------------
        // a=T — transmit + display: accumulate chunks, display on final
        // -------------------------------------------------------------------------
        else if (p.action == 'T')
        {
            ChunkAccumulator& acc { chunks[p.imageId] };

            if (p.more == 1)
            {
                // Mid-sequence: accumulate and record metadata on first chunk
                if (not acc.hasMetadata)
                {
                    acc.format      = p.format;
                    acc.pixelWidth  = p.pixelWidth;
                    acc.pixelHeight = p.pixelHeight;
                    acc.compressed  = p.compressed;
                    acc.quiet       = p.quiet;
                    acc.hasMetadata = true;
                }

                if (payloadLen > 0)
                    acc.data.append (payloadPtr, static_cast<size_t> (payloadLen));
            }
            else
            {
                // Final chunk: append remaining payload, decode, display
                if (not acc.hasMetadata)
                {
                    // Single-chunk path: metadata comes from this packet
                    acc.format      = p.format;
                    acc.pixelWidth  = p.pixelWidth;
                    acc.pixelHeight = p.pixelHeight;
                    acc.compressed  = p.compressed;
                    acc.quiet       = p.quiet;
                    acc.hasMetadata = true;
                }

                if (payloadLen > 0)
                    acc.data.append (payloadPtr, static_cast<size_t> (payloadLen));

                const size_t totalBase64 { acc.data.getSize() };

                if (totalBase64 > 0)
                {
                    jam::debug::Log::write ("KittyDecoder: finalizing id=" + juce::String (static_cast<int> (p.imageId))
                        + " accSize=" + juce::String (static_cast<int> (acc.data.getSize()))
                        + " accFormat=" + juce::String (acc.format)
                        + " accW=" + juce::String (acc.pixelWidth)
                        + " accH=" + juce::String (acc.pixelHeight));

                    DecodedImage image { decodePayload (static_cast<const uint8_t*> (acc.data.getData()),
                                                        static_cast<int> (totalBase64),
                                                        acc.format,
                                                        acc.pixelWidth,
                                                        acc.pixelHeight,
                                                        acc.compressed) };

                    if (image.isValid())
                    {
                        if (p.virtualPlacement == 1)
                        {
                            // a=T,U=1 — transmit + register virtual placement, no grid cells
                            StoredImage stored;
                            stored.width  = image.width;
                            stored.height = image.height;
                            stored.rgba   = std::move (image.rgba);
                            storedImages[p.imageId] = std::move (stored);

                            result.isVirtualPlacement = true;
                            result.placementCols      = p.placementCols;
                            result.placementRows      = p.placementRows;
                            result.kittyImageId       = p.imageId;
                            result.image.width        = stored.width;
                            result.image.height       = stored.height;

                            // Copy rgba for the pending image pipeline (storedImages owns original)
                            const auto& si { storedImages[p.imageId] };
                            const size_t totalBytes { static_cast<size_t> (si.width) * static_cast<size_t> (si.height) * 4u };
                            result.image.rgba.allocate (totalBytes, false);
                            std::memcpy (result.image.rgba.get(), si.rgba.get(), totalBytes);

                            result.response = buildKittyResponse (p.imageId, acc.quiet, false);
                        }
                        else
                        {
                            result.image        = std::move (image);
                            result.shouldDisplay= true;
                            result.kittyImageId = p.imageId;
                            result.response     = buildKittyResponse (p.imageId, acc.quiet, false);
                        }
                    }
                    else
                    {
                        result.response = buildKittyErrorResponse (p.imageId, acc.quiet);
                    }
                }

                chunks.erase (p.imageId);
            }
        }

        // -------------------------------------------------------------------------
        // a=p — display stored image (or register virtual placement when U=1)
        // -------------------------------------------------------------------------
        else if (p.action == 'p')
        {
            const auto it { storedImages.find (p.imageId) };

            if (it != storedImages.end())
            {
                const StoredImage& stored { it->second };
                const int w { stored.width };
                const int h { stored.height };

                if (w > 0 and h > 0 and stored.rgba.get() != nullptr)
                {
                    if (p.virtualPlacement == 1)
                    {
                        // a=p,U=1 — register virtual placement, supply image data for atlas staging
                        const size_t totalBytes { static_cast<size_t> (w) * static_cast<size_t> (h) * 4u };
                        result.image.rgba.allocate (totalBytes, false);
                        std::memcpy (result.image.rgba.get(), stored.rgba.get(), totalBytes);
                        result.image.width        = w;
                        result.image.height       = h;
                        result.isVirtualPlacement = true;
                        result.placementCols      = p.placementCols;
                        result.placementRows      = p.placementRows;
                        result.kittyImageId       = p.imageId;
                        result.response           = buildKittyResponse (p.imageId, p.quiet, false);
                    }
                    else
                    {
                        const size_t totalBytes { static_cast<size_t> (w) * static_cast<size_t> (h) * 4u };
                        result.image.rgba.allocate (totalBytes, false);
                        std::memcpy (result.image.rgba.get(), stored.rgba.get(), totalBytes);
                        result.image.width  = w;
                        result.image.height = h;
                        result.shouldDisplay= true;
                        result.kittyImageId = p.imageId;
                        result.response     = buildKittyResponse (p.imageId, p.quiet, false);
                    }
                }
                else
                {
                    result.response = buildKittyErrorResponse (p.imageId, p.quiet);
                }
            }
            else
            {
                result.response = buildKittyErrorResponse (p.imageId, p.quiet);
            }
        }
    }

    return result;
}

// ============================================================================
// KittyDecoder::clear
// ============================================================================

/**
 * @brief Clear all stored images and chunk accumulators.
 *
 * @note READER THREAD only.
 */
void KittyDecoder::clear() noexcept
{
    chunks.clear();
    storedImages.clear();
}

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Terminal
