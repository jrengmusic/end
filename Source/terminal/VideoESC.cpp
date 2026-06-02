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
 * @see Buffer<Row>  — flat cell storage with per-row FAM metadata
 */

#include "Video.h"

namespace terminal
{
/*____________________________________________________________________________*/

// ============================================================================
// Cursor save / restore (DECSC / DECRC)
// ============================================================================

void Video::saveCursor (int scr) noexcept
{
    auto& sc { savedCursor.at (static_cast<size_t> (scr)) };
    sc.row         = cursorRow;
    sc.col         = cursorCol;
    sc.fg          = penFg;
    sc.bg          = penBg;
    sc.flags       = penFlags;
    sc.wrapPending = wrapPending;
    sc.originMode  = originMode;
    sc.lineDrawing = useLineDrawing;
}

void Video::restoreCursor (int scr) noexcept
{
    const auto& sc { savedCursor.at (static_cast<size_t> (scr)) };
    cursorRow      = sc.row;
    cursorCol      = sc.col;
    penFg          = sc.fg;
    penBg          = sc.bg;
    penFlags       = sc.flags;
    penStyleDirty  = true;
    wrapPending    = sc.wrapPending;
    originMode     = sc.originMode;
    useLineDrawing = sc.lineDrawing;
}

// ============================================================================
// VT Handler: ESC dispatch
// ============================================================================

void Video::escDispatchNoIntermediate (int scr, uint8_t finalByte) noexcept
{
    switch (finalByte)
    {
        case 'D':
        {
            // IND — Index: line feed without CR
            const cell scrollBot { activeScrollBottom() };
            const int  cRow      { cursorRow.value };
            const int  sTop      { scrollTop.value };

            if (cRow == scrollBot.value)
            {
                scrollUpAndFill (sTop, scrollBot.value);
            }

            cursorGoToNextLine (scrollBot, visibleRows);

            break;
        }

        case 'E':
        {
            // NEL — Next Line: CR + IND
            cursorCol   = 0_cell;
            wrapPending = false;

            const cell scrollBot { activeScrollBottom() };
            const int  cRow      { cursorRow.value };
            const int  sTop      { scrollTop.value };

            if (cRow == scrollBot.value)
            {
                scrollUpAndFill (sTop, scrollBot.value);
            }

            cursorGoToNextLine (scrollBot, visibleRows);

            break;
        }

        case 'H':
            setTabStop();
            break;

        case 'M':
        {
            // RI — Reverse Index: scroll down if at top of scroll region
            const int  cRow      { cursorRow.value };
            const int  sTopVal   { scrollTop.value };
            const cell scrollBot { activeScrollBottom() };

            if (cRow == sTopVal)
            {
                scrollDownAndFill (sTopVal, scrollBot.value);
            }
            else if (cRow > 0)
            {
                cursorRow   = cell (cRow - 1);
                wrapPending = false;
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

void Video::escDispatchDEC (int scr, uint8_t finalByte) noexcept
{
    if (finalByte == '8')
    {
        const int nCols { cols.value };
        const int vRows { visibleRows.value };
        const jam::Char alignCell { jam::Char::make ('E', jam::Char::CONTENT_CODEPOINT,
                                                     jam::Char::NARROW, currentStyleId()) };

        for (int row { 0 }; row < vRows; ++row)
        {
            jam::Row* const rowPtr { blocks.at (static_cast<size_t> (scr)).getWritePointer (row, 0) };

            for (int col { 0 }; col < nCols; ++col)
                rowPtr->cells[col] = alignCell;

            rowPtr->usedCols = static_cast<uint16_t> (nCols);
            rowPtr->flags    = 0;
        }

        cursorSetPosition (0_cell, 0_cell, cell (nCols), cell (vRows));
    }
}

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
} // namespace terminal
