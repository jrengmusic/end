/**
 * @file VideoCSI.cpp
 * @brief CSI (Control Sequence Introducer) sequence dispatch, cursor, scroll, and report handlers.
 *
 * This translation unit implements `Video::applyCSI()` and the cursor, scroll,
 * and report CSI handler functions.  Mode handlers live in VideoMode.cpp.
 * A CSI sequence has the form:
 *
 * @code
 *   ESC [ <params> <intermediates> <final>
 * @endcode
 *
 * where `<params>` is a semicolon-separated list of decimal integers,
 * `<intermediates>` is zero or more bytes in 0x20–0x2F, and `<final>` is a
 * single byte in 0x40–0x7E that identifies the command.
 *
 * @par Dispatch table
 * `applyCSI()` switches on the final byte and delegates to a dedicated
 * handler for each recognised sequence.  Private sequences (those with `?`
 * as the first intermediate byte) are routed to `handlePrivateMode()` for
 * DECSET/DECRST processing.
 *
 * @par Handlers implemented here
 * | Final | Sequence name | Handler                  |
 * |-------|---------------|--------------------------|
 * | A     | CUU           | `moveCursorUp()`         |
 * | B     | CUD           | `moveCursorDown()`       |
 * | C     | CUF           | `moveCursorForward()`    |
 * | D     | CUB           | `moveCursorBackward()`   |
 * | E     | CNL           | `moveCursorNextLine()`   |
 * | F     | CPL           | `moveCursorPrevLine()`   |
 * | G     | CHA           | `setCursorColumn()`      |
 * | H / f | CUP / HVP    | `setCursorPosition()`    |
 * | I     | CHT           | `cursorForwardTab()`     |
 * | J     | ED            | `eraseInDisplay()`       |
 * | K     | EL            | `eraseInLine()`          |
 * | L     | IL            | `shiftLinesDown()`       |
 * | M     | DL            | `shiftLinesUp()`         |
 * | P     | DCH           | `removeCells()`          |
 * | S     | SU            | `scrollUp()`             |
 * | T     | SD            | `scrollDown()`           |
 * | X     | ECH           | `eraseCells()`           |
 * | Z     | CBT           | `cursorBackTab()`        |
 * | @     | ICH           | `shiftCellsRight()`      |
 * | `     | HPA           | `setCursorColumn()`      |
 * | a     | HPR           | `moveCursorForward()`    |
 * | d     | VPA           | `setCursorLine()`        |
 * | e     | VPR           | `moveCursorDown()`       |
 * | h / l | SM / RM      | `handleMode()` / `handlePrivateMode()` (VideoMode.cpp) |
 * | m     | SGR           | `applySGR()`             |
 * | n     | DSR           | `reportCursorPosition()` |
 * | r     | DECSTBM       | `setScrollRegion()`      |
 * | c     | DA            | `reportDeviceAttributes()` |
 *
 * @par Thread model
 * All functions in this file run exclusively on the **READER THREAD**.
 *
 * @see Video.h      — class declaration and full API documentation
 * @see Video.cpp    — ground-state print and applyControlCode handlers
 * @see VideoMode.cpp — DEC private mode and ANSI mode handlers
 * @see VideoESC.cpp — ESC sequence dispatch
 * @see CSI          — parameter accumulator passed to every handler
 */

#include "Video.h"
#include "Grid.h"

namespace Terminal
{ /*____________________________________________________________________________*/

// ============================================================================
// Static Helpers (file-local)
// ============================================================================

/**
 * @brief Converts a one-based CSI parameter to a zero-based grid index.
 *
 * CSI sequences use one-based row/column numbering (e.g. `ESC[1;1H` means
 * row 0, column 0 internally).  This helper performs the conversion and
 * clamps the result to a minimum of 0.
 *
 * @param params      The CSI parameter set.
 * @param idx         Index of the parameter to read (0-based within `params`).
 * @param defaultVal  Value to use if the parameter is absent or zero.
 *
 * @return Zero-based grid index (`max(0, param - 1)`).
 *
 * @note Pure function — no side effects.
 */
static int paramToIndex (const CSI& params, int idx, uint16_t defaultVal) noexcept
{
    return juce::jmax (0, static_cast<int> (params.param (idx, defaultVal)) - 1);
}

/**
 * @brief Converts a zero-based grid index to a one-based CSI parameter value.
 *
 * Used when building device response strings (e.g. CPR — Cursor Position
 * Report) that must use one-based coordinates per the VT specification.
 *
 * @param index  Zero-based grid index.
 *
 * @return One-based parameter value (`index + 1`).
 *
 * @note Pure function — no side effects.
 */
static int indexToParam (int index) noexcept
{
    return index + 1;
}

// ============================================================================
// VT Handler: CSI dispatch
// ============================================================================

/**
 * @brief Dispatches a complete CSI sequence to the appropriate handler.
 *
 * Called by `performAction()` after the CSI final byte is received and the
 * parameter accumulator has been finalised.  Routes to the correct handler
 * based on `finalByte`.  Private sequences (those with `?` as the first
 * intermediate byte) are detected via `isPrivate` and routed to
 * `handlePrivateMode()` for DECSET/DECRST processing.
 *
 * @param params             Finalised CSI parameter accumulator.
 * @param inter              Pointer to the intermediate byte buffer.
 * @param interCount         Number of valid bytes in `inter`.
 * @param finalByte          The CSI final byte (0x40–0x7E).
 *
 * @note READER THREAD only.
 *
 * @see handlePrivateMode()
 * @see handleMode()
 * @see CSI
 */
void Video::applyCSI (const CSI& params, const uint8_t* inter, uint8_t interCount, uint8_t finalByte) noexcept
{
    const bool isPrivate { interCount > 0 and inter[0] == '?' };

    switch (finalByte)
    {
        case 'A': moveCursorUp (params);                break;
        case 'B': moveCursorDown (params);              break;
        case 'C': moveCursorForward (params);           break;
        case 'D': moveCursorBackward (params);          break;
        case 'E': moveCursorNextLine (params);          break;
        case 'F': moveCursorPrevLine (params);          break;
        case 'G': setCursorColumn (params);             break;
        case 'H':
        case 'f': setCursorPosition (params);            break;
        case 'I': cursorForwardTab (params);            break;
        case 'Z': cursorBackTab (params);               break;
        case 'a': moveCursorForward (params);           break;  // HPR — same as CUF
        case 'e': moveCursorDown (params);              break;  // VPR — same as CUD
        case '`': setCursorColumn (params);             break;  // HPA — same as CHA
        case 'J': eraseInDisplay (static_cast<int> (params.param (0, 0)));  break;
        case 'K': eraseInLine (static_cast<int> (params.param (0, 0)));     break;
        case 'L': shiftLinesDown (static_cast<int> (params.param (0, 1)));  break;
        case 'M': shiftLinesUp (static_cast<int> (params.param (0, 1)));    break;
        case 'P': removeCells (static_cast<int> (params.param (0, 1)));     break;
        case 'S': scrollUp (params);                    break;
        case 'T': scrollDown (params);                  break;
        case 'X': eraseCells (static_cast<int> (params.param (0, 1)));    break;
        case '@': shiftCellsRight (static_cast<int> (params.param (0, 1))); break;
        case 'd': setCursorLine (params);               break;
        case 'h': isPrivate ? handlePrivateMode (params, true)  : handleMode (params, true);  break;
        case 'l': isPrivate ? handlePrivateMode (params, false) : handleMode (params, false); break;
        case 'm': applySGR (params);                    break;
        case 'n': reportCursorPosition (params);        break;
        case 'r': setScrollRegion (params);             break;
        case 'b': repeatCharacter (static_cast<int> (params.param (0, 1))); break;
        case 'c': reportDeviceAttributes (isPrivate);   break;
        case 'g':
        {
            const int ps { static_cast<int> (params.param (0, 0)) };
            if (ps == 0)
            {
                clearTabStop();
            }
            else if (ps == 3)
            {
                clearAllTabStops();
            }
            break;
        }
        case 'q':
            if (interCount > 0 and inter[0] == ' ')
                handleCursorStyle (params);
            else if (interCount > 0 and inter[0] == '>' and params.param (0, 0) == 0)
                sendResponse ("\x1bP>|xterm(1.0)\x1b\\");
            break;
        case 't':
        {
            const int ps { static_cast<int> (params.param (0, 0)) };

            if (ps == 14)
            {
                // Report text area size in pixels: ESC [ 4 ; height ; width t
                const auto total { jam::metrics::Cell::Point::totalPixels<int> (jam::metrics::Cell { cols }, jam::metrics::Cell { visibleRows }, jam::Bounds { cellWidth, cellHeight }) };
                const int totalW { total.x };
                const int totalH { total.y };

                if (totalW > 0 and totalH > 0)
                {
                    const juce::String response { "\x1b[4;" + juce::String (totalH) + ";" + juce::String (totalW) + "t" };
                    sendResponse (response.toRawUTF8());
                }
            }
            else if (ps == 16)
            {
                // Report cell size in pixels: ESC [ 6 ; height ; width t
                if (cellWidth > 0 and cellHeight > 0)
                {
                    const juce::String response { "\x1b[6;" + juce::String (cellHeight) + ";" + juce::String (cellWidth) + "t" };
                    sendResponse (response.toRawUTF8());
                }
            }
            else if (ps == 18)
            {
                // Report text area size in characters: ESC [ 8 ; rows ; cols t
                const juce::String response { "\x1b[8;" + juce::String (visibleRows) + ";" + juce::String (cols) + "t" };
                sendResponse (response.toRawUTF8());
            }

            break;
        }
        case 'u': handleKeyboardMode (params, inter, interCount); break;
        default:  break;
    }
}

// ============================================================================
// CSI Handlers — cursor
// ============================================================================

/**
 * @brief Handles `CSI Pn A` — Cursor Up (CUU).
 *
 * Moves the cursor up by `Pn` rows (default 1), clamped to the top of the
 * scrolling region.  Does not change the cursor column.
 *
 * @param params  CSI parameters.  `params.param(0, 1)` is the row count.
 *
 * @note READER THREAD only.
 *
 * @see cursorMoveUp()
 */
void Video::moveCursorUp (const CSI& params) noexcept
{
    cursorMoveUp (static_cast<int> (params.param (0, 1)));
}

/**
 * @brief Handles `CSI Pn B` — Cursor Down (CUD).
 *
 * Moves the cursor down by `Pn` rows (default 1).
 *
 * @param params  CSI parameters.  `params.param(0, 1)` is the row count.
 *
 * @note READER THREAD only.
 *
 * @see cursorMoveDown()
 */
void Video::moveCursorDown (const CSI& params) noexcept
{
    cursorMoveDown (static_cast<int> (params.param (0, 1)), effectiveClampBottom());
}

/**
 * @brief Handles `CSI Pn C` — Cursor Forward (CUF).
 *
 * Moves the cursor right by `Pn` columns (default 1), clamped to the right
 * margin (`cols - 1`).  Does not change the cursor row.
 *
 * @param params  CSI parameters.  `params.param(0, 1)` is the column count.
 *
 * @note READER THREAD only.
 *
 * @see cursorMoveForward()
 */
void Video::moveCursorForward (const CSI& params) noexcept
{
    cursorMoveForward (static_cast<int> (params.param (0, 1)), cols);
}

/**
 * @brief Handles `CSI Pn D` — Cursor Backward (CUB).
 *
 * Moves the cursor left by `Pn` columns (default 1), clamped to column 0.
 * Does not change the cursor row.
 *
 * @param params  CSI parameters.  `params.param(0, 1)` is the column count.
 *
 * @note READER THREAD only.
 *
 * @see cursorMoveBackward()
 */
void Video::moveCursorBackward (const CSI& params) noexcept
{
    cursorMoveBackward (static_cast<int> (params.param (0, 1)));
}

/**
 * @brief Handles `CSI Pn E` — Cursor Next Line (CNL).
 *
 * Moves the cursor down by `Pn` rows (default 1) and sets the column to 0.
 *
 * @param params  CSI parameters.  `params.param(0, 1)` is the row count.
 *
 * @note READER THREAD only.
 *
 * @see cursorMoveDown()
 */
void Video::moveCursorNextLine (const CSI& params) noexcept
{
    const int count { static_cast<int> (params.param (0, 1)) };
    cursorMoveDown (count, effectiveClampBottom());
    cursorCol = 0;
}

/**
 * @brief Handles `CSI Pn F` — Cursor Previous Line (CPL).
 *
 * Moves the cursor up by `Pn` rows (default 1) and sets the column to 0.
 *
 * @param params  CSI parameters.  `params.param(0, 1)` is the row count.
 *
 * @note READER THREAD only.
 *
 * @see cursorMoveUp()
 */
void Video::moveCursorPrevLine (const CSI& params) noexcept
{
    const int count { static_cast<int> (params.param (0, 1)) };
    cursorMoveUp (count);
    cursorCol = 0;
}

/**
 * @brief Handles `CSI Pn I` — Cursor Forward Tabulation (CHT).
 *
 * Advances the cursor to the next tab stop `Pn` times (default 1).
 *
 * @param params  CSI parameters.  `params.param(0, 1)` is the tab count.
 *
 * @note READER THREAD only.
 *
 * @see nextTabStop()
 */
void Video::cursorForwardTab (const CSI& params) noexcept
{
    const int count { static_cast<int> (params.param (0, 1)) };

    for (int i { 0 }; i < count; ++i)
    {
        cursorCol = nextTabStop (cols);
    }

    wrapPending = false;
}

/**
 * @brief Handles `CSI Pn Z` — Cursor Backward Tabulation (CBT).
 *
 * Moves the cursor to the previous tab stop `Pn` times (default 1).
 *
 * @param params  CSI parameters.  `params.param(0, 1)` is the tab count.
 *
 * @note READER THREAD only.
 *
 * @see prevTabStop()
 */
void Video::cursorBackTab (const CSI& params) noexcept
{
    const int count { static_cast<int> (params.param (0, 1)) };

    for (int i { 0 }; i < count; ++i)
    {
        cursorCol = prevTabStop();
    }

    wrapPending = false;
}

/**
 * @brief Handles `CSI Pn G` — Cursor Horizontal Absolute (CHA).
 *
 * Sets the cursor column to `Pn - 1` (one-based input, zero-based internal),
 * clamped to `[0, cols - 1]`.  Clears the wrap-pending flag.
 *
 * @param params  CSI parameters.  `params.param(0, 1)` is the target column
 *                (one-based, default 1 → column 0).
 *
 * @note READER THREAD only.
 *
 * @see paramToIndex()
 */
void Video::setCursorColumn (const CSI& params) noexcept
{
    cursorCol = paramToIndex (params, 0, 1);
    wrapPending = false;
    cursorCol = juce::jlimit (0, cols - 1, cursorCol);
}

/**
 * @brief Handles `CSI Pr ; Pc H` / `CSI Pr ; Pc f` — Cursor Position (CUP / HVP).
 *
 * Sets the cursor to the absolute position (row, col), both one-based.
 * Respects origin mode (DECOM).
 *
 * @param params  CSI parameters.  `params.param(0, 1)` is the target row and
 *                `params.param(1, 1)` is the target column (both one-based,
 *                default 1 → row 0, column 0).
 *
 * @note READER THREAD only.
 *
 * @see moveCursorTo()
 * @see cursorSetPositionInOrigin()
 */
void Video::setCursorPosition (const CSI& params) noexcept
{
    moveCursorTo (paramToIndex (params, 0, 1), paramToIndex (params, 1, 1));
}

/**
 * @brief Handles `CSI Pn d` — Line Position Absolute (VPA).
 *
 * Sets the cursor row to `Pn - 1` (one-based input, zero-based internal),
 * preserving the current cursor column.
 *
 * @param params  CSI parameters.  `params.param(0, 1)` is the target row
 *                (one-based, default 1 → row 0).
 *
 * @note READER THREAD only.
 *
 * @see moveCursorTo()
 */
void Video::setCursorLine (const CSI& params) noexcept
{
    moveCursorTo (paramToIndex (params, 0, 1), cursorCol);
}

/**
 * @brief Moves the cursor to an absolute (row, col) position (zero-based).
 *
 * Internal helper used by `setCursorPosition()` and `setCursorLine()` after
 * converting from one-based CSI parameters.  Delegates to either
 * `cursorSetPositionInOrigin()` (when DECOM is active) or
 * `cursorSetPosition()` (normal mode).  Calls `calc()` after the move to
 * synchronise the cached scroll-region bottom.
 *
 * @param row  Zero-based target row.
 * @param col  Zero-based target column.
 *
 * @note READER THREAD only.
 *
 * @see cursorSetPosition()
 * @see cursorSetPositionInOrigin()
 * @see calc()
 */
void Video::moveCursorTo (int row, int col) noexcept
{
    const int colCount { cols };
    const int rowCount { visibleRows };

    if (originMode)
    {
        cursorSetPositionInOrigin (row, col, colCount, rowCount);
    }
    else
    {
        cursorSetPosition (row, col, colCount, rowCount);
    }

    calc();
}

// ============================================================================
// CSI Handlers — scroll
// ============================================================================

/**
 * @brief Handles `CSI Pn S` — Scroll Up (SU).
 *
 * Scrolls the active scroll region upward via Grid.  Lines scrolled off the
 * top are discarded; blank lines are inserted at the bottom.  The cursor
 * position is not changed.
 *
 * @param params  CSI parameters.  `params.param(0, 1)` is the line count.
 *
 * @note READER THREAD only.
 */
void Video::scrollUp (const CSI& params) noexcept
{
    const auto scr { activeScreen };
    const int scrTop { scrollTop };
    const int bottom { activeScrollBottom() };
    const int count { static_cast<int> (params.param (0, 1)) };
    const int clampedCount { juce::jmin (count, bottom - scrTop + 1) };

    grid.scrollUp (scr, scrTop, bottom, clampedCount);

    if (penBg.getAlpha() > 0)
    {
        const jam::Cell fill { jam::Cell::erase (eraseStyleId()) };

        for (int r { bottom - clampedCount + 1 }; r <= bottom; ++r)
        {
            jam::Cell* row { grid.getWritePointer (scr, r) };

            for (int c { 0 }; c < cols; ++c)
                row[c] = fill;
        }
    }
}

/**
 * @brief Handles `CSI Pn T` — Scroll Down (SD).
 *
 * Scrolls the active scroll region downward via Grid.  Lines scrolled off the
 * bottom are discarded; blank lines are inserted at the top.  The cursor
 * position is not changed.
 *
 * @param params  CSI parameters.  `params.param(0, 1)` is the line count.
 *
 * @note READER THREAD only.
 */
void Video::scrollDown (const CSI& params) noexcept
{
    const auto scr { activeScreen };
    const int scrTop { scrollTop };
    const int bottom { activeScrollBottom() };
    const int count { static_cast<int> (params.param (0, 1)) };
    const int clampedCount { juce::jmin (count, bottom - scrTop + 1) };

    grid.scrollDown (scr, scrTop, bottom, clampedCount);

    if (penBg.getAlpha() > 0)
    {
        const jam::Cell fill { jam::Cell::erase (eraseStyleId()) };

        for (int r { scrTop }; r < scrTop + clampedCount; ++r)
        {
            jam::Cell* row { grid.getWritePointer (scr, r) };

            for (int c { 0 }; c < cols; ++c)
                row[c] = fill;
        }
    }
}

/**
 * @brief Handles `CSI Pt ; Pb r` — Set Scrolling Region (DECSTBM).
 *
 * Sets the top and bottom margins of the scrolling region.  After setting the
 * region, the cursor is moved to the home position.
 *
 * @param params  CSI parameters.  `params.param(0, 1)` is the top margin
 *                (one-based) and `params.param(1, visibleRows)` is the bottom
 *                margin (one-based).
 *
 * @note READER THREAD only.
 *
 * @see cursorSetScrollRegion()
 * @see cursorResetScrollRegion()
 * @see calc()
 */
void Video::setScrollRegion (const CSI& params) noexcept
{
    const int top { paramToIndex (params, 0, 1) };
    const int bottom { paramToIndex (params, 1, static_cast<uint16_t> (visibleRows)) };

    if (top >= 0 and bottom > top and bottom < visibleRows)
    {
        cursorSetScrollRegion (top, bottom);
    }
    else
    {
        cursorResetScrollRegion();
    }

    calc();

    cursorSetPosition (0, 0, cols, visibleRows);
}

// ============================================================================
// CSI Handlers — report
// ============================================================================

/**
 * @brief Handles `CSI Pn n` — Device Status Report (DSR).
 *
 * Responds to terminal status queries from the host application.
 *
 * @param params  CSI parameters.  `params.param(0, 0)` selects the sub-command.
 *
 * @note READER THREAD only.  Responses are queued via `sendResponse()` and
 *       flushed after `process()` returns.
 *
 * @see sendResponse()
 * @see flushResponses()
 */
void Video::reportCursorPosition (const CSI& params) noexcept
{
    const auto scr { activeScreen };
    const auto modeValue { params.param (0, 0) };

    if (modeValue == 6)
    {
        char buf[32];
        std::snprintf (buf, sizeof (buf), "\x1b[%d;%dR", indexToParam (cursorRow), indexToParam (cursorCol));
        sendResponse (buf);
    }
    else if (modeValue == 5)
    {
        sendResponse ("\x1b[0n");
    }
}

/**
 * @brief Handles `CSI c` / `CSI ? c` — Device Attributes (DA1 / DA2).
 *
 * @param isPrivate  `true` if the sequence had a `>` intermediate (DA2).
 *
 * @note READER THREAD only.
 */
void Video::reportDeviceAttributes (bool isPrivate) noexcept
{
    if (isPrivate)
    {
        sendResponse ("\x1b[>65;100;0c");
    }
    else
    {
        sendResponse ("\x1b[?62;4c");
    }
}

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Terminal
