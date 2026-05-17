/**
 * @file Skit.cpp
 * @brief Image decode and SKiT filepath handler — implementation.
 *
 * Implements `Skit::processDCS()`, `Skit::processAPC()`, `Skit::processOSC1337()`,
 * and `Skit::handleSkitFilepath()`.  All decode logic that previously lived in
 * `VideoDCS.cpp` and `VideoOSCExt.cpp` has been consolidated here so that
 * image decoding is separated from VT command processing.
 *
 * @par Image pipeline (all three protocols share the same commit path)
 * 1. Receive raw payload + cursor position from Processor command handler.
 * 2. Check for SKiT prefix (`END;` / `GEND;`).
 * 3. On SKiT: fire `"previewFile"` event and return.
 * 4. On image: decode with protocol-specific decoder.
 * 5. Compute cell span from pixel dimensions and `cellWidth` / `cellHeight`.
 * 6. Fire `"imageDecoded"` event with pixel data and placement metadata.
 * 7. Set `lastImageRows` so Processor can advance the cursor via Video.
 *
 * @par Thread model
 * All methods run exclusively on the **READER THREAD**.
 * All methods run on the READER THREAD, including `setCellSize()`.
 *
 * @see Skit.h
 * @see SixelDecoder
 * @see KittyDecoder
 * @see ITerm2Decoder
 */

#include "Skit.h"
#include <jam_tui/jam_tui.h>
#include "../data/Identifier.h"
#include "ImageDecode.h"
#include "ITerm2Decoder.h"
#include "SixelDecoder.h"

namespace Terminal
{ /*____________________________________________________________________________*/

Skit::Skit (jam::Function::Map<juce::Identifier, void>& eventsRef) noexcept
    : events (eventsRef)
{
}

// =============================================================================

void Skit::setCellSize (int widthPx, int heightPx) noexcept
{
    cellWidth = widthPx;
    cellHeight = heightPx;
}

// =============================================================================

/**
 * @brief Handles an END; filepath signal from any SKiT protocol envelope.
 *
 * Fires the `"previewFile"` event with the filepath, cursor row, cursor column,
 * and optional protocol bounds.  An empty filepath signals preview dismissal.
 *
 * @param filepath   Absolute path extracted after the END;/GEND; marker.  Empty = dismiss.
 * @param cols       Protocol-specified overlay width in cells; 0 = use config.
 * @param lines      Protocol-specified overlay height in cells; 0 = use config.
 * @param cursorRow  Cursor row at signal time (screen-relative).
 * @param cursorCol  Cursor column at signal time.
 * @note READER THREAD only.
 */
void Skit::handleSkitFilepath (const juce::String& filepath, int cols, int lines,
                                int cursorRow, int cursorCol) noexcept
{
    if (events.contains (ID::previewFile))
        events.get (ID::previewFile, filepath, int (cursorRow), int (cursorCol), int (cols), int (lines));
}

// =============================================================================

/**
 * @brief Processes a DCS payload (Sixel or SKiT filepath).
 *
 * When `finalByte == 'q'` and `length > 0`, checks for the SKiT `END;`
 * prefix first.  If present delegates to `handleSkitFilepath()`.  Otherwise
 * runs `SixelDecoder`, fires `"imageDecoded"`, and sets `lastImageRows`.
 *
 * Silently skips when:
 * - `finalByte != 'q'` (not a Sixel sequence)
 * - Cell dimensions are zero (not yet calibrated)
 * - Decoder returns an invalid image
 *
 * @note READER THREAD only.
 */
void Skit::processDCS (uint8_t finalByte, const uint8_t* data, int length,
                        int cursorRow, int cursorCol) noexcept
{
    lastImageRows = 0_cell;
    lastResponse  = {};

    if (finalByte == 'q' and length > 0)
    {
        constexpr int endPrefixLen { 4 };

        if (length >= endPrefixLen
            and data[0] == 'E' and data[1] == 'N'
            and data[2] == 'D' and data[3] == ';')
        {
            const juce::String payload { juce::String::fromUTF8 (
                reinterpret_cast<const char*> (data + endPrefixLen),
                length - endPrefixLen) };

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

            handleSkitFilepath (filepath, previewCols, previewLines, cursorRow, cursorCol);
        }
        else
        {
            const int cellW { cellWidth };
            const int cellH { cellHeight };

            if (cellW > 0 and cellH > 0)
            {
                SixelDecoder decoder;
                DecodedImage image { decoder.decode (data, static_cast<size_t> (length)) };

                if (image.isValid())
                {
                    const auto span { cell::Rectangle (
                        jam::Bounds { cellW, cellH },
                        juce::Rectangle<int> { 0, 0, image.width, image.height },
                        true) };

                    const int cellCols { span.getWidth().value };
                    const int cellRows { span.getHeight().value };

                    if (events.contains (ID::imageDecoded))
                    {
                        events.get (ID::imageDecoded,
                            std::move (image.rgba),
                            juce::HeapBlock<int>(),
                            1,
                            int (image.width), int (image.height),
                            int (cursorRow), int (cursorCol),
                            int (cellCols), int (cellRows),
                            false);
                    }

                    lastImageRows = cell (cellRows);
                }
            }
        }
    }
}

// =============================================================================

/**
 * @brief Processes an APC payload (Kitty graphics or SKiT filepath).
 *
 * Checks for the SKiT `GEND;` prefix first.  If present delegates to
 * `handleSkitFilepath()`.  Otherwise runs `KittyDecoder`, fires `"imageDecoded"`,
 * and sets `lastImageRows` / `lastResponse`.
 *
 * Kitty responses (capability query, OK/ERROR acks) are stored in `lastResponse`
 * for the Processor to forward via the `"writeToHost"` event.
 *
 * Silently skips when:
 * - Cell dimensions are zero (not yet calibrated)
 * - Decoder produces no displayable image on this chunk
 *
 * @note READER THREAD only.
 */
void Skit::processAPC (const uint8_t* data, int length,
                        int cursorRow, int cursorCol) noexcept
{
    lastImageRows = 0_cell;
    lastResponse  = {};

    if (length > 0)
    {
        constexpr int endPrefixLen { 5 };

        if (length >= endPrefixLen
            and data[0] == 'G' and data[1] == 'E'
            and data[2] == 'N' and data[3] == 'D'
            and data[4] == ';')
        {
            const juce::String payload { juce::String::fromUTF8 (
                reinterpret_cast<const char*> (data + endPrefixLen),
                length - endPrefixLen) };

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

            handleSkitFilepath (filepath, previewCols, previewLines, cursorRow, cursorCol);
        }
        else
        {
            const int cellW { cellWidth };
            const int cellH { cellHeight };

            if (cellW > 0 and cellH > 0)
            {
                KittyDecoder::Result result { kittyDecoder.process (data, length) };

                lastResponse = result.response;

                if (result.shouldDisplay and result.image.isValid())
                {
                    const auto span { cell::Rectangle (
                        jam::Bounds { cellW, cellH },
                        juce::Rectangle<int> { 0, 0, result.image.width, result.image.height },
                        true) };

                    const int cellCols { span.getWidth().value };
                    const int cellRows { result.placementRows > 0
                                             ? result.placementRows
                                             : span.getHeight().value };

                    if (events.contains (ID::imageDecoded))
                    {
                        events.get (ID::imageDecoded,
                            std::move (result.image.rgba),
                            juce::HeapBlock<int>(),
                            1,
                            int (result.image.width), int (result.image.height),
                            int (cursorRow), int (cursorCol),
                            int (cellCols), int (cellRows),
                            false);
                    }

                    lastImageRows = cell (cellRows);
                }
            }
        }
    }
}

// =============================================================================

/**
 * @brief Processes an OSC 1337 payload (iTerm2 image or SKiT filepath).
 *
 * Checks for the SKiT `END;` prefix first.  If present delegates to
 * `handleSkitFilepath()`.  Otherwise runs `ITerm2Decoder`, fires
 * `"imageDecoded"`, and sets `lastImageRows`.
 *
 * Silently skips when:
 * - Cell dimensions are zero (not yet calibrated)
 * - `inline=0` or absent (download-only mode)
 * - Decoder returns an invalid ImageSequence
 *
 * @note READER THREAD only.
 */
void Skit::processOSC1337 (const uint8_t* data, int length,
                             int cursorRow, int cursorCol) noexcept
{
    lastImageRows = 0_cell;
    lastResponse  = {};

    if (length > 0)
    {
        constexpr int endPrefixLen { 4 };

        if (length >= endPrefixLen
            and data[0] == 'E' and data[1] == 'N'
            and data[2] == 'D' and data[3] == ';')
        {
            const juce::String payload { juce::String::fromUTF8 (
                reinterpret_cast<const char*> (data + endPrefixLen),
                length - endPrefixLen) };

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

            handleSkitFilepath (filepath, previewCols, previewLines, cursorRow, cursorCol);
        }
        else
        {
            const int cellW { cellWidth };
            const int cellH { cellHeight };

            if (cellW > 0 and cellH > 0)
            {
                ITerm2Decoder decoder;
                ImageSequence seq { decoder.decode (data, length) };

                if (seq.isValid())
                {
                    const auto span { cell::Rectangle (
                        jam::Bounds { cellW, cellH },
                        juce::Rectangle<int> { 0, 0, seq.width, seq.height },
                        true) };

                    const int cellCols { span.getWidth().value };
                    const int cellRows { span.getHeight().value };

                    if (events.contains (ID::imageDecoded))
                    {
                        events.get (ID::imageDecoded,
                            std::move (seq.pixels),
                            std::move (seq.delays),
                            int (seq.frameCount),
                            int (seq.width), int (seq.height),
                            int (cursorRow), int (cursorCol),
                            int (cellCols), int (cellRows),
                            false);
                    }

                    lastImageRows = cell (cellRows);
                }
            }
        }
    }
}

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Terminal
