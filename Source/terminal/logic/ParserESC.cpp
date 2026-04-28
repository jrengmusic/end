/**
 * @file ParserESC.cpp
 * @brief ESC sequence dispatch — two-byte and short ESC sequences.
 *
 * This translation unit implements `Parser::escDispatch()` and all ESC-level
 * handler functions.  OSC dispatch lives in `ParserOSC.cpp`; DCS and APC
 * passthrough live in `ParserDCS.cpp`.
 *
 * @par ESC sequence structure
 * An ESC sequence has the form:
 * @code
 *   ESC <intermediates> <final>
 * @endcode
 * where `<intermediates>` is zero or more bytes in 0x20–0x2F and `<final>`
 * is a single byte in 0x30–0x7E.  `escDispatch()` routes based on the
 * intermediate count and value:
 *
 * | Intermediates | Handler                      | Examples                   |
 * |---------------|------------------------------|----------------------------|
 * | None          | `escDispatchNoIntermediate()` | ESC D, ESC M, ESC c, ESC H |
 * | `(`           | `escDispatchCharset()`        | ESC ( 0, ESC ( B           |
 * | `#`           | `escDispatchDEC()`            | ESC # 8 (DECALN)           |
 *
 * @par Thread model
 * All functions in this file run exclusively on the **READER THREAD**.
 *
 * @see Parser.h      — class declaration and full API documentation
 * @see ParserOSC.cpp — OSC sequence dispatch
 * @see ParserDCS.cpp — DCS and APC passthrough
 * @see ParserVT.cpp  — ground-state print and execute handlers
 * @see ParserCSI.cpp — CSI sequence dispatch
 * @see State         — atomic terminal parameter store
 * @see Grid          — screen buffer written by DECALN
 */

#include "Parser.h"
#include "Grid.h"

namespace Terminal
{ /*____________________________________________________________________________*/

// ============================================================================
// Cursor save / restore (DECSC / DECRC)
// ============================================================================

void Parser::saveCursor (ActiveScreen scr) noexcept
{
    auto& sc { savedCursor.at (static_cast<size_t> (scr)) };
    sc.row         = state.getRawValue<int>  (state.screenKey (scr, ID::cursorRow));
    sc.col         = state.getRawValue<int>  (state.screenKey (scr, ID::cursorCol));
    sc.pen         = pen;
    sc.wrapPending = state.getRawValue<bool> (state.screenKey (scr, ID::wrapPending));
    sc.originMode  = state.getRawValue<bool> (state.modeKey (ID::originMode));
    sc.lineDrawing = useLineDrawing;
}

void Parser::restoreCursor (ActiveScreen scr) noexcept
{
    const auto& sc { savedCursor.at (static_cast<size_t> (scr)) };
    state.setCursorRow   (scr, sc.row);
    state.setCursorCol   (scr, sc.col);
    pen            = sc.pen;
    stamp          = sc.pen;
    state.setWrapPending (scr, sc.wrapPending);
    state.setMode        (ID::originMode, sc.originMode);
    useLineDrawing = sc.lineDrawing;
}

// ============================================================================
// VT Handler: ESC dispatch
// ============================================================================

/**
 * @brief Handles ESC sequences with no intermediate bytes.
 *
 * Covers the most common two-byte ESC sequences.  The final byte selects the
 * action:
 *
 * | Final | Name  | Action                                                       |
 * |-------|-------|--------------------------------------------------------------|
 * | 'D'   | IND   | Index — line feed without CR (scroll if at bottom)           |
 * | 'E'   | NEL   | Next Line — CR + line feed (scroll if at bottom)             |
 * | 'H'   | HTS   | Horizontal Tab Set — set tab stop at current column          |
 * | 'M'   | RI    | Reverse Index — move cursor up, scroll down if at top        |
 * | 'c'   | RIS   | Reset to Initial State — full terminal reset                 |
 *
 * @par Sequences
 * @code
 *   ESC D    — IND (Index)
 *   ESC E    — NEL (Next Line)
 *   ESC H    — HTS (Horizontal Tab Set)
 *   ESC M    — RI  (Reverse Index)
 *   ESC c    — RIS (Reset to Initial State)
 * @endcode
 *
 * @par IND (ESC D)
 * Moves the cursor down one line.  If the cursor is at the bottom of the
 * scrolling region, the region is scrolled up by one line instead.
 *
 * @par NEL (ESC E)
 * Moves the cursor to column 0 and then performs an IND.
 *
 * @par RI (ESC M)
 * Reverse Index — moves the cursor up one line.  If the cursor is at the top
 * of the scrolling region, the region is scrolled down by one line instead.
 *
 * @param scr        Target screen buffer (normal or alternate).
 * @param finalByte  The ESC final byte (0x30–0x7E).
 *
 * @note READER THREAD only.
 *
 * @see cursorGoToNextLine()
 * @see setTabStop()
 * @see reset()
 */
void Parser::escDispatchNoIntermediate (ActiveScreen scr, uint8_t finalByte) noexcept
{
    switch (finalByte)
    {
        case 'D':
        {
            if (not cursorGoToNextLine (scr, activeScrollBottom(), state.getRawValue<int> (ID::visibleRows)))
            {
                Cell fill {};
                fill.bg = stamp.bg;
                writer.scrollRegionUp (state.getRawValue<int> (state.screenKey (scr, ID::scrollTop)), activeScrollBottom(), 1, fill);
            }
            break;
        }

        case 'E':
            state.setCursorCol (scr, 0);
            state.setWrapPending (scr, false);
            if (not cursorGoToNextLine (scr, activeScrollBottom(), state.getRawValue<int> (ID::visibleRows)))
            {
                Cell fill {};
                fill.bg = stamp.bg;
                writer.scrollRegionUp (state.getRawValue<int> (state.screenKey (scr, ID::scrollTop)), activeScrollBottom(), 1, fill);
            }
            break;

        case 'H':
            setTabStop (scr);
            break;

        case 'M':
        {
            if (state.getRawValue<int> (state.screenKey (scr, ID::cursorRow)) == state.getRawValue<int> (state.screenKey (scr, ID::scrollTop)))
            {
                Cell fill {};
                fill.bg = stamp.bg;
                writer.scrollRegionDown (state.getRawValue<int> (state.screenKey (scr, ID::scrollTop)), activeScrollBottom(), 1, fill);
            }
            else if (state.getRawValue<int> (state.screenKey (scr, ID::cursorRow)) > 0)
            {
                state.setCursorRow (scr, state.getRawValue<int> (state.screenKey (scr, ID::cursorRow)) - 1);
                state.setWrapPending (scr, false);
            }
            break;
        }

        case 'c':
            reset();
            break;

        case '7':
            saveCursor (scr);
            break;

        case '8':
            restoreCursor (scr);
            break;

        case '=':
            state.setMode (ID::applicationKeypad, true);
            break;

        case '>':
            state.setMode (ID::applicationKeypad, false);
            break;

        default:
            break;
    }
}

/**
 * @brief Handles ESC sequences that designate a character set (G0–G3 slots).
 *
 * Intermediate bytes 0x28–0x2B select the target charset slot:
 * - `(` (0x28) → G0
 * - `)` (0x29) → G1
 * - `*` (0x2A) → G2
 * - `+` (0x2B) → G3
 *
 * The final byte selects the character set to load into that slot:
 * - `B` → ASCII (ISO 646 US)
 * - `0` → DEC Special Graphics (VT100 line-drawing characters)
 *
 * The G0 slot (`(`) sets `useLineDrawing` and `g0LineDrawing`.  The G1 slot
 * (`)`) sets `g1LineDrawing` but does not immediately affect `useLineDrawing`.
 *
 * @par Sequences
 * @code
 *   ESC ( 0    — Designate G0 = DEC Special Graphics (line drawing)
 *   ESC ( B    — Designate G0 = ASCII (default)
 *   ESC ) 0    — Designate G1 = DEC Special Graphics (ignored)
 * @endcode
 *
 * @param interByte  The intermediate byte (`(`, `)`, `*`, or `+`).
 * @param finalByte  The charset designator byte (`B`, `0`, etc.).
 *
 * @note READER THREAD only.
 *
 * @see useLineDrawing
 * @see translateCharset()
 */
void Parser::escDispatchCharset (uint8_t interByte, uint8_t finalByte) noexcept
{
    if (interByte == '(')
    {
        g0LineDrawing = (finalByte == '0');
        useLineDrawing = g0LineDrawing;
    }
    else if (interByte == ')')
    {
        g1LineDrawing = (finalByte == '0');
    }
}

/**
 * @brief Handles DEC-private ESC sequences with intermediate byte `#`.
 *
 * Currently implements only DECALN (DEC Screen Alignment Test, ESC # 8),
 * which fills the entire visible screen with the letter `E` and homes the
 * cursor.  DECALN is used by terminal conformance tests to verify that all
 * cells are addressable and that the screen geometry is correct.
 *
 * @par Sequence
 * @code
 *   ESC # 8    — DECALN (DEC Screen Alignment Test)
 * @endcode
 *
 * @par DECALN behaviour
 * Every cell in the visible grid is overwritten with `Cell { codepoint='E',
 * style=0, width=1, layout=0 }`.  The cursor is moved to row 0, column 0.
 *
 * @param scr        Target screen buffer (normal or alternate).
 * @param finalByte  The ESC final byte following `#`.
 *
 * @note READER THREAD only.
 *
 * @see Grid::activeWriteCell()
 * @see cursorSetPosition()
 */
void Parser::escDispatchDEC (ActiveScreen scr, uint8_t finalByte) noexcept
{
    if (finalByte == '8')
    {
        const int cols { state.getRawValue<int> (ID::cols) };
        const int visibleRows { state.getRawValue<int> (ID::visibleRows) };

        for (int row { 0 }; row < visibleRows; ++row)
        {
            for (int col { 0 }; col < cols; ++col)
            {
                Cell st {};
                st.codepoint = 'E';
                st.style = 0;
                st.width = 1;
                st.layout = 0;
                writer.activeWriteCell (row, col, st);
                writer.activeEraseGrapheme (row, col);
            }
        }

        cursorSetPosition (scr, 0, 0, cols, visibleRows);
    }
}

/**
 * @brief Dispatches a complete ESC sequence to the appropriate sub-handler.
 *
 * Called by `performAction()` when the `escDispatch` action fires.  Routes
 * based on the number and value of intermediate bytes:
 *
 * | Condition                          | Handler                      |
 * |------------------------------------|------------------------------|
 * | `interCount == 0`                  | `escDispatchNoIntermediate()` |
 * | `interCount == 1 && inter[0]=='('` | `escDispatchCharset()`       |
 * | `interCount == 1 && inter[0]=='#'` | `escDispatchDEC()`           |
 * | Other                              | silently ignored             |
 *
 * @par Sequence format
 * @code
 *   ESC <final>              — no intermediate
 *   ESC ( <final>            — G0 charset designation
 *   ESC # <final>            — DEC private sequence
 * @endcode
 *
 * @param inter              Pointer to the intermediate byte buffer.
 * @param interCount         Number of valid bytes in `inter`.
 * @param finalByte          The ESC final byte (0x30–0x7E).
 *
 * @note READER THREAD only.
 *
 * @see escDispatchNoIntermediate()
 * @see escDispatchCharset()
 * @see escDispatchDEC()
 */
void Parser::escDispatch (const uint8_t* inter, uint8_t interCount, uint8_t finalByte) noexcept
{
    const auto scr { state.getRawValue<ActiveScreen> (ID::activeScreen) };

    if (interCount == 0)
    {
        escDispatchNoIntermediate (scr, finalByte);
    }
    else if (interCount == 1 and (inter[0] == '(' or inter[0] == ')'))
    {
        escDispatchCharset (inter[0], finalByte);
    }
    else if (interCount == 1 and inter[0] == '#')
    {
        escDispatchDEC (scr, finalByte);
    }
}

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Terminal
