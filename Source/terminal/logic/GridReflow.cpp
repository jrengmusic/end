/**
 * @file GridReflow.cpp
 * @brief Grid member functions for terminal resize and content reflow.
 *
 * This file implements `Grid::resize()` and the reflow algorithm that
 * re-wraps soft-wrapped lines when the terminal column width changes.
 * All public entry points are called on the **READER THREAD** under
 * `resizeLock`.
 *
 * ## Why reflow?
 *
 * When a terminal is resized, lines that were soft-wrapped at the old column
 * width must be re-wrapped at the new width.  Without reflow, a line that was
 * split across two rows at 80 columns would remain split at 120 columns,
 * leaving a visible gap.  Reflow reconstructs the original logical lines and
 * re-wraps them at the new width, preserving content.
 *
 * ## Algorithm overview
 *
 * The reflow algorithm runs in four passes:
 *
 * @par Pass 1 — Find last content row
 * `findLastContent()` scans visible rows from the bottom upward to find the
 * last row that contains at least one non-blank cell.  This bounds the reflow
 * to only the rows that actually hold content, avoiding unnecessary work on
 * empty trailing rows.
 *
 * @par Pass 2 — Count output rows (dry run)
 * `countOutputRows()` walks every logical line in the old buffer (following
 * soft-wrap chains via `nextLogicalLine()`), flattens each logical line into a
 * temporary buffer via `flattenLogicalLine()`, and counts how many new rows
 * each logical line will occupy at the new column width.  This total is used
 * to compute how many rows to skip at the top so that the new buffer's
 * scrollback does not overflow.
 *
 * @par Pass 3 — Write reflowed content
 * `writeReflowedContent()` repeats the same logical-line walk, this time
 * writing each re-wrapped row into the new buffer via `writeNewRow()`.  Rows
 * that would overflow the new buffer's total capacity are skipped (counted by
 * `rowsToSkip`).
 *
 * @par Pass 4 — Update buffer metadata
 * `Grid::reflow()` sets `newBuffer.head` and `newBuffer.scrollbackUsed` based
 * on the number of rows actually written.
 *
 * ## Logical lines and soft-wrap chains
 *
 * A "logical line" is a sequence of one or more physical rows connected by the
 * `RowState::isWrapped()` flag.  The last row in the chain has `isWrapped()`
 * false.  `nextLogicalLine()` returns the run length (number of physical rows)
 * for the logical line starting at a given linear row index.
 *
 * ## Linear row indexing
 *
 * During reflow, rows are addressed by a "linear" index that spans both
 * scrollback and visible rows:
 *
 * @code
 * linear index 0                    → oldest scrollback row
 * linear index scrollbackUsed       → first visible row (row 0)
 * linear index scrollbackUsed + r   → visible row r
 * @endcode
 *
 * `linearToPhysical()` converts a linear index to a physical ring-buffer index
 * using the old buffer's `head` and `totalRows`.
 *
 * ## Wide character handling
 *
 * Wide characters (e.g. CJK ideographs) occupy two columns: a leading cell
 * with `LAYOUT_WIDE_LEAD` and a trailing continuation cell with
 * `LAYOUT_WIDE_CONT`.  During reflow, `flattenLogicalLine()` copies cells
 * verbatim into the flat temporary buffer.  `writeNewRow()` then copies up to
 * `newCols` cells per output row.  If a wide character's leading cell falls at
 * column `newCols - 1`, the continuation cell would be cut off; the terminal
 * writer is responsible for not placing wide characters at the last column, so
 * this case should not arise in practice.
 *
 * @see Grid.h   — class declaration, ring-buffer layout, thread ownership table.
 * @see Cell     — 16-byte terminal cell type; `hasContent()` used to find last content.
 * @see RowState — per-row metadata; `isWrapped()` / `setWrapped()` drive the reflow walk.
 * @see State    — atomic terminal parameter store (new dimensions read here).
 */

#include "Grid.h"
#include "../data/State.h"

namespace Terminal
{ /*____________________________________________________________________________*/

// ============================================================================
// Resize
// ============================================================================

/**
 * @brief Re-allocates the active screen buffer to the new terminal dimensions
 *        and reflows content if the column count changed.
 *
 * Acquires `resizeLock` and reads the new dimensions from `state`.  If neither
 * `cols` nor `visibleRows` changed, the function returns immediately.
 *
 * @par Normal screen
 * A new primary buffer is allocated at the new dimensions (with scrollback),
 * then `reflow()` copies and re-wraps all content from the old buffer into the
 * new one.  The old buffer is replaced by `std::move`.
 *
 * @par Alternate screen
 * The alternate screen has no scrollback and does not reflow — its buffer is
 * simply re-initialised (content is discarded), matching xterm behaviour.
 *
 * After resizing, `markAllDirty()` is called so the renderer repaints the
 * entire viewport.
 *
 * @note READER THREAD — acquires `resizeLock`.  Allocates heap memory.
 * @see reflow(), initBuffer(), markAllDirty()
 */
void Grid::resize()
{
    juce::ScopedLock lock (resizeLock);

    const int newCols { state.getCols() };
    const int newVisibleRows { state.getVisibleRows() };
    const bool dimensionsChanged { newCols != cols or newVisibleRows != visibleRows };

    if (dimensionsChanged)
    {
        const int oldCols { cols };
        const int oldVisibleRows { visibleRows };

        cols = newCols;
        visibleRows = newVisibleRows;

        const auto scr { state.getScreen() };

        if (scr == normal)
        {
            Buffer newPrimary;
            initBuffer (newPrimary, cols, visibleRows + scrollbackCapacity, visibleRows);
            reflow (buffers.at (normal), oldCols, oldVisibleRows, newPrimary, cols, visibleRows);
            buffers.at (normal) = std::move (newPrimary);
            initBuffer (buffers.at (alternate), cols, visibleRows, visibleRows);
        }
        else
        {
            initBuffer (buffers.at (alternate), cols, visibleRows, visibleRows);
            Buffer newPrimary;
            initBuffer (newPrimary, cols, visibleRows + scrollbackCapacity, visibleRows);
            reflow (buffers.at (normal), oldCols, oldVisibleRows, newPrimary, cols, visibleRows);
            buffers.at (normal) = std::move (newPrimary);
        }

        markAllDirty();
    }
}

// ============================================================================
// Reflow helpers (file-local)
// ============================================================================

/**
 * @brief Converts a linear row index to a physical ring-buffer row index in
 *        the old buffer.
 *
 * Linear row indices span both scrollback and visible rows:
 * - `linearRow == 0` is the oldest scrollback row.
 * - `linearRow == scrollbackUsed` is visible row 0 (top of viewport).
 *
 * The conversion maps the linear index to a visible-row offset relative to the
 * old buffer's `head`, then applies the ring-buffer formula:
 *
 * @code
 * visibleRow = linearRow - scrollbackUsed
 * raw        = (head - oldVisibleRows + 1 + visibleRow) % totalRows
 * physical   = (raw + totalRows) % totalRows   // ensure non-negative
 * @endcode
 *
 * @param head           Physical index of the bottom visible row in the old buffer.
 * @param totalRows      Total allocated rows in the old buffer (power of two).
 * @param oldVisibleRows Number of visible rows in the old buffer.
 * @param scrollbackUsed Number of scrollback rows stored in the old buffer.
 * @param linearRow      Linear row index to convert.
 * @return Physical row index in [0, totalRows).
 */
static int linearToPhysical (int head, int totalRows, int oldVisibleRows,
                             int scrollbackUsed, int linearRow) noexcept
{
    const int visibleRow { linearRow - scrollbackUsed };
    const int raw { (head - oldVisibleRows + 1 + visibleRow) % totalRows };
    return (raw + totalRows) % totalRows;
}

/**
 * @brief Finds the last visible row that contains at least one non-blank cell.
 *
 * Scans visible rows from the bottom upward.  For each row, scans columns from
 * right to left and returns the row index as soon as a cell with
 * `Cell::hasContent()` is found.
 *
 * @par Purpose
 * Bounds the reflow to only the rows that hold actual content.  Empty trailing
 * rows are excluded from the logical-line walk, avoiding unnecessary output
 * rows in the new buffer.
 *
 * @param cells          Flat cell array of the old buffer.
 * @param oldCols        Column count of the old buffer.
 * @param totalRows      Total allocated rows in the old buffer.
 * @param head           Physical index of the bottom visible row.
 * @param oldVisibleRows Number of visible rows in the old buffer.
 * @param scrollbackUsed Number of scrollback rows stored in the old buffer.
 * @return Zero-based visible row index of the last row with content, or -1 if
 *         all visible rows are empty.
 */
static int findLastContent (const Cell* cells, int oldCols, int totalRows,
                            int head, int oldVisibleRows, int scrollbackUsed) noexcept
{
    for (int row { oldVisibleRows - 1 }; row >= 0; --row)
    {
        const int linear { scrollbackUsed + row };
        const int phys { linearToPhysical (head, totalRows, oldVisibleRows, scrollbackUsed, linear) };
        const Cell* rowCells { cells + phys * oldCols };

        for (int col { oldCols - 1 }; col >= 0; --col)
        {
            if (rowCells[col].hasContent())
            {
                return row;
            }
        }
    }

    return -1;
}

/**
 * @brief Copies `lineCount` consecutive physical rows into a flat temporary
 *        buffer and returns the number of content cells.
 *
 * Reads `lineCount` rows starting at linear index `startLinear` from the old
 * buffer and copies them sequentially into `tempCells` / `tempGraphs` (each
 * row occupies `oldCols` entries).  After copying, trailing blank cells are
 * trimmed from the end of the flat buffer.
 *
 * @par Output layout
 * @code
 * tempCells[0 … oldCols-1]           ← row startLinear
 * tempCells[oldCols … 2*oldCols-1]   ← row startLinear + 1
 * ...
 * @endcode
 *
 * @param cells          Flat cell array of the old buffer.
 * @param graphemes      Flat grapheme array of the old buffer.
 * @param oldCols        Column count of the old buffer.
 * @param totalRows      Total allocated rows in the old buffer.
 * @param head           Physical index of the bottom visible row.
 * @param oldVisibleRows Number of visible rows in the old buffer.
 * @param scrollbackUsed Number of scrollback rows stored in the old buffer.
 * @param startLinear    Linear index of the first row in the logical line.
 * @param lineCount      Number of physical rows in the logical line (run length).
 * @param tempCells      Output flat cell buffer; must hold at least
 *                       `lineCount * oldCols` entries.
 * @param tempGraphs     Output flat grapheme buffer; same size requirement.
 * @return Number of non-blank cells at the start of the flat buffer (trailing
 *         blanks are excluded).  Returns 0 if the logical line is entirely blank.
 */
static int flattenLogicalLine (const Cell* cells, const Grapheme* graphemes,
                               int oldCols, int totalRows, int head,
                               int oldVisibleRows, int scrollbackUsed,
                               int startLinear, int lineCount,
                               Cell* tempCells, Grapheme* tempGraphs) noexcept
{
    for (int r { 0 }; r < lineCount; ++r)
    {
        const int phys { linearToPhysical (head, totalRows, oldVisibleRows, scrollbackUsed, startLinear + r) };
        const int srcOff { phys * oldCols };
        const int dstOff { r * oldCols };
        std::memcpy (tempCells + dstOff, cells + srcOff, static_cast<size_t> (oldCols) * sizeof (Cell));
        std::memcpy (tempGraphs + dstOff, graphemes + srcOff, static_cast<size_t> (oldCols) * sizeof (Grapheme));
    }

    int total { lineCount * oldCols };

    while (total > 0 and not tempCells[total - 1].hasContent())
    {
        --total;
    }

    return total;
}

/**
 * @brief Writes one output row of reflowed content into the new buffer.
 *
 * Copies up to `newCols` cells from `src + srcOffset` into the new buffer at
 * physical row `writePhys`.  If `cellCount < newCols`, the remainder of the
 * row is filled with blank `Cell {}` and `Grapheme {}` entries.  Sets the
 * `RowState::wrapped` flag according to `wrapped`.
 *
 * @param dstCells     Flat cell array of the new buffer.
 * @param dstGraphemes Flat grapheme array of the new buffer.
 * @param dstStates    RowState array of the new buffer (indexed by physical row).
 * @param newCols      Column count of the new buffer.
 * @param writePhys    Physical row index in the new buffer to write into.
 * @param src          Source flat cell array (from `flattenLogicalLine()`).
 * @param srcG         Source flat grapheme array.
 * @param srcOffset    Byte offset (in cells) into `src` / `srcG` for this output row.
 * @param cellCount    Number of content cells to copy (may be less than `newCols`).
 * @param wrapped      `true` if this output row is followed by another row from
 *                     the same logical line (i.e. the logical line continues).
 */
static void writeNewRow (Cell* dstCells, Grapheme* dstGraphemes, RowState* dstStates,
                         int newCols, int writePhys,
                         const Cell* src, const Grapheme* srcG,
                         int srcOffset, int cellCount, bool wrapped) noexcept
{
    const int dstOff { writePhys * newCols };
    const int toCopy { juce::jmin (cellCount, newCols) };

    std::memcpy (dstCells + dstOff, src + srcOffset, static_cast<size_t> (toCopy) * sizeof (Cell));
    std::memcpy (dstGraphemes + dstOff, srcG + srcOffset, static_cast<size_t> (toCopy) * sizeof (Grapheme));

    if (toCopy < newCols)
    {
        const Cell empty {};
        std::fill (dstCells + dstOff + toCopy, dstCells + dstOff + newCols, empty);
        std::fill (dstGraphemes + dstOff + toCopy, dstGraphemes + dstOff + newCols, Grapheme {});
    }

    dstStates[writePhys] = RowState {};
    dstStates[writePhys].setWrapped (wrapped);
}

/**
 * @brief Emits all output rows for one logical line into the new buffer.
 *
 * Divides the flat logical line (`flat`, length `flatLen`) into chunks of
 * `newCols` cells and calls `writeNewRow()` for each chunk.  Rows that fall
 * before `rowsToSkip` (i.e. rows that would overflow the new buffer's total
 * capacity) are counted but not written.
 *
 * @par Wrap flag
 * All output rows except the last carry `wrapped = true`.  The last row
 * carries `wrapped = false`, marking the end of the logical line.
 *
 * @par Empty logical lines
 * A logical line with `flatLen == 0` (entirely blank) still emits exactly one
 * output row (a blank row), so that blank lines in the original content are
 * preserved.
 *
 * @param flat            Flat cell array for this logical line.
 * @param flatG           Flat grapheme array for this logical line.
 * @param flatLen         Number of content cells in the flat buffer.
 * @param dstCells        Flat cell array of the new buffer.
 * @param dstGraphemes    Flat grapheme array of the new buffer.
 * @param dstStates       RowState array of the new buffer.
 * @param newCols         Column count of the new buffer.
 * @param newTotalRows    Total allocated rows in the new buffer (for modular wrap).
 * @param rowsToSkip      Number of output rows to skip at the start (overflow rows).
 * @param outputRowsSoFar Running count of output rows emitted so far (updated in place).
 * @param writePhys       Physical write head in the new buffer (updated in place).
 */
static void emitLogicalLine (const Cell* flat, const Grapheme* flatG, int flatLen,
                             Cell* dstCells, Grapheme* dstGraphemes, RowState* dstStates,
                             int newCols, int newTotalRows,
                             int rowsToSkip, int* outputRowsSoFar, int* writePhys) noexcept
{
    const int rowsNeeded { (flatLen > 0) ? (flatLen + newCols - 1) / newCols : 1 };

    for (int r { 0 }; r < rowsNeeded; ++r)
    {
        if (*outputRowsSoFar + r >= rowsToSkip)
        {
            const int offset { r * newCols };
            const int remaining { flatLen - offset };
            const int count { juce::jmax (0, juce::jmin (remaining, newCols)) };
            const bool wrapped { r < rowsNeeded - 1 };

            writeNewRow (dstCells, dstGraphemes, dstStates, newCols,
                         *writePhys, flat, flatG, offset, count, wrapped);
            *writePhys = (*writePhys + 1) % newTotalRows;
        }
    }

    *outputRowsSoFar += rowsNeeded;
}

// ============================================================================
// Logical line walk — shared by count and write passes
// ============================================================================

/**
 * @struct WalkParams
 * @brief Read-only parameters shared by the logical-line walk functions.
 *
 * Bundles all the old-buffer geometry needed by `nextLogicalLine()`,
 * `countOutputRows()`, and `writeReflowedContent()` into a single struct to
 * avoid long parameter lists.
 */
struct WalkParams
{
    /** Flat cell array of the old buffer. */
    const Cell* cells;

    /** Flat grapheme array of the old buffer. */
    const Grapheme* graphemes;

    /** RowState array of the old buffer (indexed by physical row). */
    const RowState* rowStates;

    /** Physical index of the bottom visible row in the old buffer. */
    int head;

    /** Total allocated rows in the old buffer (power of two). */
    int totalRows;

    /** Column count of the old buffer. */
    int oldCols;

    /** Number of visible rows in the old buffer. */
    int oldVisibleRows;

    /** Number of scrollback rows stored in the old buffer. */
    int scrollbackUsed;

    /** Total number of linear rows to walk (scrollbackUsed + lastContent + 1). */
    int linearRows;
};

/**
 * @brief Returns the run length of the logical line starting at linear row `r`.
 *
 * Follows the `RowState::isWrapped()` chain from row `r` forward, counting
 * consecutive wrapped rows.  The run ends at the first row whose `isWrapped()`
 * flag is false (the last physical row of the logical line).
 *
 * @par Example
 * If rows 5, 6, 7 form a logical line (rows 5 and 6 are wrapped, row 7 is not),
 * then `nextLogicalLine(wp, 5, &runLen)` sets `*outRunLen = 3` and returns 3.
 *
 * @param wp         Walk parameters for the old buffer.
 * @param r          Linear index of the first row of the logical line.
 * @param outRunLen  Output: number of physical rows in the logical line.
 * @return Same value as `*outRunLen`.
 */
static int nextLogicalLine (const WalkParams& wp, int r, int* outRunLen) noexcept
{
    int runLen { 1 };
    int phys { linearToPhysical (wp.head, wp.totalRows, wp.oldVisibleRows, wp.scrollbackUsed, r) };

    while (r + runLen < wp.linearRows and wp.rowStates[phys].isWrapped())
    {
        ++runLen;
        phys = linearToPhysical (wp.head, wp.totalRows, wp.oldVisibleRows, wp.scrollbackUsed, r + runLen - 1);
    }

    *outRunLen = runLen;
    return runLen;
}

/**
 * @brief Dry-run pass: counts the total number of output rows after reflow.
 *
 * Walks every logical line in the old buffer using `nextLogicalLine()` and
 * `flattenLogicalLine()`, and accumulates the number of new rows each logical
 * line will occupy at `newCols`.  No data is written to the new buffer.
 *
 * @par Purpose
 * The total output row count is used by `Grid::reflow()` to compute
 * `rowsToSkip` — the number of rows at the top of the output that must be
 * discarded because they would overflow the new buffer's total capacity.
 *
 * @param wp         Walk parameters for the old buffer.
 * @param tempCells  Scratch buffer for `flattenLogicalLine()`; must hold at
 *                   least `wp.linearRows * wp.oldCols` Cell entries.
 * @param tempGraphs Scratch buffer for `flattenLogicalLine()`; same size.
 * @param newCols    Column count of the new buffer.
 * @return Total number of output rows that the reflowed content will occupy.
 */
static int countOutputRows (const WalkParams& wp, Cell* tempCells, Grapheme* tempGraphs,
                            int newCols) noexcept
{
    int total { 0 };
    int r { 0 };

    while (r < wp.linearRows)
    {
        int runLen { 0 };
        nextLogicalLine (wp, r, &runLen);

        const int flatLen { flattenLogicalLine (wp.cells, wp.graphemes, wp.oldCols, wp.totalRows,
                                                wp.head, wp.oldVisibleRows, wp.scrollbackUsed,
                                                r, runLen, tempCells, tempGraphs) };
        total += (flatLen > 0) ? (flatLen + newCols - 1) / newCols : 1;
        r += runLen;
    }

    return total;
}

/**
 * @brief Write pass: emits all reflowed rows into the new buffer.
 *
 * Walks every logical line in the old buffer (same traversal as
 * `countOutputRows()`), flattens each logical line, and calls
 * `emitLogicalLine()` to write the re-wrapped rows into the new buffer.
 * Rows before `rowsToSkip` are counted but not written.
 *
 * @param wp           Walk parameters for the old buffer.
 * @param tempCells    Scratch buffer for `flattenLogicalLine()`.
 * @param tempGraphs   Scratch buffer for `flattenLogicalLine()`.
 * @param dstCells     Flat cell array of the new buffer.
 * @param dstGraphemes Flat grapheme array of the new buffer.
 * @param dstStates    RowState array of the new buffer.
 * @param newCols      Column count of the new buffer.
 * @param newTotalRows Total allocated rows in the new buffer.
 * @param rowsToSkip   Number of output rows to skip at the start (computed by
 *                     `Grid::reflow()` from `countOutputRows()` output).
 */
static void writeReflowedContent (const WalkParams& wp, Cell* tempCells, Grapheme* tempGraphs,
                                  Cell* dstCells, Grapheme* dstGraphemes, RowState* dstStates,
                                  int newCols, int newTotalRows, int rowsToSkip) noexcept
{
    int writePhys { 0 };
    int outputRowsSoFar { 0 };
    int r { 0 };

    while (r < wp.linearRows)
    {
        int runLen { 0 };
        nextLogicalLine (wp, r, &runLen);

        const int flatLen { flattenLogicalLine (wp.cells, wp.graphemes, wp.oldCols, wp.totalRows,
                                                wp.head, wp.oldVisibleRows, wp.scrollbackUsed,
                                                r, runLen, tempCells, tempGraphs) };

        emitLogicalLine (tempCells, tempGraphs, flatLen,
                         dstCells, dstGraphemes, dstStates,
                         newCols, newTotalRows,
                         rowsToSkip, &outputRowsSoFar, &writePhys);
        r += runLen;
    }
}

// ============================================================================
// Reflow
// ============================================================================

/**
 * @brief Reflows the content of `oldBuffer` into `newBuffer` at the new column width.
 *
 * This is the top-level reflow entry point, called by `resize()` when the
 * normal screen buffer is resized.  It orchestrates the four-pass algorithm
 * described in the file header.
 *
 * @par Step 1 — Bound the content
 * `findLastContent()` returns the last visible row with non-blank content.
 * The total number of linear rows to process is `scrollbackUsed + lastContent + 1`.
 * If there is no content (`lastContent == -1`), `linearRows` is 0 and the
 * function returns immediately, leaving `newBuffer` in its default state.
 *
 * @par Step 2 — Allocate scratch buffers
 * A `HeapBlock<Cell>` and `HeapBlock<Grapheme>` of size
 * `linearRows * oldCols` are allocated as scratch space for
 * `flattenLogicalLine()`.  These are reused across all logical-line iterations.
 *
 * @par Step 3 — Dry run
 * `countOutputRows()` computes the total number of output rows.  From this,
 * the function computes:
 * - `newScrollback` — how many rows of scrollback the reflowed content would
 *   produce.
 * - `scClamped` — `newScrollback` clamped to the new buffer's scrollback
 *   capacity (`newBuffer.totalRows - newVisibleRows`).
 * - `rowsToSkip` — rows at the top of the output to discard so that the
 *   written content fits exactly into `scClamped + newVisibleRows` rows.
 *
 * @par Step 4 — Write pass
 * `writeReflowedContent()` writes the reflowed rows into `newBuffer`, starting
 * at physical row 0 and advancing the write head modulo `newBuffer.totalRows`.
 *
 * @par Step 5 — Update metadata
 * `newBuffer.head` is set to the physical index of the last written row.
 * `newBuffer.scrollbackUsed` is set to `scClamped`.
 *
 * @param oldBuffer      Source buffer (read-only).  Must be the normal screen buffer.
 * @param oldCols        Column count of the source buffer.
 * @param oldVisibleRows Visible row count of the source buffer.
 * @param newBuffer      Destination buffer (already allocated by `initBuffer()`).
 * @param newCols        Column count of the destination buffer.
 * @param newVisibleRows Visible row count of the destination buffer.
 * @note READER THREAD — allocates temporary heap memory for scratch buffers.
 * @see resize(), countOutputRows(), writeReflowedContent()
 */
void Grid::reflow (const Buffer& oldBuffer, int oldCols, int oldVisibleRows,
                   Buffer& newBuffer, int newCols, int newVisibleRows)
{
    const int scrollbackUsed { oldBuffer.scrollbackUsed };
    const int lastContent { findLastContent (oldBuffer.cells.get(), oldCols, oldBuffer.totalRows,
                                             oldBuffer.head, oldVisibleRows, scrollbackUsed) };
    const int linearRows { scrollbackUsed + lastContent + 1 };

    if (linearRows > 0)
    {
        const WalkParams wp { oldBuffer.cells.get(), oldBuffer.graphemes.get(), oldBuffer.rowStates.get(),
                              oldBuffer.head, oldBuffer.totalRows, oldCols, oldVisibleRows,
                              scrollbackUsed, linearRows };

        const int maxFlatLen { linearRows * oldCols };
        juce::HeapBlock<Cell> tempCells (static_cast<size_t> (maxFlatLen));
        juce::HeapBlock<Grapheme> tempGraphs (static_cast<size_t> (maxFlatLen));

        const int totalOutputRows { countOutputRows (wp, tempCells.get(), tempGraphs.get(), newCols) };
        const int newScrollback { juce::jmax (0, totalOutputRows - newVisibleRows) };
        const int maxSc { newBuffer.totalRows - newVisibleRows };
        const int scClamped { juce::jmin (newScrollback, maxSc) };
        const int rowsToSkip { totalOutputRows - (scClamped + newVisibleRows) };

        writeReflowedContent (wp, tempCells.get(), tempGraphs.get(),
                              newBuffer.cells.get(), newBuffer.graphemes.get(), newBuffer.rowStates.get(),
                              newCols, newBuffer.totalRows, rowsToSkip);

        const int written { scClamped + newVisibleRows };
        newBuffer.head = ((written - 1) % newBuffer.totalRows + newBuffer.totalRows) % newBuffer.totalRows;
        newBuffer.scrollbackUsed = scClamped;
    }
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Terminal
