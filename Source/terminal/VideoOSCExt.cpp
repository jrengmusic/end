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

namespace terminal
{
/*____________________________________________________________________________*/

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
            if (events.contains (id::registerLink))
                events.get (id::registerLink, uriStr, paramStr);
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

void Video::handleOsc133 (int scr, const uint8_t* data, int dataLength) noexcept
{
    if (dataLength >= 1)
    {
        const int cRow { cursorRow.value };

        switch (data[0])
        {
            case 'A':
                if (events.contains (id::promptRow)) events.get (id::promptRow, int (cRow));
                break;

            case 'B':
                break;

            case 'C':
            {
                if (events.contains (id::outputBlockStart)) events.get (id::outputBlockStart, int (cRow));
                break;
            }

            case 'D':
                if (events.contains (id::outputBlockEnd)) events.get (id::outputBlockEnd, int (cRow));
                break;

            default:
                break;
        }
    }
}

void Video::handleOsc1337 (const uint8_t* data, int dataLength) noexcept
{
    if (dataLength > 0 and events.contains (id::osc1337Raw))
    {
        const int scr  { activeScreen };
        const int cRow { cursorRow.value };
        const int cCol { cursorCol.value };
        events.get (id::osc1337Raw, data, int (dataLength), int (cRow), int (cCol));
    }
}

/**______________________________END OF NAMESPACE______________________________*/
} // namespace terminal
