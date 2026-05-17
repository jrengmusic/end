/**
 * @file Skit.h
 * @brief Image decode and SKiT filepath handler.
 *
 * Encapsulates Sixel, Kitty, and iTerm2 image decoding plus the SKiT
 * filepath preview protocol.  Owned by Processor.  Receives raw payloads
 * from Video's DCS/APC/OSC handlers and fires events with decoded results.
 *
 * @par Thread model
 * All methods are **READER THREAD** only.  `setCellSize()` is called by
 * `Processor::process()` on the reader thread when State's cell dimension
 * atomics differ from Video's cached values.
 *
 * @see SixelDecoder
 * @see KittyDecoder
 * @see ITerm2Decoder
 */

#pragma once

#include <JuceHeader.h>
#include "KittyDecoder.h"

namespace Terminal
{ /*____________________________________________________________________________*/

/**
 * @class Skit
 * @brief Image decode and SKiT filepath handler.
 *
 * Encapsulates Sixel, Kitty, and iTerm2 image decoding plus the SKiT
 * filepath preview protocol.  Owned by Processor.  Receives raw payloads
 * from Video's DCS/APC/OSC handlers and fires events with decoded results.
 *
 * @see SixelDecoder
 * @see KittyDecoder
 * @see ITerm2Decoder
 */
class Skit
{
public:
    explicit Skit (jam::Function::Map<juce::Identifier, void>& events) noexcept;

    /** @brief Sets physical cell dimensions for image decode.
     *
     *  Called by Processor when Screen reports new font metrics.
     *
     *  @param widthPx   Cell width in pixels.
     *  @param heightPx  Cell height in pixels.
     *  @note READER THREAD only.
     */
    void setCellSize (int widthPx, int heightPx) noexcept;

    /** @brief Processes a DCS payload (Sixel or SKiT filepath).
     *
     *  When `finalByte == 'q'` and `length > 0`, checks for the SKiT `END;`
     *  prefix first.  If present delegates to `handleSkitFilepath()`.
     *  Otherwise runs `SixelDecoder`, fires `"imageDecoded"`, and sets
     *  `lastImageRows` for the caller to advance the cursor.
     *
     *  @param finalByte  DCS final byte stored by `Video::storeDCSHeader()`.
     *  @param data       Raw DCS payload bytes.
     *  @param length     Number of bytes in `data`.
     *  @param cursorRow  Cursor row at decode time (screen-relative).
     *  @param cursorCol  Cursor column at decode time.
     *  @note READER THREAD only.
     */
    void processDCS (uint8_t finalByte, const uint8_t* data, int length,
                     int cursorRow, int cursorCol) noexcept;

    /** @brief Processes an APC payload (Kitty graphics or SKiT filepath).
     *
     *  Checks for the SKiT `GEND;` prefix first.  If present delegates to
     *  `handleSkitFilepath()`.  Otherwise runs `KittyDecoder`, fires
     *  `"imageDecoded"`, and sets `lastImageRows` / `lastResponse`.
     *
     *  @param data       Raw APC payload bytes.
     *  @param length     Number of bytes in `data`.
     *  @param cursorRow  Cursor row at decode time (screen-relative).
     *  @param cursorCol  Cursor column at decode time.
     *  @note READER THREAD only.
     */
    void processAPC (const uint8_t* data, int length,
                     int cursorRow, int cursorCol) noexcept;

    /** @brief Processes an OSC 1337 payload (iTerm2 image or SKiT filepath).
     *
     *  Checks for the SKiT `END;` prefix first.  If present delegates to
     *  `handleSkitFilepath()`.  Otherwise runs `ITerm2Decoder`, fires
     *  `"imageDecoded"`, and sets `lastImageRows`.
     *
     *  @param data       Raw OSC payload bytes after `"1337;"`.
     *  @param length     Number of bytes in `data`.
     *  @param cursorRow  Cursor row at decode time (screen-relative).
     *  @param cursorCol  Cursor column at decode time.
     *  @note READER THREAD only.
     */
    void processOSC1337 (const uint8_t* data, int length,
                         int cursorRow, int cursorCol) noexcept;

    /** @brief Returns the number of cell rows the last image placement occupied.
     *
     *  Video uses this to advance the cursor after decode.  0 = no image placed.
     */
    cell getLastImageRows() const noexcept { return lastImageRows; }

    /** @brief Returns the response string from the last Kitty decode (capability query, ack).
     *
     *  Empty if no response was produced.
     */
    const juce::String& getLastResponse() const noexcept { return lastResponse; }

private:
    /** @brief Events map owned by Processor.  Skit fires events through this map. */
    jam::Function::Map<juce::Identifier, void>& events;

    /** @brief Kitty graphics protocol decoder — persistent across APC chunks. */
    KittyDecoder kittyDecoder;

    /** @brief Cell width in pixels.  Reader thread only.  Calculation input for image decode. */
    int cellWidth { 0 };

    /** @brief Cell height in pixels.  Reader thread only.  Calculation input for image decode. */
    int cellHeight { 0 };

    /** @brief Number of cell rows occupied by the last decoded image placement.  0 = none. */
    cell lastImageRows { 0 };

    /** @brief Response string from the last Kitty decode.  Empty if none. */
    juce::String lastResponse;

    /** @brief Handles an END; filepath signal from any SKiT protocol envelope.
     *
     *  Parses optional `cols;lines` suffix, then fires the `"previewFile"` event
     *  with the filepath, cursor row, cursor column, and optional bounds.
     *  An empty `filepath` signals preview dismissal.
     *
     *  @param filepath   Absolute path extracted after the END;/GEND; marker.  Empty = dismiss.
     *  @param cols       Protocol-specified overlay width in cells; 0 = use config.
     *  @param lines      Protocol-specified overlay height in cells; 0 = use config.
     *  @param cursorRow  Cursor row at signal time (screen-relative).
     *  @param cursorCol  Cursor column at signal time.
     *  @note READER THREAD only.
     */
    void handleSkitFilepath (const juce::String& filepath, int cols, int lines,
                             int cursorRow, int cursorCol) noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Skit)
};

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Terminal
