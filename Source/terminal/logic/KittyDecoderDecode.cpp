/**
 * @file KittyDecoderDecode.cpp
 * @brief Kitty graphics payload decoding and parameter parsing.
 *
 * @see KittyDecoder.h
 * @see KittyDecoder.cpp
 */

#include "KittyDecoder.h"
#include "ImageDecode.h"

namespace Terminal
{ /*____________________________________________________________________________*/

// ============================================================================
// KittyDecoder::parseKittyParams
// ============================================================================

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
KittyDecoder::KittyParams KittyDecoder::parseKittyParams (const uint8_t* data, int length) noexcept
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

// ============================================================================
// KittyDecoder::decodePayload
// ============================================================================

/**
 * @brief Decode a Kitty image payload to RGBA8 pixels.
 *
 * Steps:
 *  1. Base64-decode the raw bytes via `juce::Base64::convertFromBase64()`.
 *  2. If compressed (o=z): decompress with `GZIPDecompressorInputStream` (zlib).
 *  3. Auto-detect PNG/JPEG: if format is f=32 or f=24 AND dimensions are unknown
 *     (pixelWidth==0 or pixelHeight==0), inspect the magic bytes and override
 *     format to 100 so JUCE handles the container decode.
 *  4. Dispatch on format:
 *     - f=100 (PNG/JPEG/auto): JUCE image load → ARGB convert → swizzle + un-premultiply via swizzleARGBToRGBA.
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
                // PNG/JPEG/auto: JUCE image load → ARGB → swizzle + un-premultiply → straight RGBA8
                juce::Image image { juce::ImageFileFormat::loadFrom (pixelData, pixelBytes) };

                // Platform-native fallback for formats JUCE doesn't handle (TIFF, BMP, WebP, etc.)
                if (not image.isValid())
                    image = loadImageNative (pixelData, pixelBytes);

                if (image.isValid())
                {
                    const juce::Image argbImage { image.convertedToFormat (juce::Image::ARGB) };
                    int w { 0 };
                    int h { 0 };

                    if (swizzleARGBToRGBA (argbImage, result.rgba, w, h))
                    {
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
// KittyDecoder::buildKittyResponse
// ============================================================================

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
juce::String KittyDecoder::buildKittyResponse (uint32_t imageId, int quiet, bool suppressOk) noexcept
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

// ============================================================================
// KittyDecoder::buildKittyErrorResponse
// ============================================================================

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
juce::String KittyDecoder::buildKittyErrorResponse (uint32_t imageId, int quiet) noexcept
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

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Terminal
