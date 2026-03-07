/**
 * @file GridErase.cpp
 * @brief Grid member functions for erase operations.
 *
 * This file implements the erase half of the Grid class.  All functions here
 * operate on the active screen buffer and are called exclusively on the
 * **READER THREAD**.
 *
 * ## Erase semantics
 *
 * Terminal erase operations replace cells with the **default cell** — a
 * default-constructed `Cell {}` whose codepoint is 0 (blank), whose
 * foreground and background colours are the terminal defaults, and whose
 * attribute flags are all clear.  This matches the VT standard: erased cells
 * carry no colour or attribute from the current pen.
 *
 * @note The VT standard also defines "selective erase" (DECSED/DECSEL) which
 *       respects the selective-erase attribute, but that path is handled at a
 *       higher layer.  The functions here always write `Cell {}`.
 *
 * ## Three-part row clear
 *
 * `clearRow()` — the lowest-level primitive — resets all three parallel
 * arrays for a row in a single call:
 *
 * | Array              | Reset action                          |
 * |--------------------|---------------------------------------|
 * | `buffer.cells`     | `std::fill` with `Cell {}`            |
 * | `buffer.graphemes` | `std::fill` with `Grapheme {}`        |
 * | `buffer.rowStates` | Assign `RowState {}` (clears wrap flag)|
 *
 * Higher-level functions (`eraseRow`, `eraseRowRange`, `eraseCell`,
 * `eraseCellRange`) add bounds checking and dirty-row marking on top of this
 * primitive.
 *
 * ## Dirty tracking
 *
 * Every public erase function calls `markRowDirty()` for each affected row so
 * that the MESSAGE THREAD repaints exactly the rows that changed.
 *
 * @see Grid.h   — class declaration, ring-buffer layout, thread ownership table.
 * @see Cell     — 16-byte terminal cell type; default construction yields a blank cell.
 * @see Grapheme — extra codepoints for multi-codepoint grapheme clusters.
 * @see RowState — per-row metadata; cleared alongside cells to reset the wrap flag.
 */

#include "Grid.h"

namespace Terminal
{ /*____________________________________________________________________________*/

/**
 * @brief Fills all cells, graphemes, and the RowState for visible row `visibleRow`
 *        with their default-constructed values.
 *
 * This is the lowest-level erase primitive.  It resets all three parallel
 * arrays for the given row:
 *
 * - `buffer.cells` — filled with `Cell {}` (blank codepoint, default colours,
 *   no attributes).
 * - `buffer.graphemes` — filled with `Grapheme {}` (zero extra codepoints).
 * - `buffer.rowStates[phys]` — assigned `RowState {}` (wrap flag cleared,
 *   double-width flag cleared).
 *
 * @par Usage
 * Called by scroll operations to blank newly exposed rows, and by the public
 * erase functions to reset individual rows.  Does **not** call `markRowDirty()`
 * — callers that need dirty tracking must do so themselves.
 *
 * @param buffer      Target buffer (normal or alternate screen).
 * @param visibleRow  Zero-based visible row index (0 = top of viewport).
 * @note READER THREAD — lock-free, noexcept.
 * @see eraseRow(), shiftRegionUp(), shiftRegionDown()
 */
void Grid::clearRow (Buffer& buffer, int visibleRow) noexcept
{
    Cell* row { rowPtr (buffer, visibleRow) };
    const Cell defaultCell {};
    std::fill (row, row + cols, defaultCell);

    const int phys { physicalRow (buffer, visibleRow) };
    buffer.rowStates[phys] = RowState {};

    Grapheme* gRow { buffer.graphemes.get() + phys * cols };
    std::fill (gRow, gRow + cols, Grapheme {});
}

/**
 * @brief Erases all cells in visible row `row` and resets its RowState.
 *
 * Bounds-checks `row` against [0, visibleRows), then delegates to `clearRow()`
 * to reset cells, graphemes, and the RowState.  Calls `markRowDirty (row)` so
 * that the MESSAGE THREAD repaints the affected row.
 *
 * @param row  Zero-based visible row index (0 … visibleRows-1).
 * @note READER THREAD — lock-free, noexcept.
 * @see eraseRowRange(), clearRow()
 */
void Grid::eraseRow (int row) noexcept
{
    if (row >= 0 and row < visibleRows)
    {
        Buffer& buffer { bufferForScreen() };
        clearRow (buffer, row);
        markRowDirty (row);
    }
}

/**
 * @brief Erases all rows in the inclusive range [startRow, endRow].
 *
 * Calls `eraseRow()` for each row in the range.  Each call bounds-checks its
 * argument and marks the row dirty independently.
 *
 * @par Typical callers
 * - ED 0 (erase from cursor to end of screen): `eraseRowRange (cursor.row + 1, visibleRows - 1)`
 * - ED 1 (erase from start of screen to cursor): `eraseRowRange (0, cursor.row - 1)`
 * - ED 2 (erase entire screen): `eraseRowRange (0, visibleRows - 1)`
 *
 * @param startRow  First row to erase (inclusive, 0-based).
 * @param endRow    Last row to erase (inclusive, 0-based).
 * @note READER THREAD — lock-free, noexcept.
 * @see eraseRow()
 */
void Grid::eraseRowRange (int startRow, int endRow) noexcept
{
    for (int r { startRow }; r <= endRow; ++r)
    {
        eraseRow (r);
    }
}

/**
 * @brief Erases the single cell at (row, col).
 *
 * Bounds-checks both coordinates against [0, visibleRows) × [0, cols), writes
 * a default-constructed `Cell {}` to the target position, and calls
 * `markRowDirty (row)`.
 *
 * @note The grapheme sidecar for this cell is **not** explicitly cleared here.
 *       If the cell previously held a grapheme cluster, the stale `Grapheme`
 *       entry remains in the sidecar but is unreachable because the cell's
 *       `LAYOUT_GRAPHEME` flag is cleared by the `Cell {}` assignment.
 *
 * @param row  Zero-based visible row index.
 * @param col  Zero-based column index.
 * @note READER THREAD — lock-free, noexcept.
 * @see eraseCellRange(), eraseRow()
 */
void Grid::eraseCell (int row, int col) noexcept
{
    if (row >= 0 and row < visibleRows and col >= 0 and col < cols)
    {
        Buffer& buffer { bufferForScreen() };
        rowPtr (buffer, row)[col] = Cell {};
        markRowDirty (row);
    }
}

/**
 * @brief Erases cells in the inclusive column range [startCol, endCol] on row `row`.
 *
 * Bounds-checks `row` against [0, visibleRows), then clamps `startCol` and
 * `endCol` to [0, cols-1] before writing.  Each cell in the clamped range is
 * replaced with a default-constructed `Cell {}`.  Calls `markRowDirty (row)`.
 *
 * @par Typical callers
 * - EL 0 (erase from cursor to end of line): `eraseCellRange (row, cursor.col, cols - 1)`
 * - EL 1 (erase from start of line to cursor): `eraseCellRange (row, 0, cursor.col)`
 * - EL 2 (erase entire line): `eraseCellRange (row, 0, cols - 1)`
 *
 * @param row       Zero-based visible row index.
 * @param startCol  First column to erase (inclusive; clamped to 0 if negative).
 * @param endCol    Last column to erase (inclusive; clamped to cols-1 if too large).
 * @note READER THREAD — lock-free, noexcept.
 * @see eraseCell(), eraseRow()
 */
void Grid::eraseCellRange (int row, int startCol, int endCol) noexcept
{
    if (row >= 0 and row < visibleRows)
    {
        Buffer& buffer { bufferForScreen() };
        Cell* rowCells { rowPtr (buffer, row) };
        const int clampedStart { juce::jmax (0, startCol) };
        const int clampedEnd { juce::jmin (cols - 1, endCol) };
        const Cell defaultCell {};

        for (int c { clampedStart }; c <= clampedEnd; ++c)
        {
            rowCells[c] = defaultCell;
        }

        markRowDirty (row);
    }
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Terminal
