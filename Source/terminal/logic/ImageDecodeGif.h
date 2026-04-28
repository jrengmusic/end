/**
 * @file ImageDecodeGif.h
 * @brief File-scope GIF metadata parser shared by all platform ImageDecode translation units.
 *
 * Included directly by ImageDecode.cpp, ImageDecodeMac.mm, and ImageDecodeWin.cpp.
 * All definitions are static — each including TU gets its own copy.  This is
 * intentional: the parser carries no mutable state and the structs are small.
 *
 * @see ImageDecode.h
 */

#pragma once

#include <cstdint>
#include <vector>

//==============================================================================
// GIF metadata types
//==============================================================================

/** @brief Per-frame positional and timing metadata extracted from a GIF stream. */
struct GifFrameInfo
{
    int left     { 0 };
    int top      { 0 };
    int width    { 0 };
    int height   { 0 };
    int delayMs  { 0 };
    int disposal { 0 };  ///< 0=unspec, 1=none, 2=restore-bg, 3=restore-previous
};

/** @brief Canvas dimensions and per-frame metadata for an entire GIF stream. */
struct GifMetadata
{
    int canvasWidth  { 0 };
    int canvasHeight { 0 };
    std::vector<GifFrameInfo> frames;
    bool isValid { false };
};

//==============================================================================
// GIF parser helpers
//==============================================================================

static int readLE16 (const uint8_t* p) noexcept { return p[0] | (p[1] << 8); }

static size_t skipSubBlocks (const uint8_t* data, size_t pos, size_t size) noexcept
{
    while (pos < size)
    {
        const int blockLen { data[pos] };
        ++pos;
        if (blockLen == 0) break;
        pos += static_cast<size_t> (blockLen);
    }
    return pos;
}

/**
 * @brief Parses GIF binary stream metadata — canvas dimensions, per-frame
 *        positions, disposal methods, and delays.
 *
 * @param data  Pointer to the raw GIF bytes.
 * @param size  Number of bytes in `data`.
 * @return A `GifMetadata` with `isValid = true` and at least one frame on
 *         success; an empty/invalid struct on failure.
 */
static GifMetadata parseGifMetadata (const uint8_t* data, size_t size) noexcept
{
    GifMetadata meta;

    // Validate GIF header ("GIF87a" or "GIF89a")
    if (size < 13
        or data[0] != 'G' or data[1] != 'I' or data[2] != 'F'
        or data[3] != '8'
        or (data[4] != '7' and data[4] != '9')
        or data[5] != 'a')
    {
        return meta;
    }

    meta.canvasWidth  = readLE16 (data + 6);
    meta.canvasHeight = readLE16 (data + 8);

    if (meta.canvasWidth <= 0 or meta.canvasHeight <= 0)
        return meta;

    // Logical Screen Descriptor packed byte — bit 7 = global colour table flag
    const uint8_t lsdPacked { data[10] };
    size_t pos { 13u };

    if (lsdPacked & 0x80u)
    {
        // Global Colour Table present — skip it
        const size_t gctSize { 3u * (2u << (lsdPacked & 0x07u)) };
        pos += gctSize;
    }

    // Pending GCE data carried forward until the next Image Descriptor
    int pendingDisposal { 0 };
    int pendingDelayMs  { 0 };
    bool hasPendingGce  { false };

    while (pos < size)
    {
        const uint8_t blockId { data[pos] };
        ++pos;

        if (blockId == 0x3Bu)
        {
            // Trailer — end of GIF
            break;
        }
        else if (blockId == 0x2Cu)
        {
            // Image Descriptor
            if (pos + 9u > size) break;

            GifFrameInfo fi;
            fi.left   = readLE16 (data + pos);
            fi.top    = readLE16 (data + pos + 2u);
            fi.width  = readLE16 (data + pos + 4u);
            fi.height = readLE16 (data + pos + 6u);

            if (hasPendingGce)
            {
                fi.disposal = pendingDisposal;
                fi.delayMs  = pendingDelayMs;
                hasPendingGce = false;
            }

            const uint8_t idPacked { data[pos + 8u] };
            pos += 9u;

            if (idPacked & 0x80u)
            {
                // Local Colour Table present — skip it
                const size_t lctSize { 3u * (2u << (idPacked & 0x07u)) };
                pos += lctSize;
            }

            // Skip LZW minimum code size byte
            if (pos < size) ++pos;

            // Skip LZW sub-blocks
            pos = skipSubBlocks (data, pos, size);

            meta.frames.push_back (fi);
        }
        else if (blockId == 0x21u)
        {
            // Extension
            if (pos >= size) break;
            const uint8_t label { data[pos] };
            ++pos;

            if (label == 0xF9u)
            {
                // Graphics Control Extension:
                // block size (4), packed byte, delay LE16 (centiseconds),
                // transparent colour index, block terminator (0)
                if (pos + 6u > size) break;

                // block size byte
                ++pos;

                const uint8_t gcePacked { data[pos] };
                ++pos;

                const int delayCentiseconds { readLE16 (data + pos) };
                pos += 2u;

                // transparent colour index byte + terminator byte
                pos += 2u;

                pendingDisposal = (gcePacked >> 2) & 0x07;
                // Minimum 10 ms per frame (matches browser behaviour)
                const int rawDelayMs { delayCentiseconds * 10 };
                pendingDelayMs = rawDelayMs < 10 ? 10 : rawDelayMs;
                hasPendingGce  = true;
            }
            else
            {
                // Other extension — skip sub-blocks
                pos = skipSubBlocks (data, pos, size);
            }
        }
        else
        {
            // Unknown block — skip sub-blocks to stay in sync
            pos = skipSubBlocks (data, pos, size);
        }
    }

    meta.isValid = not meta.frames.empty();
    return meta;
}
