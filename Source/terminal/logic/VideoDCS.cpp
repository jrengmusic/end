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
 * point and `dcsUnhook()` is the exit point.  `Video::applyDCSPayload()` records
 * the DCS final byte for `Processor` to read via `getDcsFinalByte()`, then
 * delegates all image decode and SKiT filepath handling to `Skit::processDCS()`.
 *
 * @par APC passthrough
 * APC (Application Program Command) sequences are accumulated in `apcBuffer`.
 * `Video::applyAPCPayload()` is a thin stub; all Kitty graphics decode and SKiT
 * filepath handling is delegated to `Skit::processAPC()` by Processor.
 *
 * @par Image pipeline
 * 1. Video stores DCS final byte in `storeDCSHeader()`.
 * 2. Processor calls `video.applyDCSPayload()` (no-op for image logic).
 * 3. Processor calls `skit.processDCS(video.getDcsFinalByte(), ...)`.
 * 4. Processor calls `video.advanceCursorForImage(skit.getLastImageRows())`.
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
 * Stub — image decode and SKiT filepath handling have been moved to
 * `Skit::processDCS()`.  Processor reads `getDcsFinalByte()` after this
 * method returns and forwards the payload to Skit, then calls
 * `advanceCursorForImage()` with the result.
 *
 * @note READER THREAD only.
 *
 * @see storeDCSHeader()
 * @see getDcsFinalByte()
 * @see Skit::processDCS()
 * @see advanceCursorForImage()
 */
void Video::applyDCSPayload (const uint8_t* /*data*/, int /*length*/) noexcept
{
    // Image decode delegated to Skit::processDCS() by Processor.
}

// ============================================================================
// APC end
// ============================================================================

/**
 * @brief Called when an APC sequence is terminated (BEL or ST received).
 *
 * Stub — image decode and SKiT filepath handling have been moved to
 * `Skit::processAPC()`.  Processor forwards the payload to Skit after this
 * method returns, then calls `advanceCursorForImage()` with the result and
 * forwards any Kitty response via the `"writeToHost"` event.
 *
 * @note READER THREAD only.
 *
 * @see Skit::processAPC()
 * @see advanceCursorForImage()
 */
void Video::applyAPCPayload (const uint8_t* /*data*/, int /*length*/) noexcept
{
    // Image decode delegated to Skit::processAPC() by Processor.
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
        const ActiveScreen scr { activeScreen };
        cursorMoveDown (scr, numRows, effectiveClampBottom (scr));
        cursorCol[static_cast<int> (scr)] = 0;
    }
}

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Terminal
