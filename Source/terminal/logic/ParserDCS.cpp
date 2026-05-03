/**
 * @file ParserDCS.cpp
 * @brief DCS (Device Control String) and APC (Application Program Command) passthrough.
 *
 * This translation unit implements `Parser::dcsHook()`, `Parser::dcsUnhook()`,
 * `Parser::dcsPut()`, `Parser::setPhysCellDimensions()`, and `Parser::apcEnd()`.
 *
 * ESC-level dispatch lives in `ParserESC.cpp`; OSC dispatch lives in `ParserOSC.cpp`.
 *
 * @par DCS passthrough
 * DCS (Device Control String) sequences are accepted by the state machine and
 * accumulated in `dcsBuffer` via `appendToBuffer()`.  `dcsHook()` is the entry
 * point and `dcsUnhook()` is the exit point.  When `dcsFinalByte == 'q'`,
 * `dcsUnhook()` runs `SixelDecoder` on the accumulated payload and invokes
 * the `onImageDecoded` callback with grid position metadata.
 *
 * @par APC passthrough
 * APC (Application Program Command) sequences are accumulated in `apcBuffer`.
 * `apcEnd()` processes the accumulated payload through `KittyDecoder` for
 * Kitty graphics protocol support.
 *
 * @par Image pipeline (all three protocols share the same commit path)
 * 1. Compute grid position from cursor and scrollback.
 * 2. Invoke `onImageDecoded` callback — MESSAGE THREAD → `State::addImageNode()` → ValueTree.
 *
 * @par Thread model
 * All functions in this file run exclusively on the **READER THREAD**, except
 * `setPhysCellDimensions()` which is called on the **MESSAGE THREAD**.
 *
 * @see Parser.h      — class declaration and full API documentation
 * @see ParserESC.cpp — ESC sequence dispatch
 * @see ParserOSC.cpp — OSC sequence dispatch
 * @see SixelDecoder  — Sixel graphics decoder
 * @see KittyDecoder  — Kitty graphics protocol decoder
 * @see Parser::onImageDecoded — callback invoked after decode
 */

#include "Parser.h"
#include <jam_tui/jam_tui.h>
#include "ImageDecode.h"
#include "KittyDecoder.h"
#include "SixelDecoder.h"

namespace Terminal
{ /*____________________________________________________________________________*/

// ============================================================================
// DCS hook / unhook
// ============================================================================

/**
 * @brief Called when a DCS (Device Control String) sequence is hooked.
 *
 * Invoked on entry to the `dcsPassthrough` state when the DCS final byte is
 * received.  The `dcsFinalByte` member is recorded by `performAction()` prior
 * to this call.  The current implementation is a stub — Sixel decoder dispatch
 * will be wired here in a future step.
 *
 * @par Sequence format
 * @code
 *   ESC P <params> <intermediates> <final> <data> ESC \
 * @endcode
 *
 * @param params             Finalised DCS parameter accumulator (unused until decoder wired).
 * @param inter              Pointer to the intermediate byte buffer (unused until decoder wired).
 * @param interCount         Number of valid bytes in `inter` (unused until decoder wired).
 * @param finalByte          The DCS final byte (recorded in `dcsFinalByte` by caller).
 *
 * @note READER THREAD only.
 *
 * @see dcsUnhook()
 * @see appendToBuffer()
 */
void Parser::dcsHook (const CSI& /*params*/,
                      const uint8_t* /*inter*/,
                      uint8_t /*interCount*/,
                      uint8_t /*finalByte*/) noexcept
{
}

/**
 * @brief Called for each byte in the DCS passthrough data stream.
 *
 * Retained as an extension point.  DCS passthrough accumulation is now
 * performed directly in `performAction()` via `appendToBuffer (dcsBuffer, …)`.
 * This method is no longer called by the `put` action.
 *
 * @param byte  The DCS passthrough byte (unused).
 *
 * @note READER THREAD only.
 *
 * @see dcsHook()
 * @see dcsUnhook()
 */
void Parser::dcsPut (uint8_t /*byte*/) noexcept {}

/**
 * @brief Updates the physical cell dimensions used by the Sixel decoder.
 *
 * @param widthPx   Physical cell width in pixels.
 * @param heightPx  Physical cell height in pixels.
 * @note MESSAGE THREAD.
 */
void Parser::setPhysCellDimensions (int widthPx, int heightPx) noexcept
{
    physCellWidthAtomic.store  (widthPx,  std::memory_order_relaxed);
    physCellHeightAtomic.store (heightPx, std::memory_order_relaxed);
    metrics = jam::tui::Metrics { widthPx, heightPx, 1.0f };
}

/**
 * @brief Handles an END; filepath signal from any SKiT protocol envelope.
 *
 * Shared implementation for `dcsUnhook()`, `apcEnd()`, and `handleOsc1337()`
 * when the accumulated buffer begins with an `END;` prefix.  Reads the current
 * cursor row, cursor column, and scrollback depth, then invokes `onPreviewFile`
 * with the filepath, absolute grid row, grid column, and optional protocol bounds.
 * Does NOT load or decode the file — the receiver must perform file I/O on the
 * MESSAGE THREAD.  An empty `filepath` signals preview dismissal.
 *
 * @param filepath  Absolute path extracted after the END; marker.  Empty = dismiss.
 * @param cols      Protocol-specified overlay width in cells; 0 = use config.
 * @param lines     Protocol-specified overlay height in cells; 0 = use config.
 *
 * @note READER THREAD only.
 *
 * @see dcsUnhook()
 * @see apcEnd()
 * @see Parser::handleOsc1337
 * @see Parser::onPreviewFile
 */
void Parser::handleSkitFilepath (const juce::String& filepath, int cols, int lines) noexcept
{
    if (onPreviewFile)
    {
        const ActiveScreen scr   { state.getRawValue<ActiveScreen> (ID::activeScreen) };
        const int cursorRow      { state.getRawValue<int> (state.screenKey (scr, ID::cursorRow)) };
        const int cursorCol      { state.getRawValue<int> (state.screenKey (scr, ID::cursorCol)) };
        const int scrollbackUsed { state.getRawValue<int> (ID::scrollbackUsed) };
        const int absRow         { scrollbackUsed + cursorRow };

        onPreviewFile (filepath, absRow, cursorCol, cols, lines);
    }
}

/**
 * @brief Called when a DCS sequence is terminated (ST received).
 *
 * When `dcsFinalByte == 'q'` and the buffer contains data, checks for the
 * SKiT `END;` prefix first.  If present, delegates to `handleSkitFilepath()`
 * with the extracted path.  An empty filepath sends a zero-dims sentinel that
 * dismisses the preview.  Otherwise runs `SixelDecoder` on the accumulated
 * payload.  On success, computes the absolute grid position from the current
 * cursor and scrollback depth, then invokes `onImageDecoded` with the pixel
 * data and placement metadata.  Cursor is advanced by `cellRows` downward
 * and reset to column 0 after the callback.
 *
 * Silently skips the decode path when:
 * - `dcsFinalByte != 'q'` (not a Sixel sequence)
 * - `physCellWidthAtomic` or `physCellHeightAtomic` is zero (Screen not yet
 *   calibrated — first frame startup edge case)
 * - Decoder returns an invalid image
 *
 * @note READER THREAD only.
 *
 * @see dcsHook()
 * @see appendToBuffer()
 * @see SixelDecoder
 * @see Parser::onImageDecoded
 */
void Parser::dcsUnhook() noexcept
{
    if (dcsFinalByte == 'q' and dcsBufferSize > 0)
    {
        constexpr int endPrefixLen { 4 };

        if (dcsBufferSize >= endPrefixLen
            and dcsBuffer[0] == 'E' and dcsBuffer[1] == 'N'
            and dcsBuffer[2] == 'D' and dcsBuffer[3] == ';')
        {
            const juce::String payload { juce::String::fromUTF8 (
                reinterpret_cast<const char*> (dcsBuffer.get() + endPrefixLen),
                dcsBufferSize - endPrefixLen) };

            const int firstSep  { payload.indexOfChar (';') };
            const int secondSep { firstSep >= 0 ? payload.indexOfChar (firstSep + 1, ';') : -1 };

            int previewCols  { 0 };
            int previewLines { 0 };

            juce::String filepath;

            if (firstSep >= 0 and secondSep > firstSep)
            {
                filepath     = payload.substring (0, firstSep);
                previewCols  = payload.substring (firstSep + 1, secondSep).getIntValue();
                previewLines = payload.substring (secondSep + 1).getIntValue();
            }
            else
            {
                filepath = payload;
            }

            handleSkitFilepath (filepath, previewCols, previewLines);
        }
        else
        {
            const int cellW { physCellWidthAtomic.load  (std::memory_order_relaxed) };
            const int cellH { physCellHeightAtomic.load (std::memory_order_relaxed) };

            if (cellW > 0 and cellH > 0)
            {
                SixelDecoder decoder;
                DecodedImage image { decoder.decode (dcsBuffer.get(), static_cast<size_t> (dcsBufferSize)) };

                if (image.isValid())
                {
                    const ActiveScreen scr          { state.getRawValue<ActiveScreen> (ID::activeScreen) };
                    const int cursorRow             { state.getRawValue<int> (state.screenKey (scr, ID::cursorRow)) };
                    const int cursorCol             { state.getRawValue<int> (state.screenKey (scr, ID::cursorCol)) };
                    const int scrollbackUsed        { state.getRawValue<int> (ID::scrollbackUsed) };
                    const int absRow                { scrollbackUsed + cursorRow };

                    const auto span { metrics.cellSpan (image.width, image.height) };
                    const int cellCols { span.getWidth().value };
                    const int cellRows { span.getHeight().value };

                    if (onImageDecoded)
                    {
                        onImageDecoded (std::move (image.rgba),
                                        juce::HeapBlock<int>(),
                                        1,
                                        image.width, image.height,
                                        absRow, cursorCol,
                                        cellCols, cellRows,
                                        false);
                    }

                    cursorMoveDown (scr, cellRows, effectiveClampBottom (scr));
                    state.setCursorCol (scr, 0);
                }
            }
        }
    }

    dcsBufferSize = 0;
}

// ============================================================================
// APC end
// ============================================================================

/**
 * @brief Called when an APC sequence is terminated (BEL or ST received).
 *
 * Processes accumulated APC bytes through the Kitty graphics decoder.
 * If the decoder produces a displayable image (`shouldDisplay` is true on the
 * final chunk, `m=0`), computes the absolute grid position from the current
 * cursor and scrollback depth, then invokes `onImageDecoded` with the pixel
 * data and placement metadata.  Kitty's `placementRows` field is used for
 * cell height when provided; otherwise height is derived from pixel dimensions.
 *
 * Kitty responses (capability query, OK/ERROR acks) are queued via
 * `sendResponse()` and flushed by the caller after `process()` returns.
 *
 * @note READER THREAD only.
 *
 * @see dcsUnhook()      — identical pipeline for Sixel
 * @see handleOsc1337()  — identical pipeline for iTerm2
 * @see KittyDecoder     — chunked payload accumulation and PNG/raw decode
 * @see sendResponse()   — response queuing mechanism
 * @see Parser::onImageDecoded — callback invoked after decode
 */
void Parser::apcEnd() noexcept
{
    if (apcBufferSize > 0)
    {
        constexpr int endPrefixLen { 5 };

        if (apcBufferSize >= endPrefixLen
            and apcBuffer[0] == 'G' and apcBuffer[1] == 'E'
            and apcBuffer[2] == 'N' and apcBuffer[3] == 'D'
            and apcBuffer[4] == ';')
        {
            const juce::String payload { juce::String::fromUTF8 (
                reinterpret_cast<const char*> (apcBuffer.get() + endPrefixLen),
                apcBufferSize - endPrefixLen) };

            const int firstSep  { payload.indexOfChar (';') };
            const int secondSep { firstSep >= 0 ? payload.indexOfChar (firstSep + 1, ';') : -1 };

            int previewCols  { 0 };
            int previewLines { 0 };

            juce::String filepath;

            if (firstSep >= 0 and secondSep > firstSep)
            {
                filepath     = payload.substring (0, firstSep);
                previewCols  = payload.substring (firstSep + 1, secondSep).getIntValue();
                previewLines = payload.substring (secondSep + 1).getIntValue();
            }
            else
            {
                filepath = payload;
            }

            handleSkitFilepath (filepath, previewCols, previewLines);
        }
        else
        {
            const int cellW { physCellWidthAtomic.load  (std::memory_order_relaxed) };
            const int cellH { physCellHeightAtomic.load (std::memory_order_relaxed) };

            if (cellW > 0 and cellH > 0)
            {
                KittyDecoder::Result result { kittyDecoder.process (apcBuffer.get(),
                                                                    apcBufferSize) };

                if (result.response.isNotEmpty())
                    sendResponse (result.response.toRawUTF8());

                if (result.shouldDisplay and result.image.isValid())
                {
                    const ActiveScreen scr          { state.getRawValue<ActiveScreen> (ID::activeScreen) };
                    const int cursorRow             { state.getRawValue<int> (state.screenKey (scr, ID::cursorRow)) };
                    const int cursorCol             { state.getRawValue<int> (state.screenKey (scr, ID::cursorCol)) };
                    const int scrollbackUsed        { state.getRawValue<int> (ID::scrollbackUsed) };
                    const int absRow                { scrollbackUsed + cursorRow };

                    const auto span { metrics.cellSpan (result.image.width, result.image.height) };
                    const int cellCols { span.getWidth().value };
                    const int cellRows { result.placementRows > 0
                                             ? result.placementRows
                                             : span.getHeight().value };

                    if (onImageDecoded)
                    {
                        onImageDecoded (std::move (result.image.rgba),
                                        juce::HeapBlock<int>(),
                                        1,
                                        result.image.width, result.image.height,
                                        absRow, cursorCol,
                                        cellCols, cellRows,
                                        false);
                    }

                    cursorMoveDown (scr, cellRows, effectiveClampBottom (scr));
                    state.setCursorCol (scr, 0);
                }
            }
        }
    }

    apcBufferSize = 0;
}

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Terminal
