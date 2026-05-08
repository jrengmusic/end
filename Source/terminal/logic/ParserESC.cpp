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
 * @see Grid          — SPSC FIFO receiving Commands pushed by DECALN
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
            // IND — Index: line feed without CR
            const int scrollBot { activeScrollBottom() };
            const int visibleRows { state.getRawValue<int> (ID::visibleRows) };
            const int cursorRow { state.getRawValue<int> (state.screenKey (scr, ID::cursorRow)) };

            grid.push (Command { Command::Type::LineFeed, {}, stamp.bg, 0, cursorRow, 0 });

            cursorGoToNextLine (scr, scrollBot, visibleRows);

            break;
        }

        case 'E':
        {
            // NEL — Next Line: CR + IND
            state.setCursorCol (scr, 0);
            state.setWrapPending (scr, false);

            const int scrollBot { activeScrollBottom() };
            const int visibleRows { state.getRawValue<int> (ID::visibleRows) };
            const int cursorRow { state.getRawValue<int> (state.screenKey (scr, ID::cursorRow)) };

            grid.push (Command { Command::Type::LineFeed, {}, stamp.bg, 0, cursorRow, 0 });

            cursorGoToNextLine (scr, scrollBot, visibleRows);

            break;
        }

        case 'H':
            setTabStop (scr);
            break;

        case 'M':
        {
            // RI — Reverse Index: scroll down if at top of scroll region
            const int cursorRow { state.getRawValue<int> (state.screenKey (scr, ID::cursorRow)) };
            const int scrollTopVal { state.getRawValue<int> (state.screenKey (scr, ID::scrollTop)) };
            const int scrollBot { activeScrollBottom() };

            if (cursorRow == scrollTopVal)
            {
                // Reverse Index at scroll top: push InsertLines(1) at scrollTop
                grid.push (Command { Command::Type::InsertLines, {}, stamp.bg, 1, scrollTopVal, 0 });
            }
            else if (cursorRow > 0)
            {
                state.setCursorRow (scr, cursorRow - 1);
                state.setWrapPending (scr, false);
            }

            juce::ignoreUnused (scrollBot);

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
 * Each cell is pushed as an individual Command::Print.
 *
 * @param scr        Target screen buffer (normal or alternate).
 * @param finalByte  The ESC final byte following `#`.
 *
 * @note READER THREAD only.
 */
void Parser::escDispatchDEC (ActiveScreen scr, uint8_t finalByte) noexcept
{
    if (finalByte == '8')
    {
        const int cols { state.getRawValue<int> (ID::cols) };
        const int visibleRows { state.getRawValue<int> (ID::visibleRows) };

        jam::Cell st {};
        st.codepoint = 'E';
        st.style = 0;
        st.width = 1;
        st.layout = 0;

        for (int row { 0 }; row < visibleRows; ++row)
        {
            for (int col { 0 }; col < cols; ++col)
            {
                grid.push (Command { Command::Type::Print, st, {}, 0, row, col });
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
