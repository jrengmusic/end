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
 * ## Scroll region interaction
 *
 * Insert/delete line operations (`IL`/`DL`) respect the active scroll region.
 * If the cursor is outside the scroll region, the operation is a no-op.  When
 * inside the region, `shiftLines()` delegates to `Grid::scrollRegionUp()` or
 * `Grid::scrollRegionDown()`, which confine all movement to the region bounds.
 *
 * Erase operations (`ED`, `EL`, `ECH`) do not respect the scroll region — they
 * operate on absolute screen coordinates.
 *
 * @note All functions in this file run on the READER THREAD only.
 *
 * @see Grid      — screen buffer providing erase, scroll, and row-access primitives
 * @see State     — terminal parameter store supplying cursor position and scroll region
 * @see Parser.h  — class declaration and full method documentation
 */

#include "Parser.h"
#include "Grid.h"

namespace Terminal
{ /*____________________________________________________________________________*/

// ============================================================================
// VT Handler: Erase in Display
// ============================================================================

/**
 * @brief Handles `CSI Ps J` — Erase in Display (ED).
 *
 * Erases part or all of the visible screen.  The cursor position is not
 * changed.  Erased cells are filled with the default (blank) cell value.
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
 * @par Mode 0 — erase below
 * Erases from the cursor column to the end of the cursor row, then erases all
 * rows below the cursor row down to `visibleRows - 1`.
 * @code
 * // Cursor at (row, col):
 * grid.eraseCellRange (row, col, cols - 1);          // rest of cursor row
 * grid.eraseRowRange  (row + 1, visibleRows - 1);    // all rows below
 * @endcode
 *
 * @par Mode 1 — erase above
 * Erases all rows above the cursor row, then erases from column 0 to the
 * cursor column on the cursor row.
 * @code
 * grid.eraseRowRange  (0, row - 1);   // all rows above
 * grid.eraseCellRange (row, 0, col);  // start of cursor row
 * @endcode
 *
 * @par Modes 2 and 3 — erase all
 * Both modes erase the entire visible screen (`eraseRowRange (0, visibleRows - 1)`).
 * Mode 3 additionally clears the scrollback buffer in xterm; this implementation
 * treats modes 2 and 3 identically (visible screen only).
 *
 * @param mode  Erase mode (0 = below, 1 = above, 2 = all, 3 = scrollback).
 *              Unknown modes are silently ignored.
 *
 * @note READER THREAD only.
 * @note Does not respect the scroll region — operates on the full visible screen.
 *
 * @see eraseInLine()       — erases within a single row
 * @see Grid::eraseCellRange() — erases a range of cells within a row
 * @see Grid::eraseRowRange()  — erases a range of complete rows
 */
void Parser::eraseInDisplay (int mode) noexcept
{
    const auto scr { state.getScreen() };
    const int cols { grid.getCols() };
    const int visibleRows { grid.getVisibleRows() };

    Cell fill {};
    fill.bg = stamp.bg;

    switch (mode)
    {
        case 0:
            grid.eraseCellRange (state.getCursorRow (scr), state.getCursorCol (scr), cols - 1, fill);

            if (state.getCursorRow (scr) + 1 < visibleRows)
            {
                grid.eraseRowRange (state.getCursorRow (scr) + 1, visibleRows - 1, fill);
            }
            break;

        case 1:
            if (state.getCursorRow (scr) > 0)
            {
                grid.eraseRowRange (0, state.getCursorRow (scr) - 1, fill);
            }

            grid.eraseCellRange (state.getCursorRow (scr), 0, state.getCursorCol (scr), fill);
            break;

        case 2:
        case 3:
            grid.eraseRowRange (0, visibleRows - 1, fill);
            break;

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
 * Erases part or all of the cursor's current row.  The cursor position is not
 * changed.  Erased cells are filled with the default (blank) cell value.
 *
 * @par Mode table
 *
 * | Mode | VT sequence | Effect                                              |
 * |------|-------------|-----------------------------------------------------|
 * | 0    | `CSI K`     | Erase from cursor to end of line (inclusive)        |
 * | 1    | `CSI 1 K`   | Erase from start of line to cursor (inclusive)      |
 * | 2    | `CSI 2 K`   | Erase entire line                                   |
 *
 * @par Mode 0 — erase to right
 * @code
 * grid.eraseCellRange (row, col, cols - 1);
 * @endcode
 *
 * @par Mode 1 — erase to left
 * @code
 * grid.eraseCellRange (row, 0, col);
 * @endcode
 *
 * @par Mode 2 — erase entire line
 * @code
 * grid.eraseRow (row);
 * @endcode
 *
 * @param mode  Erase mode (0 = to right, 1 = to left, 2 = entire line).
 *              Unknown modes are silently ignored.
 *
 * @note READER THREAD only.
 * @note Does not respect the scroll region.
 *
 * @see eraseInDisplay()       — erases across multiple rows
 * @see Grid::eraseCellRange() — erases a range of cells within a row
 * @see Grid::eraseRow()       — erases an entire row
 */
void Parser::eraseInLine (int mode) noexcept
{
    const auto scr { state.getScreen() };
    const int cols { grid.getCols() };

    Cell fill {};
    fill.bg = stamp.bg;

    switch (mode)
    {
        case 0:
            grid.eraseCellRange (state.getCursorRow (scr), state.getCursorCol (scr), cols - 1, fill);
            break;

        case 1:
            grid.eraseCellRange (state.getCursorRow (scr), 0, state.getCursorCol (scr), fill);
            break;

        case 2:
            grid.eraseRow (state.getCursorRow (scr), fill);
            break;

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
 * @par Scroll region constraint
 * If the cursor is above `scrollTop` or below `scrollBottom`, this is a no-op.
 *
 * @par Example (count = 2, cursor at row 3, scrollBottom = 9)
 * @code
 * // Before:  rows 3–9 contain content
 * // After:   rows 3–4 are blank; rows 5–9 contain what was in rows 3–7
 * //          (rows 8–9 of the original content are lost)
 * @endcode
 *
 * @param count  Number of blank lines to insert (>= 1).
 *
 * @note READER THREAD only.
 *
 * @see shiftLines()          — shared implementation for IL and DL
 * @see shiftLinesUp()        — DL (Delete Lines) counterpart
 * @see Grid::scrollRegionDown() — underlying scroll primitive
 */
void Parser::shiftLinesDown (int count) noexcept
{
    shiftLines (count, false);
}

/**
 * @brief Handles `CSI Pn M` — Delete Lines (DL).
 *
 * Deletes `count` lines at the cursor row by scrolling the region from the
 * cursor row to `scrollBottom` upward.  Blank lines are inserted at the bottom
 * of the scroll region to fill the vacated space.  The cursor column is reset
 * to 0.
 *
 * @par Scroll region constraint
 * If the cursor is above `scrollTop` or below `scrollBottom`, this is a no-op.
 *
 * @par Example (count = 2, cursor at row 3, scrollBottom = 9)
 * @code
 * // Before:  rows 3–9 contain content
 * // After:   rows 3–7 contain what was in rows 5–9; rows 8–9 are blank
 * @endcode
 *
 * @param count  Number of lines to delete (>= 1).
 *
 * @note READER THREAD only.
 *
 * @see shiftLines()        — shared implementation for IL and DL
 * @see shiftLinesDown()    — IL (Insert Lines) counterpart
 * @see Grid::scrollRegionUp() — underlying scroll primitive
 */
void Parser::shiftLinesUp (int count) noexcept
{
    shiftLines (count, true);
}

/**
 * @brief Shared implementation for Insert Lines (IL) and Delete Lines (DL).
 *
 * Validates that the cursor is within the active scroll region, then delegates
 * to the appropriate Grid scroll primitive.  After scrolling, the cursor column
 * is reset to 0 and the wrap-pending flag is cleared.
 *
 * @par Scroll region guard
 * The operation is a no-op if:
 * - `cursorRow < scrollTop`  (cursor is above the scroll region)
 * - `cursorRow > scrollBottom` (cursor is below the scroll region)
 *
 * @param count  Number of lines to shift (>= 1).
 * @param up     `true`  → scroll region up (DL: delete lines at cursor).
 *               `false` → scroll region down (IL: insert lines at cursor).
 *
 * @note READER THREAD only.
 *
 * @see shiftLinesDown()         — IL public entry point
 * @see shiftLinesUp()           — DL public entry point
 * @see Grid::scrollRegionUp()   — scrolls rows upward within a region
 * @see Grid::scrollRegionDown() — scrolls rows downward within a region
 */
void Parser::shiftLines (int count, bool up) noexcept
{
    const auto scr { state.getScreen() };
    const int bottom { scrollBottom };

    if (state.getCursorRow (scr) >= state.getScrollTop (scr) and state.getCursorRow (scr) <= bottom)
    {
        Cell fill {};
        fill.bg = stamp.bg;

        if (up)
        {
            grid.scrollRegionUp (state.getCursorRow (scr), bottom, count, fill);
        }
        else
        {
            grid.scrollRegionDown (state.getCursorRow (scr), bottom, count, fill);
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
 * Inserts `count` blank cells at the cursor column by shifting existing cells
 * to the right.  Cells that shift past the right margin are discarded.  The
 * cursor position is not changed.
 *
 * @par Algorithm
 * @code
 * // Shift cells right: work from right margin toward cursor
 * for col in (cols-1) downto (cursorCol + count):
 *     rowCells[col] = rowCells[col - count];
 *
 * // Clear the vacated cells
 * for col in cursorCol to min(cursorCol + count - 1, cols - 1):
 *     rowCells[col] = Cell {};
 *     grid.activeEraseGrapheme (row, col);
 * @endcode
 *
 * @param count  Number of blank cells to insert (>= 1).
 *
 * @note READER THREAD only.
 * @note If `rowCells` is null (row not allocated), the operation is a no-op.
 *
 * @see removeCells()  — DCH (Delete Characters) counterpart
 * @see eraseCells()   — ECH (Erase Characters), erases without shifting
 * @see Grid::activeVisibleRow()     — returns a pointer to the row's cell array
 * @see Grid::activeEraseGrapheme()  — clears the grapheme string for a cell
 */
void Parser::shiftCellsRight (int count) noexcept
{
    const auto scr { state.getScreen() };
    const int cols { grid.getCols() };
    Cell* rowCells { grid.activeVisibleRow (state.getCursorRow (scr)) };

    if (rowCells != nullptr)
    {
        for (int col { cols - 1 }; col >= state.getCursorCol (scr) + count; --col)
        {
            rowCells[col] = rowCells[col - count];
        }

        Cell fill {};
        fill.bg = stamp.bg;

        for (int col { state.getCursorCol (scr) }; col < juce::jmin (state.getCursorCol (scr) + count, cols); ++col)
        {
            rowCells[col] = fill;
            grid.activeEraseGrapheme (state.getCursorRow (scr), col);
        }

        grid.markRowDirty (state.getCursorRow (scr));
    }
}

/**
 * @brief Handles `CSI Pn P` — Delete Characters (DCH).
 *
 * Deletes `count` cells at the cursor column by shifting the remaining cells
 * to the left.  Blank cells are inserted at the right margin to fill the
 * vacated space.  The cursor position is not changed.
 *
 * @par Algorithm
 * @code
 * // Shift cells left: work from cursor toward right margin
 * for col in cursorCol to (cols - count - 1):
 *     rowCells[col] = rowCells[col + count];
 *
 * // Clear the vacated cells at the right end
 * for col in (cols - count) to (cols - 1):
 *     rowCells[col] = Cell {};
 *     grid.activeEraseGrapheme (row, col);
 * @endcode
 *
 * @param count  Number of cells to delete (>= 1).
 *
 * @note READER THREAD only.
 * @note If `rowCells` is null (row not allocated), the operation is a no-op.
 *
 * @see shiftCellsRight()  — ICH (Insert Characters) counterpart
 * @see eraseCells()       — ECH (Erase Characters), erases without shifting
 * @see Grid::activeVisibleRow()     — returns a pointer to the row's cell array
 * @see Grid::activeEraseGrapheme()  — clears the grapheme string for a cell
 */
void Parser::removeCells (int count) noexcept
{
    const auto scr { state.getScreen() };
    const int cols { grid.getCols() };
    Cell* rowCells { grid.activeVisibleRow (state.getCursorRow (scr)) };

    if (rowCells != nullptr)
    {
        for (int col { state.getCursorCol (scr) }; col < cols - count; ++col)
        {
            rowCells[col] = rowCells[col + count];
        }

        Cell fill {};
        fill.bg = stamp.bg;

        for (int col { cols - count }; col < cols; ++col)
        {
            rowCells[col] = fill;
            grid.activeEraseGrapheme (state.getCursorRow (scr), col);
        }

        grid.markRowDirty (state.getCursorRow (scr));
    }
}

/**
 * @brief Handles `CSI Pn X` — Erase Characters (ECH).
 *
 * Erases `count` cells starting at the cursor column without shifting any
 * surrounding content.  The cursor position is not changed.
 *
 * @par Difference from DCH
 * `eraseCells()` (ECH) blanks cells in-place; `removeCells()` (DCH) shifts
 * the remaining content left.  ECH is equivalent to writing `count` space
 * characters with the default pen, but without advancing the cursor.
 *
 * @par Clamping
 * The erase range is clamped to `[cursorCol, cols - 1]` so that `count` values
 * larger than the remaining line width do not overflow.
 *
 * @param count  Number of cells to erase (>= 1).
 *
 * @note READER THREAD only.
 *
 * @see removeCells()          — DCH (Delete Characters), shifts content left
 * @see shiftCellsRight()      — ICH (Insert Characters), shifts content right
 * @see Grid::eraseCellRange() — underlying erase primitive
 */
void Parser::eraseCells (int count) noexcept
{
    const auto scr { state.getScreen() };
    const int cols { grid.getCols() };
    const int endCol { juce::jmin (state.getCursorCol (scr) + count - 1, cols - 1) };

    Cell fill {};
    fill.bg = stamp.bg;

    grid.eraseCellRange (state.getCursorRow (scr), state.getCursorCol (scr), endCol, fill);
}

// ============================================================================
// CSI Handler — REP
// ============================================================================

/**
 * @brief Handles `CSI Ps b` — Repeat preceding graphic character (REP).
 *
 * Repeats the last graphic character printed Ps times at the current cursor
 * position, advancing the cursor.  Uses the existing `print()` path so that
 * all cell writes, dirty marking, and wrap handling are consistent.
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
 * Implements the DEC private mode ?1049 (alternate screen with cursor save).
 * When switching to the alternate screen, the grid buffer is cleared and the
 * cursor is clamped to the new dimensions.  When returning to the normal screen,
 * all cells are marked dirty to force a full repaint.
 *
 * @par Switching to alternate screen (`shouldUseAlternate = true`)
 * 1. Sets `State::screen` to `alternate`.
 * 2. Resets the scroll region for the alternate screen.
 * 3. Calls `calc()` to sync cached geometry.
 * 4. Clears the alternate screen buffer (`Grid::clearBuffer()`).
 * 5. Clamps the cursor to the current terminal dimensions.
 *
 * @par Returning to normal screen (`shouldUseAlternate = false`)
 * 1. Sets `State::screen` to `normal`.
 * 2. Resets the scroll region for the normal screen.
 * 3. Calls `calc()` to sync cached geometry.
 * 4. Marks all cells dirty (`Grid::markAllDirty()`) to force a full repaint.
 *
 * @par No-op guard
 * If the target screen is already active, the function returns immediately
 * without modifying any state.
 *
 * @param shouldUseAlternate  `true` to activate the alternate screen buffer,
 *                            `false` to return to the normal screen buffer.
 *
 * @note READER THREAD only.
 *
 * @see Grid::clearBuffer()   — clears the alternate screen on activation
 * @see Grid::markAllDirty()  — forces full repaint on return to normal screen
 * @see cursorResetScrollRegion() — resets scroll region after screen switch
 * @see calc()                — syncs `scrollBottom` and other cached state
 */
void Parser::setScreen (bool shouldUseAlternate) noexcept
{
    const auto target { shouldUseAlternate ? alternate : normal };

    if (target != state.getScreen())
    {
        state.setScreen (target);
        const auto scr { state.getScreen() };
        cursorResetScrollRegion (scr);
        calc();

        if (target == alternate)
        {
            grid.clearBuffer();
            cursorClamp (scr, grid.getCols(), grid.getVisibleRows());
        }
        else
        {
            grid.markAllDirty();
        }
    }
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Terminal
