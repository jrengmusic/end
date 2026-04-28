/**
 * @file ParserOSCExt.cpp
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
 * and `oscDispatch()` live in `ParserOSC.cpp`.
 *
 * @par Thread model
 * All functions in this file run exclusively on the **READER THREAD**.
 *
 * @see Parser.h     — class declaration and full API documentation
 * @see ParserOSC.cpp — basic OSC handlers and dispatch
 * @see ITerm2Decoder — iTerm2 image decoder
 */

#include "Parser.h"
#include "ITerm2Decoder.h"

namespace Terminal
{ /*____________________________________________________________________________*/

/**
 * @brief Handles OSC 8 — explicit hyperlink start/end.
 *
 * Payload (the data after the "8;" separator) has the form:
 *   params ; uri
 *
 * - Non-empty URI: registers the URI with `state.registerLinkUri()` and stores
 *                  the returned ID in `activeLinkId` so that subsequent cell
 *                  writes are stamped.
 * - Empty URI:     clears `activeLinkId` to 0, ending the stamp run.
 * - Malformed:     no separator found; clears `activeLinkId` to 0.
 *
 * @param data        Pointer to OSC payload bytes (after the "8;" separator).
 * @param dataLength  Number of bytes in `data`.
 *
 * @note READER THREAD only.
 */
void Parser::handleOsc8 (const uint8_t* data, int dataLength) noexcept
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
            // OSC 8 open — register URI, set active stamp
            activeLinkId = state.registerLinkUri (
                reinterpret_cast<const char*> (data + uriStart), uriLength);
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
void Parser::handleOsc133 (ActiveScreen scr, const uint8_t* data, int dataLength) noexcept
{
    if (dataLength >= 1)
    {
        const int cursorRow { state.getRawValue<int> (state.screenKey (scr, ID::cursorRow)) };
        const int absoluteRow { state.getRawValue<int> (ID::scrollbackUsed) + cursorRow };

        switch (data[0])
        {
            case 'A':
                state.setPromptRow (absoluteRow);
                break;

            case 'C':
                state.setOutputBlockStart (absoluteRow);
                break;

            case 'D':
                state.setOutputBlockEnd (absoluteRow);
                break;

            default:
                break;
        }
    }
}

/**
 * @brief Handles OSC 1337 — iTerm2 inline image display.
 *
 * Delegates to `ITerm2Decoder` to parse the `File=` key=value header,
 * base64-decode the payload, and produce an RGBA8 `DecodedImage`.  On success,
 * reserves an image ID, writes image cells to the grid, and stores the decoded
 * image in Grid for the MESSAGE THREAD to pull on first atlas encounter.
 *
 * Silently skips when:
 * - `inline=0` or the key is absent (download-only mode).
 * - Cell dimensions are not yet calibrated (zero values).
 * - Decoder returns an invalid image.
 *
 * @par Sequence
 * @code
 *   ESC ] 1337 ; File=[key=value;...] : <base64> BEL
 * @endcode
 *
 * @param data        Pointer to OSC payload bytes after "1337;".
 *                    Expected to begin with "File=".
 * @param dataLength  Number of bytes in @p data.
 *
 * @note READER THREAD only.
 *
 * @see ITerm2Decoder
 * @see Grid::reserveImageId()
 * @see Grid::storeDecodedImage()
 */
void Parser::handleOsc1337 (const uint8_t* data, int dataLength) noexcept
{
    // READER THREAD
    const int cellW { physCellWidthAtomic.load  (std::memory_order_relaxed) };
    const int cellH { physCellHeightAtomic.load (std::memory_order_relaxed) };

    if (cellW > 0 and cellH > 0)
    {
        ITerm2Decoder decoder;
        DecodedImage image { decoder.decode (data, dataLength) };

        if (image.isValid())
        {
            const ActiveScreen scr { state.getRawValue<ActiveScreen> (ID::activeScreen) };
            const int cursorRow    { state.getRawValue<int> (state.screenKey (scr, ID::cursorRow)) };
            const int cursorCol    { state.getRawValue<int> (state.screenKey (scr, ID::cursorCol)) };

            const uint32_t imageId { writer.reserveImageId() };

            writer.activeWriteImage (cursorRow, cursorCol, imageId,
                                     image.width, image.height, cellW, cellH);

            const int cellRows { (image.height + cellH - 1) / cellH };
            cursorMoveDown (scr, cellRows, effectiveClampBottom (scr));
            state.setCursorCol (scr, 0);

            PendingImage pending;
            pending.imageId = imageId;
            pending.rgba    = std::move (image.rgba);
            pending.width   = image.width;
            pending.height  = image.height;

            writer.storeDecodedImage (std::move (pending));
        }
    }
}

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Terminal
