/**
 * @file VideoOSCExt.cpp
 * @brief OSC protocol-extension handlers — hyperlink, shell integration, iTerm2 image.
 *
 * This translation unit implements three complex OSC handlers that share the
 * characteristic of tracking multi-event state or writing to the grid:
 *
 * | Command | Protocol              | Handler           |
 * |---------|-----------------------|-------------------|
 * | OSC 8   | Hyperlink (RFC-draft) | `handleOsc8()`    |
 * | OSC 133 | Shell integration     | `handleOsc133()`  |
 * | OSC 1337| iTerm2 inline image   | `handleOsc1337()` |
 *
 * Basic metadata handlers (title, cwd, clipboard, notification, cursor color)
 * and `oscDispatch()` live in `VideoOSC.cpp`.
 *
 * @par Thread model
 * All functions in this file run exclusively on the **READER THREAD**.
 *
 * @see Video.h     — class declaration and full API documentation
 * @see VideoOSC.cpp — basic OSC handlers and dispatch
 * @see ITerm2Decoder — iTerm2 image decoder
 */

#include "Video.h"
#include <jam_tui/jam_tui.h>
#include "../data/Identifier.h"

namespace Terminal
{ /*____________________________________________________________________________*/

/**
 * @brief Handles OSC 8 — explicit hyperlink start/end.
 *
 * Payload (the data after the "8;" separator) has the form:
 *   params ; uri
 *
 * - Non-empty URI: registers the URI and stores the ID in `activeLinkId` so
 *                  that subsequent cell writes are stamped.
 * - Empty URI:     clears `activeLinkId` to 0, ending the stamp run.
 * - Malformed:     no separator found; clears `activeLinkId` to 0.
 *
 * @param data        Pointer to OSC payload bytes (after the "8;" separator).
 * @param dataLength  Number of bytes in `data`.
 *
 * @note READER THREAD only.
 */
void Video::handleOsc8 (const uint8_t* data, int dataLength) noexcept
{
    // READER THREAD
    // Find the semicolon that separates params from uri
    int semiPos { -1 };

    for (int i { 0 }; i < dataLength; ++i)
    {
        if (data[i] == ';')
        {
            semiPos = i;
            break;
        }
    }

    if (semiPos >= 0)
    {
        const int uriStart  { semiPos + 1 };
        const int uriLength { dataLength - uriStart };

        if (uriLength > 0)
        {
            // OSC 8 open — fire "registerLink" event; Processor handler registers
            // the URI in State and writes the returned ID back via setActiveLinkId().
            const juce::String uriStr  { juce::String::fromUTF8 (reinterpret_cast<const char*> (data + uriStart), uriLength) };
            const juce::String paramStr { juce::String::fromUTF8 (reinterpret_cast<const char*> (data), semiPos) };
            if (events.contains (ID::registerLink))
                events.get (ID::registerLink, uriStr, paramStr);
        }
        else
        {
            // OSC 8 close — clear stamp
            activeLinkId = 0;
        }
    }
    else
    {
        // Malformed — no separator found; clear stamp
        activeLinkId = 0;
    }
}

/**
 * @brief Handles OSC 133 — shell integration semantic prompt markers.
 *
 * Subcommands A and B are accepted and silently ignored; they exist in the
 * protocol but carry no information needed for output-block tracking.
 * Subcommand C marks the start of command output: the current cursor row is
 * recorded as the output block top and the scan-active flag is set, so that
 * subsequent LF events extend the tracked row range.  Subcommand D marks the
 * end of command output: the current cursor row is recorded as the block
 * bottom and the scan flag is cleared.
 *
 * @param scr         Active screen buffer selected at dispatch time.
 * @param data        Pointer to the OSC 133 subcommand byte(s) after `"133;"`.
 * @param dataLength  Number of bytes in `data` (must be >= 1 for a valid subcommand).
 * @note READER THREAD only.
 */
void Video::handleOsc133 (ActiveScreen scr, const uint8_t* data, int dataLength) noexcept
{
    if (dataLength >= 1)
    {
        const int cRow { cursorRow[static_cast<int> (scr)] };

        switch (data[0])
        {
            case 'A':
                if (events.contains (ID::promptRow)) events.get (ID::promptRow, int (cRow));
                break;

            case 'C':
                if (events.contains (ID::outputBlockStart)) events.get (ID::outputBlockStart, int (cRow));
                break;

            case 'D':
                if (events.contains (ID::outputBlockEnd)) events.get (ID::outputBlockEnd, int (cRow));
                break;

            default:
                break;
        }
    }
}

/**
 * @brief Handles OSC 1337 — iTerm2 inline image display or SKiT filepath signal.
 *
 * Fires the `"osc1337Raw"` event with the raw payload, payload length, and
 * current cursor position.  The Processor handler receives this event and
 * delegates to `Skit::processOSC1337()` for image decode and SKiT filepath
 * handling, then calls `Video::advanceCursorForImage()` with the result.
 *
 * @par Sequence
 * @code
 *   ESC ] 1337 ; File=[key=value;...] : <base64> BEL
 * @endcode
 *
 * @param data        Pointer to OSC payload bytes after "1337;".
 *                    Expected to begin with "File=" or "END;".
 * @param dataLength  Number of bytes in @p data.
 *
 * @note READER THREAD only.
 *
 * @see Skit::processOSC1337()
 * @see advanceCursorForImage()
 */
void Video::handleOsc1337 (const uint8_t* data, int dataLength) noexcept
{
    if (dataLength > 0 and events.contains (ID::osc1337Raw))
    {
        const ActiveScreen scr { activeScreen };
        const int cRow         { cursorRow[static_cast<int> (scr)] };
        const int cCol         { cursorCol[static_cast<int> (scr)] };
        events.get (ID::osc1337Raw, data, int (dataLength), int (cRow), int (cCol));
    }
}

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Terminal
