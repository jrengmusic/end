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
 * Grid is pure storage — it never decides what a cell looks like.  Every
 * erase function receives a `const Cell& fill` that the caller has prepared
 * with the appropriate background colour (BCE).  When the caller omits the
 * argument the default `Cell {}` is used (theme-default colours, no content).
 *
 * @see Grid.h   — class declaration, ring-buffer layout, thread ownership table.
 * @see Cell     — 16-byte terminal cell type; default construction yields a blank cell.
 * @see Grapheme — extra codepoints for multi-codepoint grapheme clusters.
 * @see RowState — per-row metadata; cleared alongside cells to reset the wrap flag.
 */

#include "Grid.h"

namespace Terminal
{ /*____________________________________________________________________________*/

void Grid::clearRow (Buffer& buffer, int visibleRow, const Cell& fill) noexcept
{
    Cell* row { rowPtr (buffer, visibleRow) };
    std::fill (row, row + cols, fill);

    const int phys { physicalRow (buffer, visibleRow) };
    buffer.rowStates[phys] = RowState {};

    Grapheme* gRow { buffer.graphemes.get() + phys * cols };
    std::fill (gRow, gRow + cols, Grapheme {});
}

void Grid::eraseRow (int row, const Cell& fill) noexcept
{
    if (row >= 0 and row < visibleRows)
    {
        Buffer& buffer { bufferForScreen() };
        clearRow (buffer, row, fill);
        markRowDirty (row);
    }
}

void Grid::eraseRowRange (int startRow, int endRow, const Cell& fill) noexcept
{
    for (int r { startRow }; r <= endRow; ++r)
    {
        eraseRow (r, fill);
    }
}

void Grid::eraseCell (int row, int col, const Cell& fill) noexcept
{
    if (row >= 0 and row < visibleRows and col >= 0 and col < cols)
    {
        Buffer& buffer { bufferForScreen() };
        rowPtr (buffer, row)[col] = fill;
        markRowDirty (row);
    }
}

void Grid::eraseCellRange (int row, int startCol, int endCol, const Cell& fill) noexcept
{
    if (row >= 0 and row < visibleRows)
    {
        Buffer& buffer { bufferForScreen() };
        Cell* rowCells { rowPtr (buffer, row) };
        const int clampedStart { juce::jmax (0, startCol) };
        const int clampedEnd { juce::jmin (cols - 1, endCol) };

        for (int c { clampedStart }; c <= clampedEnd; ++c)
        {
            rowCells[c] = fill;
        }

        markRowDirty (row);
    }
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Terminal
