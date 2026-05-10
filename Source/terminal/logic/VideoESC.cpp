/**
 * @file VideoESC.cpp
 * @brief ESC sequence dispatch — two-byte and short ESC sequences.
 *
 * This translation unit implements `Video::applyESC()` and all ESC-level
 * handler functions.  OSC dispatch lives in `VideoOSC.cpp`; DCS and APC
 * passthrough live in `VideoDCS.cpp`.
 *
 * @par ESC sequence structure
 * An ESC sequence has the form:
 * @code
 *   ESC <intermediates> <final>
 * @endcode
 * where `<intermediates>` is zero or more bytes in 0x20–0x2F and `<final>`
 * is a single byte in 0x30–0x7E.  `applyESC()` routes based on the
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
 * @see Video.h      — class declaration and full API documentation
 * @see VideoOSC.cpp — OSC sequence dispatch
 * @see VideoDCS.cpp — DCS and APC passthrough
 * @see VideoVT.cpp  — ground-state print and applyControlCode handlers
 * @see VideoCSI.cpp — CSI sequence dispatch
 * @see Grid         — ring-buffer cell storage
 */

#include "Video.h"
#include "Grid.h"

namespace Terminal
{ /*____________________________________________________________________________*/

// ============================================================================
// Cursor save / restore (DECSC / DECRC)
// ============================================================================

void Video::saveCursor (int scr) noexcept
{
    auto& sc { savedCursor.at (static_cast<size_t> (scr)) };
    sc.row         = cursorRow[scr];
    sc.col         = cursorCol[scr];
    sc.pen         = pen;
    sc.wrapPending = wrapPending[scr];
    sc.originMode  = originMode;
    sc.lineDrawing = useLineDrawing;
}

void Video::restoreCursor (int scr) noexcept
{
    const auto& sc { savedCursor.at (static_cast<size_t> (scr)) };
    cursorRow[scr]     = sc.row;
    cursorCol[scr]     = sc.col;
    pen                = sc.pen;
    stamp              = sc.pen;
    wrapPending[scr]   = sc.wrapPending;
    originMode         = sc.originMode;
    useLineDrawing     = sc.lineDrawing;
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
void Video::escDispatchNoIntermediate (int scr, uint8_t finalByte) noexcept
{
    switch (finalByte)
    {
        case 'D':
        {
            // IND — Index: line feed without CR
            const int scrollBot { activeScrollBottom() };
            const int vRows { visibleRows.load (std::memory_order_relaxed) };
            const int cRow  { cursorRow[scr] };
            const int sTop  { scrollTop[scr] };

            if (cRow == scrollBot)
            {
                scrollUpAndFill (sTop, scrollBot);
            }

            cursorGoToNextLine (scr, scrollBot, vRows);

            break;
        }

        case 'E':
        {
            // NEL — Next Line: CR + IND
            cursorCol[scr]   = 0;
            wrapPending[scr] = false;

            const int scrollBot { activeScrollBottom() };
            const int vRows { visibleRows.load (std::memory_order_relaxed) };
            const int cRow  { cursorRow[scr] };
            const int sTop  { scrollTop[scr] };

            if (cRow == scrollBot)
            {
                scrollUpAndFill (sTop, scrollBot);
            }

            cursorGoToNextLine (scr, scrollBot, vRows);

            break;
        }

        case 'H':
            setTabStop (scr);
            break;

        case 'M':
        {
            // RI — Reverse Index: scroll down if at top of scroll region
            const int cRow      { cursorRow[scr] };
            const int sTopVal   { scrollTop[scr] };
            const int scrollBot { activeScrollBottom() };

            if (cRow == sTopVal)
            {
                scrollDownAndFill (sTopVal, scrollBot);
            }
            else if (cRow > 0)
            {
                cursorRow[scr]   = cRow - 1;
                wrapPending[scr] = false;
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
            applicationKeypad = true;
            break;

        case '>':
            applicationKeypad = false;
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
void Video::escDispatchCharset (uint8_t interByte, uint8_t finalByte) noexcept
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
 * Each cell is written directly to Grid via getWritePointer.
 *
 * @param scr        Target screen buffer (normal or alternate).
 * @param finalByte  The ESC final byte following `#`.
 *
 * @note READER THREAD only.
 */
void Video::escDispatchDEC (int scr, uint8_t finalByte) noexcept
{
    if (finalByte == '8')
    {
        const int nCols { cols.load (std::memory_order_relaxed) };
        const int vRows { visibleRows.load (std::memory_order_relaxed) };

        jam::Cell st {};
        st.codepoint = 'E';
        st.style = 0;
        st.width = 1;
        st.layout = 0;

        for (int row { 0 }; row < vRows; ++row)
        {
            jam::Cell* rowPtr { grid.getWritePointer (row) };

            for (int col { 0 }; col < nCols; ++col)
                rowPtr[col] = st;
        }

        cursorSetPosition (scr, 0, 0, nCols, vRows);
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
void Video::applyESC (const uint8_t* inter, uint8_t interCount, uint8_t finalByte) noexcept
{
    const auto scr { activeScreen };

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
