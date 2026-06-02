/**
 * @file VideoEdit.cpp
 * @brief Terminal edit operations for the VT video processor: erase, insert, delete, scroll.
 *
 * This file implements the screen-editing subsystem of the Video class.  It
 * covers all operations that modify the content of the Grid without moving the
 * cursor to a new position: erasing regions, inserting and deleting lines,
 * inserting and deleting characters within a line, and switching between the
 * normal and alternate screen buffers.
 *
 * All operations write directly to the Grid cell buffer on the reader thread.
 *
 * ## Operations implemented
 *
 * | VT sequence     | CSI final | Handler              | Description                    |
 * |-----------------|-----------|----------------------|--------------------------------|
 * | ED  (Erase Display) | J    | `eraseInDisplay()`   | Erase part or all of screen    |
 * | EL  (Erase Line)    | K    | `eraseInLine()`      | Erase part or all of line      |
 * | IL  (Insert Lines)  | L    | `shiftLinesDown()`   | Insert blank lines at cursor   |
 * | DL  (Delete Lines)  | M    | `shiftLinesUp()`     | Delete lines at cursor         |
 * | ICH (Insert Chars)  | @    | `shiftCellsRight()`  | Insert blank cells at cursor   |
 * | DCH (Delete Chars)  | P    | `removeCells()`      | Delete cells at cursor         |
 * | ECH (Erase Chars)   | X    | `eraseCells()`       | Erase N cells without shifting |
 *
 * @note All functions in this file run on the READER THREAD only.
 *
 * @see jam::Block — ring-addressed cell storage (jam::Block<jam::Row>[2])
 * @see Video.h — class declaration and full method documentation
 */

#include "Video.h"

namespace terminal
{
/*____________________________________________________________________________*/

// ============================================================================
// VT Handler: Erase in Display
// ============================================================================

void Video::eraseInDisplay (int mode) noexcept
{
    const auto scr         { activeScreen };
    const int nCols        { cols.value };
    const int vRows        { visibleRows.value };
    const int cRow         { cursorRow.value };
    const int cCol         { cursorCol.value };
    const bool hasBgFill   { penBg.getAlpha() > 0 };
    const jam::Char fill   { jam::Char::erase (eraseStyleId()) };

    switch (mode)
    {
        case 0:
        {
            // Cursor to end of screen
            const int wp0 { 0 };

            // Clear rest of cursor row
            if (hasBgFill)
            {
                jam::Row* const cursorRowPtr { blocks.at (static_cast<size_t> (scr)).getWritePointer (cRow, wp0) };

                for (int c { cCol }; c < nCols; ++c)
                    cursorRowPtr->cells[c] = fill;
            }
            else
            {
                jam::Row* const cursorRowPtr { blocks.at (static_cast<size_t> (scr)).getWritePointer (cRow, wp0) };
                std::memset (&cursorRowPtr->cells[cCol], 0, static_cast<size_t> (nCols - cCol) * sizeof (jam::Char));
            }

            // Update cursor row metadata after partial or full erase.
            {
                jam::Row* const rowPtr { blocks.at (static_cast<size_t> (scr)).getWritePointer (cRow, wp0) };

                if (cCol == 0)
                {
                    rowPtr->usedCols = 0;
                    rowPtr->flags    = 0;
                }
                else if (static_cast<int> (rowPtr->usedCols) > cCol)
                {
                    rowPtr->usedCols = static_cast<uint16_t> (cCol);
                }
            }

            // Clear rows below cursor
            for (int r { cRow + 1 }; r < vRows; ++r)
            {
                if (hasBgFill)
                {
                    jam::Row* const rowPtr { blocks.at (static_cast<size_t> (scr)).getWritePointer (r, wp0) };

                    for (int c { 0 }; c < nCols; ++c)
                        rowPtr->cells[c] = fill;

                    rowPtr->usedCols = 0;
                    rowPtr->flags    = 0;
                }
                else
                {
                    blocks.at (static_cast<size_t> (scr)).clear (r, wp0);
                }
            }

            break;
        }

        case 1:
        {
            // Start of screen to cursor
            const int wp1 { 0 };

            for (int r { 0 }; r < cRow; ++r)
            {
                if (hasBgFill)
                {
                    jam::Row* const rowPtr { blocks.at (static_cast<size_t> (scr)).getWritePointer (r, wp1) };

                    for (int c { 0 }; c < nCols; ++c)
                        rowPtr->cells[c] = fill;

                    rowPtr->usedCols = 0;
                    rowPtr->flags    = 0;
                }
                else
                {
                    blocks.at (static_cast<size_t> (scr)).clear (r, wp1);
                }
            }

            // Clear cursor row up to and including cursor
            if (hasBgFill)
            {
                jam::Row* const cursorRowPtr { blocks.at (static_cast<size_t> (scr)).getWritePointer (cRow, wp1) };

                for (int c { 0 }; c <= cCol; ++c)
                    cursorRowPtr->cells[c] = fill;
            }
            else
            {
                jam::Row* const cursorRowPtr { blocks.at (static_cast<size_t> (scr)).getWritePointer (cRow, wp1) };
                std::memset (&cursorRowPtr->cells[0], 0, static_cast<size_t> (cCol + 1) * sizeof (jam::Char));
            }

            break;
        }

        case 2:
        {
            // Entire screen
            const int wp2 { 0 };

            if (hasBgFill)
            {
                for (int r { 0 }; r < vRows; ++r)
                {
                    jam::Row* const rowPtr { blocks.at (static_cast<size_t> (scr)).getWritePointer (r, wp2) };

                    for (int c { 0 }; c < nCols; ++c)
                        rowPtr->cells[c] = fill;

                    rowPtr->usedCols = 0;
                    rowPtr->flags    = 0;
                }
            }
            else
            {
                const int numRows { blocks.at (static_cast<size_t> (scr)).getNumRows() };

                for (int r { 0 }; r < numRows; ++r)
                    blocks.at (static_cast<size_t> (scr)).clear (r, wp2);
            }

            // Clear scrollback history
            if (events.contains (id::clearBuffer))
                events.get (id::clearBuffer, int (scr));

            break;
        }

        case 3:
        {
            // Clear viewport + scrollback history
            const int wp3 { 0 };

            if (hasBgFill)
            {
                for (int r { 0 }; r < vRows; ++r)
                {
                    jam::Row* const rowPtr { blocks.at (static_cast<size_t> (scr)).getWritePointer (r, wp3) };

                    for (int c { 0 }; c < nCols; ++c)
                        rowPtr->cells[c] = fill;

                    rowPtr->usedCols = 0;
                    rowPtr->flags    = 0;
                }
            }
            else
            {
                const int numRows { blocks.at (static_cast<size_t> (scr)).getNumRows() };

                for (int r { 0 }; r < numRows; ++r)
                    blocks.at (static_cast<size_t> (scr)).clear (r, wp3);
            }

            if (events.contains (id::clearBuffer))
                events.get (id::clearBuffer, int (scr));

            break;
        }

        default:
            break;
    }
}

// ============================================================================
// VT Handler: Erase in Line
// ============================================================================

void Video::eraseInLine (int mode) noexcept
{
    const auto scr       { activeScreen };
    const int nCols      { cols.value };
    const int cRow       { cursorRow.value };
    const int cCol       { cursorCol.value };
    const bool hasBgFill { penBg.getAlpha() > 0 };
    const jam::Char fill { jam::Char::erase (eraseStyleId()) };

    const int wpEl { 0 };

    switch (mode)
    {
        case 0:
        {
            if (hasBgFill)
            {
                jam::Row* const rowPtr { blocks.at (static_cast<size_t> (scr)).getWritePointer (cRow, wpEl) };

                for (int c { cCol }; c < nCols; ++c)
                    rowPtr->cells[c] = fill;
            }
            else
            {
                jam::Row* const rowPtr { blocks.at (static_cast<size_t> (scr)).getWritePointer (cRow, wpEl) };
                std::memset (&rowPtr->cells[cCol], 0, static_cast<size_t> (nCols - cCol) * sizeof (jam::Char));
            }

            break;
        }

        case 1:
        {
            if (hasBgFill)
            {
                jam::Row* const rowPtr { blocks.at (static_cast<size_t> (scr)).getWritePointer (cRow, wpEl) };

                for (int c { 0 }; c <= cCol; ++c)
                    rowPtr->cells[c] = fill;
            }
            else
            {
                jam::Row* const rowPtr { blocks.at (static_cast<size_t> (scr)).getWritePointer (cRow, wpEl) };
                std::memset (&rowPtr->cells[0], 0, static_cast<size_t> (cCol + 1) * sizeof (jam::Char));
            }

            break;
        }

        case 2:
        {
            if (hasBgFill)
            {
                jam::Row* const rowPtr { blocks.at (static_cast<size_t> (scr)).getWritePointer (cRow, wpEl) };

                for (int c { 0 }; c < nCols; ++c)
                    rowPtr->cells[c] = fill;

                rowPtr->usedCols = 0;
                rowPtr->flags    = 0;
            }
            else
            {
                blocks.at (static_cast<size_t> (scr)).clear (cRow, wpEl);
            }

            break;
        }

        default:
            break;
    }
}

// ============================================================================
// VT Handler: Insert / Delete Lines
// ============================================================================

void Video::shiftLinesDown (int count) noexcept { shiftLines (count, false); }

void Video::shiftLinesUp (int count) noexcept { shiftLines (count, true); }

void Video::shiftLines (int count, bool up) noexcept
{
    const auto scr { activeScreen };
    const int bottom { activeScrollBottom().value };
    const int cRow   { cursorRow.value };
    const int sTop   { scrollTop.value };

    if (cRow >= sTop and cRow <= bottom)
    {
        const int clampedCount { juce::jmin (count, bottom - cRow + 1) };

        if (up)
        {
            for (int n { 0 }; n < clampedCount; ++n)
            {
                for (int r { cRow }; r < bottom; ++r)
                    blocks.at (static_cast<size_t> (scr)).copyRow (r, r + 1, 0);

                blocks.at (static_cast<size_t> (scr)).clear (bottom, 0);
            }
        }
        else
        {
            for (int n { 0 }; n < clampedCount; ++n)
            {
                for (int r { bottom }; r > cRow; --r)
                    blocks.at (static_cast<size_t> (scr)).copyRow (r, r - 1, 0);

                blocks.at (static_cast<size_t> (scr)).clear (cRow, 0);
            }
        }

        if (penBg.getAlpha() > 0)
        {
            const int nCols { cols.value };
            const jam::Char fill { jam::Char::erase (eraseStyleId()) };

            if (up)
            {
                for (int r { bottom - clampedCount + 1 }; r <= bottom; ++r)
                {
                    jam::Row* const rowPtr { blocks.at (static_cast<size_t> (scr)).getWritePointer (r, 0) };

                    for (int c { 0 }; c < nCols; ++c)
                        rowPtr->cells[c] = fill;

                    rowPtr->usedCols = 0;
                    rowPtr->flags    = 0;
                }
            }
            else
            {
                for (int r { cRow }; r < cRow + clampedCount; ++r)
                {
                    jam::Row* const rowPtr { blocks.at (static_cast<size_t> (scr)).getWritePointer (r, 0) };

                    for (int c { 0 }; c < nCols; ++c)
                        rowPtr->cells[c] = fill;

                    rowPtr->usedCols = 0;
                    rowPtr->flags    = 0;
                }
            }
        }

        cursorCol   = 0_cell;
        wrapPending = false;
    }
}

// ============================================================================
// VT Handler: Insert / Delete / Erase Characters
// ============================================================================

void Video::shiftCellsRight (int count) noexcept
{
    const int nCols { cols.value };
    const int cRow  { cursorRow.value };
    const int cCol  { cursorCol.value };
    const int charsToInsert { juce::jmin (count, nCols - cCol) };

    if (charsToInsert > 0 and cCol < nCols)
    {
        jam::Row* const rowPtr { blocks.at (static_cast<size_t> (activeScreen)).getWritePointer (cRow, 0) };

        std::memmove (&rowPtr->cells[cCol + charsToInsert],
                      &rowPtr->cells[cCol],
                      static_cast<size_t> (nCols - cCol - charsToInsert) * sizeof (jam::Char));

        const jam::Char fill { jam::Char::erase (eraseStyleId()) };

        for (int c { cCol }; c < cCol + charsToInsert; ++c)
            rowPtr->cells[c] = fill;
    }
}

void Video::removeCells (int count) noexcept
{
    const int nCols { cols.value };
    const int cRow  { cursorRow.value };
    const int cCol  { cursorCol.value };
    const int charsToDelete { juce::jmin (count, nCols - cCol) };

    if (charsToDelete > 0 and cCol < nCols)
    {
        jam::Row* const rowPtr { blocks.at (static_cast<size_t> (activeScreen)).getWritePointer (cRow, 0) };

        std::memmove (&rowPtr->cells[cCol],
                      &rowPtr->cells[cCol + charsToDelete],
                      static_cast<size_t> (nCols - cCol - charsToDelete) * sizeof (jam::Char));

        const jam::Char fill { jam::Char::erase (eraseStyleId()) };

        for (int c { nCols - charsToDelete }; c < nCols; ++c)
            rowPtr->cells[c] = fill;
    }
}

void Video::eraseCells (int count) noexcept
{
    const int nCols { cols.value };
    const int cRow  { cursorRow.value };
    const int cCol  { cursorCol.value };
    const int clampedCount { juce::jmin (count, nCols - cCol) };

    if (clampedCount > 0)
    {
        const jam::Char fill { jam::Char::erase (eraseStyleId()) };
        jam::Row* const rowPtr { blocks.at (static_cast<size_t> (activeScreen)).getWritePointer (cRow, 0) };

        for (int c { cCol }; c < cCol + clampedCount; ++c)
            rowPtr->cells[c] = fill;
    }
}

// ============================================================================
// CSI Handler — REP
// ============================================================================

void Video::repeatCharacter (int count) noexcept
{
    if (lastGraphicChar != 0 and count > 0)
    {
        for (int i { 0 }; i < count; ++i)
        {
            print (lastGraphicChar);
        }
    }
}

// ============================================================================
// VT Handler: Alternate Screen
// ============================================================================

void Video::setScreen (bool shouldUseAlternate) noexcept
{
    const int target { shouldUseAlternate ? Map::Screen::alternate : Map::Screen::normal };

    if (target != activeScreen)
    {
        // Cursor is already in State from flush — update activeScreen directly.
        events.get (id::activeScreen, int (target));

        activeScreen = target;
        calc();

        if (target == Map::Screen::alternate)
        {
            cursorClamp (cols, visibleRows);
            activeLinkId = 0;
        }
    }
}

/**______________________________END OF NAMESPACE______________________________*/
} // namespace terminal
