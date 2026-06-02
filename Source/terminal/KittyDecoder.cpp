/**
 * @file KittyDecoder.cpp
 * @brief Kitty Graphics Protocol state machine: process(), clear(),
 *        accumulateChunk(), and finalizeChunk().
 *
 * Payload parsing (parseKittyParams), pixel decoding (decodePayload), and
 * response builders (buildKittyResponse, buildKittyErrorResponse) live in
 * KittyDecoderDecode.cpp.
 *
 * @see KittyDecoder.h
 * @see KittyDecoderDecode.cpp
 */

#include "KittyDecoder.h"

namespace terminal
{
/*____________________________________________________________________________*/

// ============================================================================
// KittyDecoder::accumulateChunk
// ============================================================================

void KittyDecoder::accumulateChunk (ChunkAccumulator& acc, const KittyParams& p,
                                    const uint8_t* payloadPtr, int payloadLen) noexcept
{
    if (not acc.hasMetadata)
    {
        acc.format      = p.format;
        acc.pixelWidth  = p.pixelWidth;
        acc.pixelHeight = p.pixelHeight;
        acc.compressed  = p.compressed;
        acc.quiet       = p.quiet;
        acc.hasMetadata = true;
    }
    else if (p.pixelWidth > 0 and p.pixelHeight > 0)
    {
        // New image over stale accumulator — reset data, update metadata.
        acc.data.reset();
        acc.format      = p.format;
        acc.pixelWidth  = p.pixelWidth;
        acc.pixelHeight = p.pixelHeight;
        acc.compressed  = p.compressed;
        acc.quiet       = p.quiet;
    }

    if (payloadLen > 0)
        acc.data.append (payloadPtr, static_cast<size_t> (payloadLen));
}

// ============================================================================
// KittyDecoder::finalizeChunk
// ============================================================================

DecodedImage KittyDecoder::finalizeChunk (ChunkAccumulator& acc, const KittyParams& p,
                                          const uint8_t* payloadPtr, int payloadLen,
                                          Result& result) noexcept
{
    DecodedImage image;

    if (not acc.hasMetadata)
    {
        // Single-chunk path: metadata comes from this packet.
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
        image = decodePayload (static_cast<const uint8_t*> (acc.data.getData()),
                               static_cast<int> (totalBase64),
                               acc.format,
                               acc.pixelWidth,
                               acc.pixelHeight,
                               acc.compressed);

        if (not image.isValid())
            result.response = buildKittyErrorResponse (p.imageId, acc.quiet);
    }

    chunks.erase (p.imageId);

    return image;
}

// ============================================================================
// KittyDecoder::process
// ============================================================================

KittyDecoder::Result KittyDecoder::process (const uint8_t* data, int length) noexcept
{
    Result result;

    if (length > 0)
    {
        const KittyParams p { parseKittyParams (data, length) };

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
                accumulateChunk (acc, p, payloadPtr, payloadLen);
            }
            else
            {
                // Final chunk: decode and store (finalizeChunk erases acc)
                const int quietSnapshot { acc.hasMetadata ? acc.quiet : p.quiet };
                DecodedImage image { finalizeChunk (acc, p, payloadPtr, payloadLen, result) };

                if (image.isValid())
                {
                    StoredImage stored;
                    stored.width  = image.width;
                    stored.height = image.height;
                    stored.rgba   = std::move (image.rgba);
                    storedImages[p.imageId] = std::move (stored);

                    result.kittyImageId = p.imageId;
                    result.response     = buildKittyResponse (p.imageId, quietSnapshot, false);
                }
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
                accumulateChunk (acc, p, payloadPtr, payloadLen);
            }
            else
            {
                // Final chunk: decode and display (finalizeChunk erases acc)
                const int quietSnapshot { acc.hasMetadata ? acc.quiet : p.quiet };
                DecodedImage image { finalizeChunk (acc, p, payloadPtr, payloadLen, result) };

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

                        result.response = buildKittyResponse (p.imageId, quietSnapshot, false);
                    }
                    else
                    {
                        result.image         = std::move (image);
                        result.shouldDisplay = true;
                        result.kittyImageId  = p.imageId;
                        result.placementCols = p.placementCols;
                        result.placementRows = p.placementRows;
                        result.response      = buildKittyResponse (p.imageId, quietSnapshot, false);
                    }
                }
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
                        result.image.width   = w;
                        result.image.height  = h;
                        result.shouldDisplay = true;
                        result.kittyImageId  = p.imageId;
                        result.response      = buildKittyResponse (p.imageId, p.quiet, false);
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
} // namespace terminal
