/**
 * @file ITerm2Decoder.cpp
 * @brief Implementation of Terminal::ITerm2Decoder.
 *
 * @see ITerm2Decoder.h
 */

#include "ITerm2Decoder.h"
#include "ImageDecode.h"

namespace Terminal
{ /*____________________________________________________________________________*/

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
                        // 6+7. Convert to ARGB and swizzle JUCE BGRA -> straight RGBA8
                        // -------------------------------------------------------------------------
                        juce::Image argbImage { image.convertedToFormat (juce::Image::ARGB) };
                        swizzleARGBToRGBA (argbImage, result.rgba, result.width, result.height);
                    }
                }
            }
        }
    }

    return result;
}

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Terminal
