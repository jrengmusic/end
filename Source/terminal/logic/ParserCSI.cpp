/**
 * @file ParserCSI.cpp
 * @brief CSI (Control Sequence Introducer) sequence dispatch and handlers.
 *
 * This translation unit implements `Parser::csiDispatch()` and every CSI
 * handler function.  A CSI sequence has the form:
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
 * `csiDispatch()` switches on the final byte and delegates to a dedicated
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
 * | h / l | SM / RM      | `handleMode()` / `handlePrivateMode()` |
 * | m     | SGR           | `applySGR()`             |
 * | n     | DSR           | `reportCursorPosition()` |
 * | r     | DECSTBM       | `setScrollRegion()`      |
 * | c     | DA            | `reportDeviceAttributes()` |
 *
 * @par Thread model
 * All functions in this file run exclusively on the **READER THREAD**.
 *
 * @see Parser.h      — class declaration and full API documentation
 * @see ParserVT.cpp  — ground-state print and execute handlers
 * @see ParserESC.cpp — ESC sequence dispatch
 * @see CSI           — parameter accumulator passed to every handler
 * @see State         — atomic terminal parameter store
 */

#include "Parser.h"
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
 * @par Sequence format
 * @code
 *   ESC [ <Pm> <final>          — standard sequence
 *   ESC [ ? <Pm> h / l         — private mode set / reset (DECSET / DECRST)
 * @endcode
 *
 * @par Dispatch table
 * | `finalByte` | Sequence | Handler                                         |
 * |-------------|----------|-------------------------------------------------|
 * | 'A'         | CUU      | `moveCursorUp(params)`                          |
 * | 'B'         | CUD      | `moveCursorDown(params)`                        |
 * | 'C'         | CUF      | `moveCursorForward(params)`                     |
 * | 'D'         | CUB      | `moveCursorBackward(params)`                    |
 * | 'E'         | CNL      | `moveCursorNextLine(params)`                    |
 * | 'F'         | CPL      | `moveCursorPrevLine(params)`                    |
 * | 'G'         | CHA      | `setCursorColumn(params)`                       |
 * | 'H' / 'f'   | CUP/HVP  | `setCursorPosition(params)`                     |
 * | 'I'         | CHT      | `cursorForwardTab(params)`                      |
 * | 'J'         | ED       | `eraseInDisplay(param0)`                        |
 * | 'K'         | EL       | `eraseInLine(param0)`                           |
 * | 'L'         | IL       | `shiftLinesDown(param0)`                        |
 * | 'M'         | DL       | `shiftLinesUp(param0)`                          |
 * | 'P'         | DCH      | `removeCells(param0)`                           |
 * | 'S'         | SU       | `scrollUp(params)`                              |
 * | 'T'         | SD       | `scrollDown(params)`                            |
 * | 'X'         | ECH      | `eraseCells(param0)`                            |
 * | 'Z'         | CBT      | `cursorBackTab(params)`                         |
 * | '@'         | ICH      | `shiftCellsRight(param0)`                       |
 * | '`'         | HPA      | `setCursorColumn(params)`                       |
 * | 'a'         | HPR      | `moveCursorForward(params)`                     |
 * | 'd'         | VPA      | `setCursorLine(params)`                         |
 * | 'e'         | VPR      | `moveCursorDown(params)`                        |
 * | 'h' / 'l'   | SM/RM    | `handleMode()` or `handlePrivateMode()`         |
 * | 'm'         | SGR      | `applySGR(params)`                              |
 * | 'n'         | DSR      | `reportCursorPosition(params)`                  |
 * | 'r'         | DECSTBM  | `setScrollRegion(params)`                       |
 * | 'c'         | DA       | `reportDeviceAttributes(isPrivate)`             |
 * | 't'         | XTWINOPS | (ignored — window manipulation not supported)   |
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
void Parser::csiDispatch (const CSI& params, const uint8_t* inter, uint8_t interCount, uint8_t finalByte) noexcept
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
                clearTabStop (state.getRawValue<ActiveScreen> (ID::activeScreen));
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
        case 't': break;
        case 'u': handleKeyboardMode (params, inter, interCount); break;
        default:  break;
    }
}

void Parser::handleCursorStyle (const CSI& params) noexcept
{
    const int ps { static_cast<int> (params.param (0, 0)) };

    if (ps >= 0 and ps <= 6)
    {
        state.setCursorShape (state.getRawValue<ActiveScreen> (ID::activeScreen), ps);
    }
}

// ============================================================================
// CSI Handler — Kitty Keyboard Protocol
// ============================================================================

/**
 * @brief Handles kitty keyboard protocol sequences (`CSI … u`).
 *
 * Dispatches based on the intermediate byte collected before the params:
 *
 * | Intermediate | Sequence              | Action                          |
 * |--------------|-----------------------|---------------------------------|
 * | `>`          | `CSI > flags u`       | Push flags onto stack           |
 * | `<`          | `CSI < count u`       | Pop count entries from stack    |
 * | `?`          | `CSI ? u`             | Query — respond `CSI ? flags u` |
 * | `=`          | `CSI = flags ; mode u`| Set / OR / AND-NOT flags        |
 * | (none)       | `CSI … u`             | No-op (future: key event input) |
 *
 * @param params      CSI parameter accumulator.
 * @param inter       Intermediate byte buffer.
 * @param interCount  Number of intermediate bytes collected.
 * @note READER THREAD only.
 */
void Parser::handleKeyboardMode (const CSI& params, const uint8_t* inter, uint8_t interCount) noexcept
{
    if (interCount > 0)
    {
        const auto scr { state.getRawValue<ActiveScreen> (ID::activeScreen) };

        if (inter[0] == '>')
        {
            const auto flags { static_cast<uint32_t> (params.param (0, 0)) };
            state.pushKeyboardMode (scr, flags);
        }
        else if (inter[0] == '<')
        {
            const auto count { static_cast<int> (params.param (0, 1)) };
            state.popKeyboardMode (scr, count);
        }
        else if (inter[0] == '?')
        {
            const auto flags { state.getRawValue<int> (state.screenKey (scr, ID::keyboardFlags)) };
            char buf[32];
            std::snprintf (buf, sizeof (buf), "\x1b[?%uu", flags);
            sendResponse (buf);
        }
        else if (inter[0] == '=')
        {
            const auto flags { static_cast<uint32_t> (params.param (0, 0)) };
            const auto mode { static_cast<int> (params.param (1, 1)) };
            state.setKeyboardMode (scr, flags, mode);
        }
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
 * @par Sequence
 * @code
 *   ESC [ Pn A
 * @endcode
 *
 * @param params  CSI parameters.  `params.param(0, 1)` is the row count.
 *
 * @note READER THREAD only.
 *
 * @see cursorMoveUp()
 */
void Parser::moveCursorUp (const CSI& params) noexcept
{
    const auto scr { state.getRawValue<ActiveScreen> (ID::activeScreen) };
    cursorMoveUp (scr, static_cast<int> (params.param (0, 1)));
}

/**
 * @brief Handles `CSI Pn B` — Cursor Down (CUD).
 *
 * Moves the cursor down by `Pn` rows (default 1).  The clamp boundary
 * depends on whether the cursor is within the scroll region:
 * - Within margins (row >= scrollTop and row <= scrollBottom): clamp to scrollBottom.
 * - Outside margins: clamp to visibleRows - 1.
 *
 * @par Sequence
 * @code
 *   ESC [ Pn B
 * @endcode
 *
 * @param params  CSI parameters.  `params.param(0, 1)` is the row count.
 *
 * @note READER THREAD only.
 *
 * @see cursorMoveDown()
 */
void Parser::moveCursorDown (const CSI& params) noexcept
{
    const auto scr { state.getRawValue<ActiveScreen> (ID::activeScreen) };
    cursorMoveDown (scr, static_cast<int> (params.param (0, 1)), effectiveClampBottom (scr));
}

/**
 * @brief Handles `CSI Pn C` — Cursor Forward (CUF).
 *
 * Moves the cursor right by `Pn` columns (default 1), clamped to the right
 * margin (`cols - 1`).  Does not change the cursor row.
 *
 * @par Sequence
 * @code
 *   ESC [ Pn C
 * @endcode
 *
 * @param params  CSI parameters.  `params.param(0, 1)` is the column count.
 *
 * @note READER THREAD only.
 *
 * @see cursorMoveForward()
 */
void Parser::moveCursorForward (const CSI& params) noexcept
{
    const auto scr { state.getRawValue<ActiveScreen> (ID::activeScreen) };
    cursorMoveForward (scr, static_cast<int> (params.param (0, 1)), grid.getCols());
}

/**
 * @brief Handles `CSI Pn D` — Cursor Backward (CUB).
 *
 * Moves the cursor left by `Pn` columns (default 1), clamped to column 0.
 * Does not change the cursor row.
 *
 * @par Sequence
 * @code
 *   ESC [ Pn D
 * @endcode
 *
 * @param params  CSI parameters.  `params.param(0, 1)` is the column count.
 *
 * @note READER THREAD only.
 *
 * @see cursorMoveBackward()
 */
void Parser::moveCursorBackward (const CSI& params) noexcept
{
    const auto scr { state.getRawValue<ActiveScreen> (ID::activeScreen) };
    cursorMoveBackward (scr, static_cast<int> (params.param (0, 1)));
}

/**
 * @brief Handles `CSI Pn E` — Cursor Next Line (CNL).
 *
 * Moves the cursor down by `Pn` rows (default 1) and sets the column to 0.
 * Equivalent to `Pn` line feeds followed by a carriage return.  The clamp
 * boundary depends on whether the cursor is within the scroll region:
 * - Within margins (row >= scrollTop and row <= scrollBottom): clamp to scrollBottom.
 * - Outside margins: clamp to visibleRows - 1.
 *
 * @par Sequence
 * @code
 *   ESC [ Pn E
 * @endcode
 *
 * @param params  CSI parameters.  `params.param(0, 1)` is the row count.
 *
 * @note READER THREAD only.
 *
 * @see cursorMoveDown()
 */
void Parser::moveCursorNextLine (const CSI& params) noexcept
{
    const auto scr { state.getRawValue<ActiveScreen> (ID::activeScreen) };
    const int count { static_cast<int> (params.param (0, 1)) };
    cursorMoveDown (scr, count, effectiveClampBottom (scr));
    state.setCursorCol (scr, 0);
}

/**
 * @brief Handles `CSI Pn F` — Cursor Previous Line (CPL).
 *
 * Moves the cursor up by `Pn` rows (default 1) and sets the column to 0.
 * Equivalent to `Pn` reverse line feeds followed by a carriage return.
 *
 * @par Sequence
 * @code
 *   ESC [ Pn F
 * @endcode
 *
 * @param params  CSI parameters.  `params.param(0, 1)` is the row count.
 *
 * @note READER THREAD only.
 *
 * @see cursorMoveUp()
 */
void Parser::moveCursorPrevLine (const CSI& params) noexcept
{
    const auto scr { state.getRawValue<ActiveScreen> (ID::activeScreen) };
    const int count { static_cast<int> (params.param (0, 1)) };
    cursorMoveUp (scr, count);
    state.setCursorCol (scr, 0);
}

/**
 * @brief Handles `CSI Pn I` — Cursor Forward Tabulation (CHT).
 *
 * Advances the cursor to the next tab stop `Pn` times (default 1).  Each
 * iteration calls `nextTabStop()` to advance to the nearest stop to the
 * right, stopping at the right margin if no further stops exist.  The
 * wrap-pending flag is cleared after the final move.
 *
 * @par Sequence
 * @code
 *   ESC [ Pn I
 * @endcode
 *
 * @param params  CSI parameters.  `params.param(0, 1)` is the tab count.
 *
 * @note READER THREAD only.
 *
 * @see nextTabStop()
 */
void Parser::cursorForwardTab (const CSI& params) noexcept
{
    const auto scr { state.getRawValue<ActiveScreen> (ID::activeScreen) };
    const int cols { grid.getCols() };
    const int count { static_cast<int> (params.param (0, 1)) };

    for (int i { 0 }; i < count; ++i)
    {
        state.setCursorCol (scr, nextTabStop (scr, cols));
    }

    state.setWrapPending (scr, false);
}

/**
 * @brief Handles `CSI Pn Z` — Cursor Backward Tabulation (CBT).
 *
 * Moves the cursor to the previous tab stop `Pn` times (default 1).  Each
 * iteration calls `prevTabStop()` to retreat to the nearest stop to the
 * left, stopping at column 0 if no further stops exist.  The wrap-pending
 * flag is cleared after the final move.
 *
 * @par Sequence
 * @code
 *   ESC [ Pn Z
 * @endcode
 *
 * @param params  CSI parameters.  `params.param(0, 1)` is the tab count.
 *
 * @note READER THREAD only.
 *
 * @see prevTabStop()
 */
void Parser::cursorBackTab (const CSI& params) noexcept
{
    const auto scr { state.getRawValue<ActiveScreen> (ID::activeScreen) };
    const int count { static_cast<int> (params.param (0, 1)) };

    for (int i { 0 }; i < count; ++i)
    {
        state.setCursorCol (scr, prevTabStop (scr));
    }

    state.setWrapPending (scr, false);
}

/**
 * @brief Handles `CSI Pn G` — Cursor Horizontal Absolute (CHA).
 *
 * Sets the cursor column to `Pn - 1` (one-based input, zero-based internal),
 * clamped to `[0, cols - 1]`.  Clears the wrap-pending flag.
 *
 * @par Sequence
 * @code
 *   ESC [ Pn G
 * @endcode
 *
 * @param params  CSI parameters.  `params.param(0, 1)` is the target column
 *                (one-based, default 1 → column 0).
 *
 * @note READER THREAD only.
 *
 * @see paramToIndex()
 */
void Parser::setCursorColumn (const CSI& params) noexcept
{
    const auto scr { state.getRawValue<ActiveScreen> (ID::activeScreen) };
    state.setCursorCol (scr, paramToIndex (params, 0, 1));
    state.setWrapPending (scr, false);
    state.setCursorCol (scr, juce::jlimit (0, grid.getCols() - 1, state.getRawValue<int> (state.screenKey (scr, ID::cursorCol))));
}

/**
 * @brief Handles `CSI Pr ; Pc H` / `CSI Pr ; Pc f` — Cursor Position (CUP / HVP).
 *
 * Sets the cursor to the absolute position (row, col), both one-based.
 * Respects origin mode (DECOM): when origin mode is active, row 1 refers to
 * the top of the scrolling region rather than the top of the screen.
 *
 * @par Sequences
 * @code
 *   ESC [ Pr ; Pc H    — CUP (Cursor Position)
 *   ESC [ Pr ; Pc f    — HVP (Horizontal and Vertical Position)
 * @endcode
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
void Parser::setCursorPosition (const CSI& params) noexcept
{
    moveCursorTo (paramToIndex (params, 0, 1), paramToIndex (params, 1, 1));
}

/**
 * @brief Handles `CSI Pn d` — Line Position Absolute (VPA).
 *
 * Sets the cursor row to `Pn - 1` (one-based input, zero-based internal),
 * preserving the current cursor column.
 *
 * @par Sequence
 * @code
 *   ESC [ Pn d
 * @endcode
 *
 * @param params  CSI parameters.  `params.param(0, 1)` is the target row
 *                (one-based, default 1 → row 0).
 *
 * @note READER THREAD only.
 *
 * @see moveCursorTo()
 */
void Parser::setCursorLine (const CSI& params) noexcept
{
    const auto scr { state.getRawValue<ActiveScreen> (ID::activeScreen) };
    moveCursorTo (paramToIndex (params, 0, 1), state.getRawValue<int> (state.screenKey (scr, ID::cursorCol)));
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
void Parser::moveCursorTo (int row, int col) noexcept
{
    const auto scr { state.getRawValue<ActiveScreen> (ID::activeScreen) };
    const int cols { grid.getCols() };
    const int visibleRows { grid.getVisibleRows() };

    if (state.getRawValue<bool> (state.modeKey (ID::originMode)))
    {
        cursorSetPositionInOrigin (scr, row, col, cols, visibleRows);
    }
    else
    {
        cursorSetPosition (scr, row, col, cols, visibleRows);
    }

    calc();
}

// ============================================================================
// CSI Handlers — scroll
// ============================================================================

/**
 * @brief Handles `CSI Pn S` — Scroll Up (SU).
 *
 * Scrolls the active scrolling region up by `Pn` lines (default 1).  Lines
 * scrolled off the top are discarded; blank lines are inserted at the bottom.
 * The cursor position is not changed.
 *
 * @par Sequence
 * @code
 *   ESC [ Pn S
 * @endcode
 *
 * @param params  CSI parameters.  `params.param(0, 1)` is the line count.
 *
 * @note READER THREAD only.
 *
 * @see Grid::scrollRegionUp()
 */
void Parser::scrollUp (const CSI& params) noexcept
{
    const auto scr { state.getRawValue<ActiveScreen> (ID::activeScreen) };
    const int bottom { activeScrollBottom() };
    const int count { static_cast<int> (params.param (0, 1)) };

    Cell fill {};
    fill.bg = stamp.bg;

    grid.scrollRegionUp (state.getRawValue<int> (state.screenKey (scr, ID::scrollTop)), bottom, count, fill);
}

/**
 * @brief Handles `CSI Pn T` — Scroll Down (SD).
 *
 * Scrolls the active scrolling region down by `Pn` lines (default 1).  Lines
 * scrolled off the bottom are discarded; blank lines are inserted at the top.
 * The cursor position is not changed.
 *
 * @par Sequence
 * @code
 *   ESC [ Pn T
 * @endcode
 *
 * @param params  CSI parameters.  `params.param(0, 1)` is the line count.
 *
 * @note READER THREAD only.
 *
 * @see Grid::scrollRegionDown()
 */
void Parser::scrollDown (const CSI& params) noexcept
{
    const auto scr { state.getRawValue<ActiveScreen> (ID::activeScreen) };
    const int bottom { activeScrollBottom() };
    const int count { static_cast<int> (params.param (0, 1)) };

    Cell fill {};
    fill.bg = stamp.bg;

    grid.scrollRegionDown (state.getRawValue<int> (state.screenKey (scr, ID::scrollTop)), bottom, count, fill);
}

/**
 * @brief Handles `CSI Pt ; Pb r` — Set Scrolling Region (DECSTBM).
 *
 * Sets the top and bottom margins of the scrolling region.  After setting the
 * region, the cursor is moved to the home position (row 0, column 0 in normal
 * mode; top of scroll region in origin mode).
 *
 * @par Sequence
 * @code
 *   ESC [ Pt ; Pb r
 * @endcode
 *
 * @par Validation
 * The region is accepted only if `top >= 0`, `bottom > top`, and
 * `bottom < visibleRows`.  If the parameters are invalid or omitted, the
 * scroll region is reset to the full screen height via
 * `cursorResetScrollRegion()`.
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
void Parser::setScrollRegion (const CSI& params) noexcept
{
    const auto scr { state.getRawValue<ActiveScreen> (ID::activeScreen) };
    const int visibleRows { grid.getVisibleRows() };
    const int top { paramToIndex (params, 0, 1) };
    const int bottom { paramToIndex (params, 1, static_cast<uint16_t> (visibleRows)) };

    if (top >= 0 and bottom > top and bottom < visibleRows)
    {
        cursorSetScrollRegion (scr, top, bottom);
    }
    else
    {
        cursorResetScrollRegion (scr);
    }

    calc();

    const int cols { grid.getCols() };
    cursorSetPosition (scr, 0, 0, cols, visibleRows);
}

// ============================================================================
// CSI Handlers — report
// ============================================================================

/**
 * @brief Handles `CSI Pn n` — Device Status Report (DSR).
 *
 * Responds to terminal status queries from the host application.
 *
 * @par Supported sub-commands
 * | `Pn` | Query              | Response                                    |
 * |------|--------------------|---------------------------------------------|
 * | 5    | Terminal status    | `ESC [ 0 n` (terminal OK)                   |
 * | 6    | Cursor position    | `ESC [ row ; col R` (CPR — one-based)       |
 *
 * @par Sequence
 * @code
 *   ESC [ 5 n    — request terminal status
 *   ESC [ 6 n    — request cursor position (CPR)
 * @endcode
 *
 * @par Response format (CPR)
 * @code
 *   ESC [ <row> ; <col> R
 * @endcode
 * where `<row>` and `<col>` are one-based.
 *
 * @param params  CSI parameters.  `params.param(0, 0)` selects the sub-command.
 *
 * @note READER THREAD only.  Responses are queued via `sendResponse()` and
 *       flushed after `process()` returns.
 *
 * @see sendResponse()
 * @see flushResponses()
 */
void Parser::reportCursorPosition (const CSI& params) noexcept
{
    const auto scr { state.getRawValue<ActiveScreen> (ID::activeScreen) };
    const auto modeValue { params.param (0, 0) };

    if (modeValue == 6)
    {
        char buf[32];
        std::snprintf (buf, sizeof (buf), "\x1b[%d;%dR", indexToParam (state.getRawValue<int> (state.screenKey (scr, ID::cursorRow))), indexToParam (state.getRawValue<int> (state.screenKey (scr, ID::cursorCol))));
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
 * Responds to DA1 (`CSI c` or `CSI 0 c`) with a VT220-compatible response
 * identifying the terminal's capabilities:
 *
 * @code
 *   CSI ? 62 ; 4 c
 * @endcode
 *
 * - `62` — VT220 conformance level.
 * - `4`  — Sixel graphics (declared for compatibility; not yet implemented).
 *
 * DA2 (`CSI > c`) responds with a VT500-family identification:
 *
 * @code
 *   CSI > 65 ; 100 ; 0 c
 * @endcode
 *
 * - `65`  — VT500 family (Pp).
 * - `100` — Firmware version (Pv).
 * - `0`   — ROM cartridge registration (Pc).
 *
 * @par Sequences
 * @code
 *   ESC [ c      — Primary DA (DA1)
 *   ESC [ 0 c    — Primary DA (DA1, explicit parameter)
 *   ESC [ > c    — Secondary DA (DA2)
 * @endcode
 *
 * @param isPrivate  `true` if the sequence had a `>` intermediate (DA2).
 *
 * @note READER THREAD only.
 */
void Parser::reportDeviceAttributes (bool isPrivate) noexcept
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

// ============================================================================
// VT Handler: Private Mode (DECSET / DECRST)
// ============================================================================

namespace
{
    /**
     * @brief Lookup-table entry mapping a DECSET/DECRST mode number to a State ID.
     *
     * Used by `applyPrivateModeTable()` to resolve the most common private
     * mode numbers without a large switch statement.
     *
     * @see privateModeTable
     * @see applyPrivateModeTable()
     */
    struct PrivateModeEntry { uint16_t modeValue; juce::Identifier id; };

    /**
     * @brief Table of DECSET/DECRST mode numbers and their corresponding State IDs.
     *
     * Covers the private modes that map directly to a single `State::setMode()`
     * call.  Modes that require additional side effects (e.g. ?6 origin mode,
     * ?25 cursor visibility, ?1049 alternate screen) are handled separately in
     * `handlePrivateMode()`.
     *
     * | Mode | Name                  | State ID                  |
     * |------|-----------------------|---------------------------|
     * |    1 | DECCKM                | applicationCursor         |
     * |    5 | DECSCNM               | reverseVideo              |
     * |    7 | DECAWM                | autoWrap                  |
     * |   66 | DECNKM                | applicationKeypad         |
     * | 1000 | X10 mouse tracking    | mouseTracking             |
     * | 1002 | Button-event tracking | mouseMotionTracking       |
     * | 1003 | Any-event tracking    | mouseAllTracking          |
     * | 1004 | Focus events          | focusEvents               |
     * | 1006 | SGR mouse encoding    | mouseSgr                  |
     * | 2004 | Bracketed paste       | bracketedPaste            |
     * | 9001 | Win32 input mode      | win32InputMode            |
     */
    static const PrivateModeEntry privateModeTable[]
    {
        {    1, ID::applicationCursor    },
        {    5, ID::reverseVideo         },
        {    7, ID::autoWrap             },
        {   66, ID::applicationKeypad   },
        { 1000, ID::mouseTracking        },
        { 1002, ID::mouseMotionTracking  },
        { 1003, ID::mouseAllTracking     },
        { 1004, ID::focusEvents          },
        { 1006, ID::mouseSgr             },
        { 2004, ID::bracketedPaste       },
        { 9001, ID::win32InputMode       },
    };

    /**
     * @brief Applies a private mode number via the lookup table.
     *
     * Searches `privateModeTable` for an entry matching `modeValue`.  If found,
     * calls `State::setMode()` with the corresponding ID and `enable`.
     *
     * @param state      Terminal parameter store to update.
     * @param modeValue  The DECSET/DECRST mode number.
     * @param enable     `true` to set the mode, `false` to reset it.
     *
     * @return `true` if the mode was found in the table and applied;
     *         `false` if the caller must handle it separately.
     *
     * @note READER THREAD only.
     */
    static bool applyPrivateModeTable (State& state, uint16_t modeValue, bool enable) noexcept
    {
        bool found { false };

        for (const auto& entry : privateModeTable)
        {
            if (entry.modeValue == modeValue and not found)
            {
                state.setMode (entry.id, enable);
                found = true;
            }
        }

        return found;
    }
}

/**
 * @brief Handles `CSI ? Pm h` / `CSI ? Pm l` — DEC Private Mode Set/Reset (DECSET/DECRST).
 *
 * Iterates over all parameters in `params` and enables or disables the
 * corresponding private mode.  Most modes are resolved via `applyPrivateModeTable()`.
 * Modes with side effects are handled inline:
 *
 * | Mode | Name    | Side effect                                              |
 * |------|---------|----------------------------------------------------------|
 * | ?6   | DECOM   | Homes the cursor when enabled                            |
 * | ?25  | DECTCEM | Updates cursor visibility in State                       |
 * | ?47 / ?1047 / ?1049 | Alternate screen | Calls `setScreen()` |
 *
 * @par Sequences
 * @code
 *   ESC [ ? Pm h    — DECSET (set private mode)
 *   ESC [ ? Pm l    — DECRST (reset private mode)
 * @endcode
 *
 * @param params  CSI parameters containing the mode numbers.
 * @param enable  `true` to set the mode (h), `false` to reset it (l).
 *
 * @note READER THREAD only.
 *
 * @see applyPrivateModeTable()
 * @see handleMode()
 * @see setScreen()
 */
void Parser::handlePrivateMode (const CSI& params, bool enable) noexcept
{
    const auto scr { state.getRawValue<ActiveScreen> (ID::activeScreen) };

    for (uint8_t i { 0 }; i < params.count; ++i)
    {
        const auto modeValue { params.values.at (i) };

        if (applyPrivateModeTable (state, modeValue, enable))
        {
            continue;
        }

        if (modeValue == 6)
        {
            state.setMode (ID::originMode, enable);
            if (enable)
                cursorSetPosition (scr, 0, 0, grid.getCols(), grid.getVisibleRows());
        }
        else if (modeValue == 25)
        {
            state.setCursorVisible (scr, enable);
        }
        else if (modeValue == 47 or modeValue == 1047 or modeValue == 1049)
        {
            setScreen (enable);
        }
        else if (modeValue == 2026)
        {
            state.setSyncOutput (enable);

            if (enable)
                state.requestSyncResize();
        }
    }
}

// ============================================================================
// VT Handler: Mode (SM / RM)
// ============================================================================

/**
 * @brief Handles `CSI Pm h` / `CSI Pm l` — ANSI Mode Set/Reset (SM / RM).
 *
 * Iterates over all parameters in `params` and enables or disables the
 * corresponding ANSI standard mode.
 *
 * @par Supported modes
 * | Mode | Name | Effect                                                    |
 * |------|------|-----------------------------------------------------------|
 * | 4    | IRM  | Insert mode — characters shift right on input             |
 *
 * @par Sequences
 * @code
 *   ESC [ Pm h    — SM (Set Mode)
 *   ESC [ Pm l    — RM (Reset Mode)
 * @endcode
 *
 * @param params  CSI parameters containing the mode numbers.
 * @param enable  `true` to set the mode (h), `false` to reset it (l).
 *
 * @note READER THREAD only.
 *
 * @see handlePrivateMode()
 */
void Parser::handleMode (const CSI& params, bool enable) noexcept
{
    for (uint8_t i { 0 }; i < params.count; ++i)
    {
        const auto modeValue { params.values.at (i) };

        switch (modeValue)
        {
            case 4:
                state.setMode (ID::insertMode, enable);
                break;

            default:
                break;
        }
    }
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Terminal
