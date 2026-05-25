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

/**
 * @brief Handles `CSI Ps J` — Erase in Display (ED).
 *
 * Clears cells directly in Grid.  Modes 2 and 3 also fire the `"clearBuffer"`
 * event, which Processor handles by resetting writeHead and scrollOffset to 0.
 *
 * @par Mode table
 *
 * | Mode | VT sequence | Effect                                              |
 * |------|-------------|-----------------------------------------------------|
 * | 0    | `CSI J`     | Erase from cursor to end of screen (inclusive)      |
 * | 1    | `CSI 1 J`   | Erase from start of screen to cursor (inclusive)    |
 * | 2    | `CSI 2 J`   | Erase entire visible screen                         |
 * | 3    | `CSI 3 J`   | Erase entire screen including scrollback (xterm)    |
 *
 * @param mode  Erase mode (0 = below, 1 = above, 2 = all, 3 = scrollback).
 *              Unknown modes are silently ignored.
 *
 * @note READER THREAD only.
 * @note Does not respect the scroll region — operates on the full visible screen.
 *
 * @see eraseInLine()
 * @see events
 */
void Video::eraseInDisplay (int mode) noexcept
{
    const auto scr         { activeScreen };
    const int nCols        { cols.value };
    const int vRows        { visibleRows.value };
    const int cRow         { cursorRow.value };
    const int cCol         { cursorCol.value };
    const bool hasBgFill   { penBg.getAlpha() > 0 };
    const jam::Cell fill   { jam::Cell::erase (eraseStyleId()) };

    switch (mode)
    {
        case 0:
        {
            // Cursor to end of screen
            const int wp0 { writePosition.at (static_cast<size_t> (scr)) };

            // Clear rest of cursor row
            if (hasBgFill)
            {
                jam::Row* const cursorRowPtr { blocks[scr].getWritePointer (cRow, wp0) };

                for (int c { cCol }; c < nCols; ++c)
                    cursorRowPtr->cells[c] = fill;
            }
            else
            {
                jam::Row* const cursorRowPtr { blocks[scr].getWritePointer (cRow, wp0) };
                std::memset (&cursorRowPtr->cells[cCol], 0, static_cast<size_t> (nCols - cCol) * sizeof (jam::Cell));
            }

            // Update cursor row metadata after partial or full erase.
            {
                jam::Row* const rowPtr { blocks[scr].getWritePointer (cRow, wp0) };

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
                    jam::Row* const rowPtr { blocks[scr].getWritePointer (r, wp0) };

                    for (int c { 0 }; c < nCols; ++c)
                        rowPtr->cells[c] = fill;

                    rowPtr->usedCols = 0;
                    rowPtr->flags    = 0;
                }
                else
                {
                    blocks[scr].clear (r, wp0);
                }
            }

            break;
        }

        case 1:
        {
            // Start of screen to cursor
            const int wp1 { writePosition.at (static_cast<size_t> (scr)) };

            for (int r { 0 }; r < cRow; ++r)
            {
                if (hasBgFill)
                {
                    jam::Row* const rowPtr { blocks[scr].getWritePointer (r, wp1) };

                    for (int c { 0 }; c < nCols; ++c)
                        rowPtr->cells[c] = fill;

                    rowPtr->usedCols = 0;
                    rowPtr->flags    = 0;
                }
                else
                {
                    blocks[scr].clear (r, wp1);
                }
            }

            // Clear cursor row up to and including cursor
            if (hasBgFill)
            {
                jam::Row* const cursorRowPtr { blocks[scr].getWritePointer (cRow, wp1) };

                for (int c { 0 }; c <= cCol; ++c)
                    cursorRowPtr->cells[c] = fill;
            }
            else
            {
                jam::Row* const cursorRowPtr { blocks[scr].getWritePointer (cRow, wp1) };
                std::memset (&cursorRowPtr->cells[0], 0, static_cast<size_t> (cCol + 1) * sizeof (jam::Cell));
            }

            break;
        }

        case 2:
        {
            // Entire screen
            const int wp2 { writePosition.at (static_cast<size_t> (scr)) };

            if (hasBgFill)
            {
                for (int r { 0 }; r < vRows; ++r)
                {
                    jam::Row* const rowPtr { blocks[scr].getWritePointer (r, wp2) };

                    for (int c { 0 }; c < nCols; ++c)
                        rowPtr->cells[c] = fill;

                    rowPtr->usedCols = 0;
                    rowPtr->flags    = 0;
                }
            }
            else
            {
                const int numRows { blocks[scr].getNumRows() };

                for (int r { 0 }; r < numRows; ++r)
                    blocks[scr].clear (r, wp2);
            }

            // Clear scrollback history
            if (events.contains (id::clearBuffer))
                events.get (id::clearBuffer, int (scr));

            break;
        }

        case 3:
        {
            // Clear viewport + scrollback history
            const int wp3 { writePosition.at (static_cast<size_t> (scr)) };

            if (hasBgFill)
            {
                for (int r { 0 }; r < vRows; ++r)
                {
                    jam::Row* const rowPtr { blocks[scr].getWritePointer (r, wp3) };

                    for (int c { 0 }; c < nCols; ++c)
                        rowPtr->cells[c] = fill;

                    rowPtr->usedCols = 0;
                    rowPtr->flags    = 0;
                }
            }
            else
            {
                const int numRows { blocks[scr].getNumRows() };

                for (int r { 0 }; r < numRows; ++r)
                    blocks[scr].clear (r, wp3);
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

/**
 * @brief Handles `CSI Ps K` — Erase in Line (EL).
 *
 * Clears cells directly in Grid on the cursor row.  EL is a cell-layer
 * operation — the image layer is not affected.
 *
 * @par Mode table
 *
 * | Mode | VT sequence | Effect                                              |
 * |------|-------------|-----------------------------------------------------|
 * | 0    | `CSI K`     | Erase from cursor to end of line (inclusive)        |
 * | 1    | `CSI 1 K`   | Erase from start of line to cursor (inclusive)      |
 * | 2    | `CSI 2 K`   | Erase entire line                                   |
 *
 * @param mode  Erase mode (0 = to right, 1 = to left, 2 = entire line).
 *              Unknown modes are silently ignored.
 *
 * @note READER THREAD only.
 * @note Does not respect the scroll region.
 *
 * @see eraseInDisplay()
 */
void Video::eraseInLine (int mode) noexcept
{
    const auto scr       { activeScreen };
    const int nCols      { cols.value };
    const int cRow       { cursorRow.value };
    const int cCol       { cursorCol.value };
    const bool hasBgFill { penBg.getAlpha() > 0 };
    const jam::Cell fill { jam::Cell::erase (eraseStyleId()) };

    const int wpEl { writePosition.at (static_cast<size_t> (scr)) };

    switch (mode)
    {
        case 0:
        {
            if (hasBgFill)
            {
                jam::Row* const rowPtr { blocks[scr].getWritePointer (cRow, wpEl) };

                for (int c { cCol }; c < nCols; ++c)
                    rowPtr->cells[c] = fill;
            }
            else
            {
                jam::Row* const rowPtr { blocks[scr].getWritePointer (cRow, wpEl) };
                std::memset (&rowPtr->cells[cCol], 0, static_cast<size_t> (nCols - cCol) * sizeof (jam::Cell));
            }

            break;
        }

        case 1:
        {
            if (hasBgFill)
            {
                jam::Row* const rowPtr { blocks[scr].getWritePointer (cRow, wpEl) };

                for (int c { 0 }; c <= cCol; ++c)
                    rowPtr->cells[c] = fill;
            }
            else
            {
                jam::Row* const rowPtr { blocks[scr].getWritePointer (cRow, wpEl) };
                std::memset (&rowPtr->cells[0], 0, static_cast<size_t> (cCol + 1) * sizeof (jam::Cell));
            }

            break;
        }

        case 2:
        {
            if (hasBgFill)
            {
                jam::Row* const rowPtr { blocks[scr].getWritePointer (cRow, wpEl) };

                for (int c { 0 }; c < nCols; ++c)
                    rowPtr->cells[c] = fill;

                rowPtr->usedCols = 0;
                rowPtr->flags    = 0;
            }
            else
            {
                blocks[scr].clear (cRow, wpEl);
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

/**
 * @brief Handles `CSI Pn L` — Insert Lines (IL).
 *
 * Inserts `count` blank lines at the cursor row by scrolling the region from
 * the cursor row to `scrollBottom` downward.  Lines that scroll off the bottom
 * of the scroll region are discarded.  The cursor column is reset to 0.
 *
 * @param count  Number of blank lines to insert (>= 1).
 *
 * @note READER THREAD only.
 *
 * @see shiftLines()
 * @see shiftLinesUp()
 */
void Video::shiftLinesDown (int count) noexcept { shiftLines (count, false); }

/**
 * @brief Handles `CSI Pn M` — Delete Lines (DL).
 *
 * Deletes `count` lines at the cursor row by scrolling the region from the
 * cursor row to `scrollBottom` upward.  Blank lines are inserted at the bottom
 * of the scroll region to fill the vacated space.  The cursor column is reset
 * to 0.
 *
 * @param count  Number of lines to delete (>= 1).
 *
 * @note READER THREAD only.
 *
 * @see shiftLines()
 * @see shiftLinesDown()
 */
void Video::shiftLinesUp (int count) noexcept { shiftLines (count, true); }

/**
 * @brief Shared implementation for Insert Lines (IL) and Delete Lines (DL).
 *
 * Validates that the cursor is within the active scroll region, then scrolls
 * rows directly in Grid.  After scrolling, the cursor column is reset to 0
 * and the wrap-pending flag is cleared.
 *
 * @par Scroll region guard
 * The operation is a no-op if:
 * - `cursorRow < scrollTop`    (cursor is above the scroll region)
 * - `cursorRow > scrollBottom` (cursor is below the scroll region)
 *
 * @param count  Number of lines to shift (>= 1).
 * @param up     `true`  → DeleteLines (DL: delete lines at cursor).
 *               `false` → InsertLines (IL: insert lines at cursor).
 *
 * @note READER THREAD only.
 *
 * @see shiftLinesDown()
 * @see shiftLinesUp()
 */
void Video::shiftLines (int count, bool up) noexcept
{
    const auto scr { activeScreen };
    const int bottom { activeScrollBottom().value };
    const int cRow   { cursorRow.value };
    const int sTop   { scrollTop.value };

    if (cRow >= sTop and cRow <= bottom)
    {
        const int clampedCount { juce::jmin (count, bottom - cRow + 1) };

        const int wp { writePosition.at (static_cast<size_t> (scr)) };

        if (up)
        {
            const bool isFullScreen { cRow == 0 and bottom == visibleRows.value - 1 };

            if (isFullScreen)
            {
                for (int n { 0 }; n < clampedCount; ++n)
                {
                    writePosition.at (static_cast<size_t> (scr)) =
                        (writePosition.at (static_cast<size_t> (scr)) + 1) % blocks[scr].getNumRows();
                    blocks[scr].clear (bottom, writePosition.at (static_cast<size_t> (scr)));
                }

                if (events.contains (id::scrollUp))
                    events.get (id::scrollUp, int (scr), int (clampedCount), writePosition.at (static_cast<size_t> (scr)));
            }
            else
            {
                for (int n { 0 }; n < clampedCount; ++n)
                {
                    for (int r { cRow }; r < bottom; ++r)
                        blocks[scr].copyRow (r, r + 1, wp);

                    blocks[scr].clear (bottom, wp);
                }
            }
        }
        else
        {
            const bool isFullScreen { cRow == 0 and bottom == visibleRows.value - 1 };

            if (isFullScreen)
            {
                for (int n { 0 }; n < clampedCount; ++n)
                {
                    const int ringRows { blocks[scr].getNumRows() };
                    writePosition.at (static_cast<size_t> (scr)) =
                        (writePosition.at (static_cast<size_t> (scr)) - 1 + ringRows) % ringRows;
                    blocks[scr].clear (0, writePosition.at (static_cast<size_t> (scr)));
                }
            }
            else
            {
                for (int n { 0 }; n < clampedCount; ++n)
                {
                    for (int r { bottom }; r > cRow; --r)
                        blocks[scr].copyRow (r, r - 1, wp);

                    blocks[scr].clear (cRow, wp);
                }
            }
        }

        const int wpFill { writePosition.at (static_cast<size_t> (scr)) };

        if (penBg.getAlpha() > 0)
        {
            const int nCols { cols.value };
            const jam::Cell fill { jam::Cell::erase (eraseStyleId()) };

            if (up)
            {
                for (int r { bottom - clampedCount + 1 }; r <= bottom; ++r)
                {
                    jam::Row* const rowPtr { blocks[scr].getWritePointer (r, wpFill) };

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
                    jam::Row* const rowPtr { blocks[scr].getWritePointer (r, wpFill) };

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

/**
 * @brief Handles `CSI Pn @` — Insert Characters (ICH).
 *
 * Inserts blank cells directly in Grid at the cursor column, shifting existing
 * cells to the right.  Cells that shift past the right margin are discarded.
 * The cursor position is not changed.
 *
 * @param count  Number of blank cells to insert (>= 1).
 *
 * @note READER THREAD only.
 *
 * @see removeCells()
 * @see eraseCells()
 */
void Video::shiftCellsRight (int count) noexcept
{
    const int nCols { cols.value };
    const int cRow  { cursorRow.value };
    const int cCol  { cursorCol.value };
    const int charsToInsert { juce::jmin (count, nCols - cCol) };

    if (charsToInsert > 0 and cCol < nCols)
    {
        jam::Row* const rowPtr { blocks[activeScreen].getWritePointer (cRow, writePosition.at (static_cast<size_t> (activeScreen))) };

        std::memmove (&rowPtr->cells[cCol + charsToInsert],
                      &rowPtr->cells[cCol],
                      static_cast<size_t> (nCols - cCol - charsToInsert) * sizeof (jam::Cell));

        const jam::Cell fill { jam::Cell::erase (eraseStyleId()) };

        for (int c { cCol }; c < cCol + charsToInsert; ++c)
            rowPtr->cells[c] = fill;
    }
}

/**
 * @brief Handles `CSI Pn P` — Delete Characters (DCH).
 *
 * Removes cells directly in Grid, shifting remaining cells to the left and
 * inserting blank cells at the right margin.  The cursor position is not
 * changed.
 *
 * @param count  Number of cells to delete (>= 1).
 *
 * @note READER THREAD only.
 *
 * @see shiftCellsRight()
 * @see eraseCells()
 */
void Video::removeCells (int count) noexcept
{
    const int nCols { cols.value };
    const int cRow  { cursorRow.value };
    const int cCol  { cursorCol.value };
    const int charsToDelete { juce::jmin (count, nCols - cCol) };

    if (charsToDelete > 0 and cCol < nCols)
    {
        jam::Row* const rowPtr { blocks[activeScreen].getWritePointer (cRow, writePosition.at (static_cast<size_t> (activeScreen))) };

        std::memmove (&rowPtr->cells[cCol],
                      &rowPtr->cells[cCol + charsToDelete],
                      static_cast<size_t> (nCols - cCol - charsToDelete) * sizeof (jam::Cell));

        const jam::Cell fill { jam::Cell::erase (eraseStyleId()) };

        for (int c { nCols - charsToDelete }; c < nCols; ++c)
            rowPtr->cells[c] = fill;
    }
}

/**
 * @brief Handles `CSI Pn X` — Erase Characters (ECH).
 *
 * Blanks cells directly in Grid without shifting surrounding content.  The
 * cursor position is not changed.  ECH is a cell-layer operation — the image
 * layer is not affected.
 *
 * @param count  Number of cells to erase (>= 1).
 *
 * @note READER THREAD only.
 *
 * @see removeCells()
 * @see shiftCellsRight()
 */
void Video::eraseCells (int count) noexcept
{
    const int nCols { cols.value };
    const int cRow  { cursorRow.value };
    const int cCol  { cursorCol.value };
    const int clampedCount { juce::jmin (count, nCols - cCol) };

    if (clampedCount > 0)
    {
        const jam::Cell fill { jam::Cell::erase (eraseStyleId()) };
        jam::Row* const rowPtr { blocks[activeScreen].getWritePointer (cRow, writePosition.at (static_cast<size_t> (activeScreen))) };

        for (int c { cCol }; c < cCol + clampedCount; ++c)
            rowPtr->cells[c] = fill;
    }
}

// ============================================================================
// CSI Handler — REP
// ============================================================================

/**
 * @brief Handles `CSI Ps b` — Repeat preceding graphic character (REP).
 *
 * Repeats the last graphic character printed Ps times at the current cursor
 * position, advancing the cursor.  Uses the existing `print()` path so that
 * all Grid writes, dirty marking, and wrap handling are consistent.
 *
 * @param count  Number of repetitions (default 1).
 *
 * @note READER THREAD only.
 */
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

/**
 * @brief Switches between the normal and alternate screen buffers.
 *
 * Updates `activeScreen` to the target.  No-op guard is applied:
 * if the target screen is already active, the function returns immediately
 * without any state mutation.
 *
 * @param shouldUseAlternate  `true` to activate the alternate screen buffer,
 *                            `false` to return to the normal screen buffer.
 *
 * @note READER THREAD only.
 */
void Video::setScreen (bool shouldUseAlternate) noexcept
{
    const int target { shouldUseAlternate ? Map::Screen::alternate : Map::Screen::normal };

    if (target != activeScreen)
    {
        // Update State activeScreen and signal Processor.
        // Cursor is already in State from VTPC flush — no screenSwitch mediation needed.
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
