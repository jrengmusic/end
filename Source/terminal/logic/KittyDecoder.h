/**
 * @file KittyDecoder.h
 * @brief Kitty Graphics Protocol APC payload decoder.
 *
 * `Terminal::KittyDecoder` parses an APC G payload (everything after 'G' in the
 * `ESC _ G [key=value,...] ; [base64 payload] ESC \` sequence) and produces an
 * RGBA8 pixel buffer in the same `DecodedImage` format used by `SixelDecoder`
 * and `ITerm2Decoder`.
 *
 * @par Protocol summary
 * @code
 *   ESC _ G key=value,...;base64payload ESC \
 * @endcode
 * Key=value pairs (comma-separated, before `;`):
 * - `a` — action: t=transmit only, T=transmit+display (DEFAULT), p=display by id,
 *          d=delete, q=query
 * - `f` — format: 24=RGB, 32=RGBA (DEFAULT), 100=PNG
 * - `t` — transmission medium: d=direct base64 (DEFAULT — only supported mode)
 * - `s` — pixel width (for f=24/32)
 * - `v` — pixel height (for f=24/32)
 * - `i` — image ID (for a=p/a=d)
 * - `m` — more data: 1=more chunks coming, 0=final chunk (DEFAULT)
 * - `o` — compression: z=zlib compressed (applied before base64)
 * - `q` — quiet: 1=suppress OK response, 2=suppress all responses
 *
 * @see Terminal::DecodedImage
 * @see Terminal::SixelDecoder
 * @see Terminal::ITerm2Decoder
 * @see Terminal::State::addImageNode()
 */

#pragma once
#include <JuceHeader.h>
#include "SixelDecoder.h"

namespace Terminal
{ /*____________________________________________________________________________*/

/**
 * @class KittyDecoder
 * @brief Decodes APC G payloads (Kitty Graphics Protocol) into RGBA8 pixel buffers.
 *
 * Handles the core Kitty graphics actions: transmit-only (`a=t`),
 * transmit-and-display (`a=T`), display-stored (`a=p`), delete (`a=d`),
 * and query (`a=q`).  Chunked multi-packet transmission is supported via
 * the `m=1` / `m=0` protocol.  Direct base64 transmission only (`t=d`).
 *
 * @note Constructed and used on the READER THREAD only.
 */
class KittyDecoder
{
public:
    KittyDecoder() = default;

    /**
     * @brief Result of processing one APC G payload.
     *
     * Only one of `shouldDisplay`, `isQuery`, or `isDelete` will be true per
     * successful result.  For chunked transmissions, all flags remain false
     * until the final chunk (`m=0`) is processed.
     */
    struct Result
    {
        DecodedImage image;///< Valid only on final transmit+display.
        bool shouldDisplay { false };///< True if image should be placed in grid.
        bool isQuery { false };///< True if this was a query (a=q).
        bool isDelete { false };///< True if this was a delete (a=d).
        bool isVirtualPlacement { false };///< True if this is a virtual placement (U=1, a=p or a=T).
        int placementCols { 0 };///< Virtual placement column count (c= param).
        int placementRows { 0 };///< Virtual placement row count (r= param).
        uint32_t kittyImageId { 0 };///< Kitty-assigned image ID (from i= param).
        juce::String response;///< Response to write back to PTY (OK or error).
    };

    /**
     * @brief Process a complete APC G payload.
     *
     * Parses the command, handles chunked transmission, and decodes the image
     * on the final chunk.
     *
     * @param data    Raw APC payload bytes (everything after 'G').
     * @param length  Number of bytes.
     * @return Result with `shouldDisplay=true` on final transmit+display.
     *         Returns a default-constructed Result otherwise (query, delete,
     *         or mid-chunk accumulation).
     *
     * @note READER THREAD only.
     */
    Result process (const uint8_t* data, int length) noexcept;

    /**
     * @brief Clear all stored images and chunk accumulators.
     *
     * @note READER THREAD only.
     */
    void clear() noexcept;

private:
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
     * @struct ChunkAccumulator
     * @brief Intermediate storage for chunked image transmissions.
     *
     * Keyed by image ID (0 for anonymous).  Metadata is set from the first
     * chunk's key=value params and held until the final chunk arrives.
     */
    struct ChunkAccumulator
    {
        juce::MemoryBlock data;
        int format { 32 };///< f= value
        int pixelWidth { 0 };///< s= value
        int pixelHeight { 0 };///< v= value
        bool compressed { false };///< o=z
        bool hasMetadata { false };///< True once metadata fields have been stored from the first APC
        int quiet { 0 };///< q= captured from the first chunk
    };

    /**
     * @struct StoredImage
     * @brief RGBA8 buffer held after a=t (transmit-only) for later a=p display.
     */
    struct StoredImage
    {
        juce::HeapBlock<uint8_t> rgba;
        int width { 0 };
        int height { 0 };
    };

    /** @brief Chunk accumulator keyed by image ID (0 = anonymous). */
    std::unordered_map<uint32_t, ChunkAccumulator> chunks;

    /** @brief Stored images keyed by image ID for deferred a=p display. */
    std::unordered_map<uint32_t, StoredImage> storedImages;

    /**
     * @brief Parses the key=value region of a Kitty APC payload.
     *
     * Scans from byte 0 looking for comma-separated `key=value` pairs.  Stops
     * when a `;` delimiter is found (start of base64 payload) or when `length`
     * is exhausted.  Single-character keys only; value is every character up to
     * the next `,` or `;`.
     *
     * @param data    Pointer to APC payload bytes (everything after 'G').
     * @param length  Number of bytes.
     * @return Populated KittyParams.  `payloadStart` is set to the index after
     *         the `;` delimiter, or `length` if no `;` was found.
     *
     * @note Pure function — no side effects.
     * @note READER THREAD only.
     */
    static KittyParams parseKittyParams (const uint8_t* data, int length) noexcept;

    /**
     * @brief Decode a Kitty image payload to RGBA8.
     *
     * Performs base64 decode, optional zlib decompression, then pixel
     * extraction per the given format.
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
    DecodedImage decodePayload (const uint8_t* base64Data,
                                int base64Length,
                                int format,
                                int pixelWidth,
                                int pixelHeight,
                                bool compressed) noexcept;

    /**
     * @brief Accumulate a mid-sequence chunk (`m=1`) into @p acc.
     *
     * On the first chunk for this image ID: records metadata from @p p into
     * @p acc.  On subsequent chunks where new dimensions are provided: treats
     * the accumulator as stale, resets its data, and records the fresh
     * metadata.  Appends @p payloadPtr / @p payloadLen to `acc.data` in both
     * cases.
     *
     * Called by both the `a=t` and `a=T` m=1 paths.
     *
     * @param acc        Chunk accumulator for this image ID.
     * @param p          Parsed key=value params from the current APC packet.
     * @param payloadPtr Pointer to the base64 payload bytes.
     * @param payloadLen Number of payload bytes.
     *
     * @note READER THREAD only.
     */
    void accumulateChunk (ChunkAccumulator& acc, const KittyParams& p,
                          const uint8_t* payloadPtr, int payloadLen) noexcept;

    /**
     * @brief Finalize a chunked sequence (`m=0`): append, decode, and clean up.
     *
     * Applies a metadata fallback for the single-chunk path (when `acc` has no
     * metadata yet), appends any remaining payload bytes, then calls
     * `decodePayload()`.  On decode failure writes an error response into
     * @p result.  Erases the accumulator from `chunks` regardless of outcome.
     *
     * Called by both the `a=t` and `a=T` m=0 paths.  The caller is responsible
     * for storing or displaying the returned `DecodedImage`.
     *
     * @param acc        Chunk accumulator for this image ID.
     * @param p          Parsed key=value params from the current APC packet.
     * @param payloadPtr Pointer to the final base64 payload bytes.
     * @param payloadLen Number of payload bytes.
     * @param result     Result being built; receives the error response on failure.
     * @return Decoded image on success; invalid DecodedImage on failure.
     *
     * @note READER THREAD only.
     */
    DecodedImage finalizeChunk (ChunkAccumulator& acc, const KittyParams& p,
                                const uint8_t* payloadPtr, int payloadLen,
                                Result& result) noexcept;

    /**
     * @brief Builds a Kitty OK response string for the given image ID and quiet level.
     *
     * Returns an empty string when `quiet >= 2` (suppress all), or when
     * `quiet == 1` and `suppressOk` is true.  Otherwise returns the APC-framed
     * OK response.
     *
     * @param imageId    Kitty image ID to embed in the response.
     * @param quiet      Quiet level from the `q=` parameter.
     * @param suppressOk True to suppress OK-level responses (quiet=1 case).
     * @return APC-framed response string, or empty string if suppressed.
     *
     * @note Pure function — no side effects.
     */
    static juce::String buildKittyResponse (uint32_t imageId, int quiet,
                                            bool suppressOk) noexcept;

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
    static juce::String buildKittyErrorResponse (uint32_t imageId, int quiet) noexcept;
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Terminal
