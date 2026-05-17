/**
 * @file VideoDCS.cpp
 * @brief DCS (Device Control String) and APC (Application Program Command) passthrough.
 *
 * This translation unit implements `Video::storeDCSHeader()`, `Video::applyDCSPayload()`,
 * `Video::applyAPCPayload()`, and `Video::advanceCursorForImage()`.
 *
 * ESC-level dispatch lives in `VideoESC.cpp`; OSC dispatch lives in `VideoOSC.cpp`.
 *
 * @par DCS passthrough
 * DCS (Device Control String) sequences are accepted by the VT command processor and
 * accumulated in `dcsBuffer` via `appendToBuffer()`.  `dcsHook()` is the entry
 * point and `dcsUnhook()` is the exit point.  `Video::applyDCSPayload()` stores
 * the DCS final byte (via `storeDCSHeader()`) and fires `ID::dcsPayloadComplete`
 * through the events map.  The Processor handler delegates to `Skit::processDCS()`.
 *
 * @par APC passthrough
 * APC (Application Program Command) sequences are accumulated in `apcBuffer`.
 * `Video::applyAPCPayload()` fires `ID::apcPayloadComplete` through the events map.
 * The Processor handler delegates to `Skit::processAPC()`.
 *
 * @par Image pipeline
 * 1. Video stores DCS final byte in `storeDCSHeader()`.
 * 2. Parser calls `video.applyDCSPayload()` — fires `ID::dcsPayloadComplete`.
 * 3. Processor event handler calls `skit.processDCS(video.getDcsFinalByte(), ...)`.
 * 4. Processor event handler calls `video.advanceCursorForImage(skit.getLastImageRows())`.
 *
 * @par Thread model
 * All functions in this file run exclusively on the **READER THREAD**.
 *
 * @see Video.h      — class declaration and full API documentation
 * @see Skit.h       — image decode and SKiT filepath handler
 * @see VideoESC.cpp — ESC sequence dispatch
 * @see VideoOSC.cpp — OSC sequence dispatch
 */

#include "Video.h"
#include <jam_tui/jam_tui.h>

namespace Terminal
{ /*____________________________________________________________________________*/

// ============================================================================
// DCS hook / unhook
// ============================================================================

/**
 * @brief Stores the DCS sequence header when a DCS sequence is hooked.
 *
 * Invoked on entry to the `dcsPassthrough` state when the DCS final byte is
 * received.  Records `finalByte` in `dcsFinalByte` for Processor to read via
 * `getDcsFinalByte()` after `applyDCSPayload()` returns.
 *
 * @par Sequence format
 * @code
 *   ESC P <params> <intermediates> <final> <data> ESC \
 * @endcode
 *
 * @param params             Finalised DCS parameter accumulator (unused).
 * @param inter              Pointer to the intermediate byte buffer (unused).
 * @param interCount         Number of valid bytes in `inter` (unused).
 * @param finalByte          The DCS final byte, stored in `dcsFinalByte`.
 *
 * @note READER THREAD only.
 *
 * @see getDcsFinalByte()
 * @see applyDCSPayload()
 */
void Video::storeDCSHeader (const CSI& /*params*/,
                             const uint8_t* /*inter*/,
                             uint8_t /*interCount*/,
                             uint8_t finalByte) noexcept
{
    dcsFinalByte = finalByte;
}

/**
 * @brief Called when a DCS sequence is terminated (ST received).
 *
 * Fires `ID::dcsPayloadComplete` through the events map, delivering the raw
 * payload to the Processor handler.  The handler reads `getDcsFinalByte()`
 * and delegates to `Skit::processDCS()`, then calls `advanceCursorForImage()`.
 *
 * @note READER THREAD only.
 *
 * @see storeDCSHeader()
 * @see getDcsFinalByte()
 * @see Skit::processDCS()
 * @see advanceCursorForImage()
 */
void Video::applyDCSPayload (const uint8_t* data, int length) noexcept
{
    if (events.contains (ID::dcsPayloadComplete))
        events.get (ID::dcsPayloadComplete, data, length);
}

// ============================================================================
// APC end
// ============================================================================

/**
 * @brief Called when an APC sequence is terminated (BEL or ST received).
 *
 * Fires `ID::apcPayloadComplete` through the events map, delivering the raw
 * payload to the Processor handler.  The handler delegates to
 * `Skit::processAPC()`, forwards any Kitty response via `writeToHost`, then
 * calls `advanceCursorForImage()`.
 *
 * @note READER THREAD only.
 *
 * @see Skit::processAPC()
 * @see advanceCursorForImage()
 */
void Video::applyAPCPayload (const uint8_t* data, int length) noexcept
{
    if (events.contains (ID::apcPayloadComplete))
        events.get (ID::apcPayloadComplete, data, length);
}

// ============================================================================
// Post-decode cursor advance
// ============================================================================

/**
 * @brief Advances cursor after image placement — moves down by `numRows`, resets to column 0.
 *
 * Called by Processor after `Skit::processDCS()`, `Skit::processAPC()`, or
 * `Skit::processOSC1337()` to apply the post-decode cursor position update.
 * No-op when `numRows <= 0`.
 *
 * @param numRows  Number of cell rows to advance downward.
 * @note READER THREAD only.
 */
void Video::advanceCursorForImage (int numRows) noexcept
{
    if (numRows > 0)
    {
        cursorMoveDown (numRows, effectiveClampBottom());
        cursorCol = 0_cell;
    }
}

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Terminal
