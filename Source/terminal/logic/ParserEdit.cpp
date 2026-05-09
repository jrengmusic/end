/**
 * @file ParserEdit.cpp
 * @brief Terminal edit operations for the VT parser: erase, insert, delete, scroll.
 *
 * This file implements the screen-editing subsystem of the Parser class.  It
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
 * @see Grid      — ring-buffer cell storage with dirty tracking
 * @see State     — terminal parameter store supplying cursor position and scroll region
 * @see Parser.h  — class declaration and full method documentation
 */

#include "Parser.h"
#include "Grid.h"
#include <cstring>

namespace Terminal
{ /*____________________________________________________________________________*/

// ============================================================================
// VT Handler: Erase in Display
// ============================================================================

/**
 * @brief Handles `CSI Ps J` — Erase in Display (ED).
 *
 * Clears cells directly in Grid.  For modes 2 and 3, `State::queueImageErase()`
 * is called so the renderer evicts all image placements.  Mode 3 additionally
 * calls `state.setSnapshotDirty()`.
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
 * @see State::queueImageErase()
 */
void Parser::eraseInDisplay (int mode) noexcept
{
    const auto scr { state.getRawValue<ActiveScreen> (ID::activeScreen) };
    const int cols           { state.getRawValue<int> (ID::cols) };
    const int visibleRows    { state.getRawValue<int> (ID::visibleRows) };
    const int scrollbackUsed { state.getRawValue<int> (ID::scrollbackUsed) };
    const int absRowTop      { scrollbackUsed };
    const int absRowLast     { scrollbackUsed + visibleRows - 1 };
    const int cursorRow      { state.getRawValue<int> (state.screenKey (scr, ID::cursorRow)) };
    const int cursorCol      { state.getRawValue<int> (state.screenKey (scr, ID::cursorCol)) };
    const jam::Cell fill { jam::Cell::erase (stamp.bg) };
    const bool hasFill { stamp.bg.getAlpha() > 0 };

    switch (mode)
    {
        case 0:
        {
            // Cursor to end of screen
            // Clear rest of cursor row
            if (hasFill)
            {
                jam::Cell* row { grid.getWritePointer (cursorRow) };

                for (int c { cursorCol }; c < cols; ++c)
                    row[c] = fill;
            }
            else
            {
                grid.clear (cursorRow, cursorCol, cols - cursorCol);
            }

            // Clear rows below cursor
            for (int r { cursorRow + 1 }; r < visibleRows; ++r)
            {
                if (hasFill)
                {
                    jam::Cell* row { grid.getWritePointer (r) };

                    for (int c { 0 }; c < cols; ++c)
                        row[c] = fill;
                }
                else
                {
                    grid.clear (r);
                }
            }

            break;
        }

        case 1:
        {
            // Start of screen to cursor
            for (int r { 0 }; r < cursorRow; ++r)
            {
                if (hasFill)
                {
                    jam::Cell* row { grid.getWritePointer (r) };

                    for (int c { 0 }; c < cols; ++c)
                        row[c] = fill;
                }
                else
                {
                    grid.clear (r);
                }
            }

            // Clear cursor row up to and including cursor
            if (hasFill)
            {
                jam::Cell* row { grid.getWritePointer (cursorRow) };

                for (int c { 0 }; c <= cursorCol; ++c)
                    row[c] = fill;
            }
            else
            {
                grid.clear (cursorRow, 0, cursorCol + 1);
            }

            break;
        }

        case 2:
        {
            // Entire screen
            if (hasFill)
            {
                for (int r { 0 }; r < visibleRows; ++r)
                {
                    jam::Cell* row { grid.getWritePointer (r) };

                    for (int c { 0 }; c < cols; ++c)
                        row[c] = fill;
                }
            }
            else
            {
                grid.clear();
            }

            state.queueImageErase (absRowTop, 0, absRowLast, cols - 1);
            break;
        }

        case 3:
        {
            // Clear scrollback — set State flag for Display to handle
            state.setSnapshotDirty();
            state.queueImageErase (0, 0, absRowLast, cols - 1);
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
void Parser::eraseInLine (int mode) noexcept
{
    const auto scr { state.getRawValue<ActiveScreen> (ID::activeScreen) };
    const int cols { state.getRawValue<int> (ID::cols) };
    const int cursorRow { state.getRawValue<int> (state.screenKey (scr, ID::cursorRow)) };
    const int cursorCol { state.getRawValue<int> (state.screenKey (scr, ID::cursorCol)) };
    const jam::Cell fill { jam::Cell::erase (stamp.bg) };
    const bool hasFill { stamp.bg.getAlpha() > 0 };

    switch (mode)
    {
        case 0:
        {
            if (hasFill)
            {
                jam::Cell* row { grid.getWritePointer (cursorRow) };

                for (int c { cursorCol }; c < cols; ++c)
                    row[c] = fill;
            }
            else
            {
                grid.clear (cursorRow, cursorCol, cols - cursorCol);
            }

            break;
        }

        case 1:
        {
            if (hasFill)
            {
                jam::Cell* row { grid.getWritePointer (cursorRow) };

                for (int c { 0 }; c <= cursorCol; ++c)
                    row[c] = fill;
            }
            else
            {
                grid.clear (cursorRow, 0, cursorCol + 1);
            }

            break;
        }

        case 2:
        {
            if (hasFill)
            {
                jam::Cell* row { grid.getWritePointer (cursorRow) };

                for (int c { 0 }; c < cols; ++c)
                    row[c] = fill;
            }
            else
            {
                grid.clear (cursorRow);
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
void Parser::shiftLinesDown (int count) noexcept { shiftLines (count, false); }

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
void Parser::shiftLinesUp (int count) noexcept { shiftLines (count, true); }

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
void Parser::shiftLines (int count, bool up) noexcept
{
    const auto scr { state.getRawValue<ActiveScreen> (ID::activeScreen) };
    const int bottom { activeScrollBottom() };
    const int cursorRow { state.getRawValue<int> (state.screenKey (scr, ID::cursorRow)) };
    const int scrollTop { state.getRawValue<int> (state.screenKey (scr, ID::scrollTop)) };

    if (cursorRow >= scrollTop and cursorRow <= bottom)
    {
        const int clampedCount { juce::jmin (count, bottom - cursorRow + 1) };

        if (up)
        {
            grid.scrollUp (cursorRow, bottom, clampedCount);
        }
        else
        {
            grid.scrollDown (cursorRow, bottom, clampedCount);
        }

        if (stamp.bg.getAlpha() > 0)
        {
            const int cols { state.getRawValue<int> (ID::cols) };
            const jam::Cell fill { jam::Cell::erase (stamp.bg) };

            if (up)
            {
                for (int r { bottom - clampedCount + 1 }; r <= bottom; ++r)
                {
                    jam::Cell* row { grid.getWritePointer (r) };

                    for (int c { 0 }; c < cols; ++c)
                        row[c] = fill;
                }
            }
            else
            {
                for (int r { cursorRow }; r < cursorRow + clampedCount; ++r)
                {
                    jam::Cell* row { grid.getWritePointer (r) };

                    for (int c { 0 }; c < cols; ++c)
                        row[c] = fill;
                }
            }
        }

        state.setCursorCol (scr, 0);
        state.setWrapPending (scr, false);
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
void Parser::shiftCellsRight (int count) noexcept
{
    const auto scr { state.getRawValue<ActiveScreen> (ID::activeScreen) };
    const int cols { state.getRawValue<int> (ID::cols) };
    const int cursorRow { state.getRawValue<int> (state.screenKey (scr, ID::cursorRow)) };
    const int cursorCol { state.getRawValue<int> (state.screenKey (scr, ID::cursorCol)) };
    const int charsToInsert { juce::jmin (count, cols - cursorCol) };

    if (charsToInsert > 0 and cursorCol < cols)
    {
        jam::Cell* row { grid.getWritePointer (cursorRow) };

        std::memmove (row + cursorCol + charsToInsert,
                      row + cursorCol,
                      static_cast<size_t> (cols - cursorCol - charsToInsert) * sizeof (jam::Cell));

        const jam::Cell fill { jam::Cell::erase (stamp.bg) };

        for (int c { cursorCol }; c < cursorCol + charsToInsert; ++c)
            row[c] = fill;
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
void Parser::removeCells (int count) noexcept
{
    const auto scr { state.getRawValue<ActiveScreen> (ID::activeScreen) };
    const int cols { state.getRawValue<int> (ID::cols) };
    const int cursorRow { state.getRawValue<int> (state.screenKey (scr, ID::cursorRow)) };
    const int cursorCol { state.getRawValue<int> (state.screenKey (scr, ID::cursorCol)) };
    const int charsToDelete { juce::jmin (count, cols - cursorCol) };

    if (charsToDelete > 0 and cursorCol < cols)
    {
        jam::Cell* row { grid.getWritePointer (cursorRow) };

        std::memmove (row + cursorCol,
                      row + cursorCol + charsToDelete,
                      static_cast<size_t> (cols - cursorCol - charsToDelete) * sizeof (jam::Cell));

        const jam::Cell fill { jam::Cell::erase (stamp.bg) };

        for (int c { cols - charsToDelete }; c < cols; ++c)
            row[c] = fill;
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
void Parser::eraseCells (int count) noexcept
{
    const auto scr { state.getRawValue<ActiveScreen> (ID::activeScreen) };
    const int cols { state.getRawValue<int> (ID::cols) };
    const int cursorRow { state.getRawValue<int> (state.screenKey (scr, ID::cursorRow)) };
    const int cursorCol { state.getRawValue<int> (state.screenKey (scr, ID::cursorCol)) };
    const int clampedCount { juce::jmin (count, cols - cursorCol) };

    if (clampedCount > 0)
    {
        const jam::Cell fill { jam::Cell::erase (stamp.bg) };
        jam::Cell* row { grid.getWritePointer (cursorRow) };

        for (int c { cursorCol }; c < cursorCol + clampedCount; ++c)
            row[c] = fill;
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
void Parser::repeatCharacter (int count) noexcept
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
 * Calls `grid.setScreen()` to swap the active buffer.  No-op guard is applied:
 * if the target screen is already active, the function returns immediately
 * without any state mutation.
 *
 * @param shouldUseAlternate  `true` to activate the alternate screen buffer,
 *                            `false` to return to the normal screen buffer.
 *
 * @note READER THREAD only.
 */
void Parser::setScreen (bool shouldUseAlternate) noexcept
{
    if (const auto target { shouldUseAlternate ? alternate : normal };
        target != state.getRawValue<ActiveScreen> (ID::activeScreen))
    {
        state.setScreen (target);
        const auto scr { state.getRawValue<ActiveScreen> (ID::activeScreen) };
        cursorResetScrollRegion (scr);
        calc();

        grid.setScreen (shouldUseAlternate);

        if (target == alternate)
        {
            cursorClamp (scr, state.getRawValue<int> (ID::cols), state.getRawValue<int> (ID::visibleRows));
            activeLinkId = 0;
        }
    }
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Terminal
