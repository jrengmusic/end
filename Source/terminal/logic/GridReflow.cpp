/**
 * @file GridReflow.cpp
 * @brief Grid member functions for terminal resize and content reflow.
 *
 * This file implements `Grid::resize()` and the reflow algorithm that
 * re-wraps soft-wrapped lines when the terminal column width changes.
 * All public entry points are called on the **MESSAGE THREAD** under
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
 * `reflowPass()` (dry-run mode: `dstCells == nullptr`) walks every logical
 * line in the old buffer (following soft-wrap chains via `nextLogicalLine()`),
 * flattens each logical line into a temporary buffer via
 * `flattenLogicalLine()`, and counts how many new rows each logical line will
 * occupy at the new column width.  This total is used to compute how many rows
 * to skip at the top so that the new buffer's scrollback does not overflow.
 *
 * @par Pass 3 — Write reflowed content
 * `reflowPass()` (write mode: `dstCells != nullptr`) runs the identical
 * logical-line walk and writes each re-wrapped row into the new buffer via
 * `writeNewRow()`.  Rows that would overflow the new buffer's total capacity
 * are skipped (counted by `rowsToSkip`).  Because both passes call the same
 * function body, count and write can never disagree — SSOT.
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
 * Called from the message thread.  Acquires `resizeLock` to synchronise with
 * the reader thread's data processing.  If neither `cols` nor `visibleRows`
 * changed, the function returns immediately.
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
 * @param newCols        New terminal width in character columns.
 * @param newVisibleRows New terminal height in character rows.
 * @note MESSAGE THREAD — acquires `resizeLock`.  Allocates heap memory.
 * @see reflow(), initBuffer(), markAllDirty()
 */
void Grid::resize (int newCols, int newVisibleRows)
{
    juce::ScopedLock lock (resizeLock);

    const int oldCols { buffers.at (normal).allocatedCols };
    const int oldVisibleRows { buffers.at (normal).allocatedVisibleRows };

    const bool dimensionsChanged { newCols != oldCols or newVisibleRows != oldVisibleRows };

    const bool heightOnly { dimensionsChanged and newCols == oldCols
                            and newVisibleRows <= buffers.at (normal).totalRows
                            and newVisibleRows <= buffers.at (alternate).totalRows };

    if (heightOnly)
    {
        // Fast path: columns unchanged — no reflow needed, just adjust metadata.
        Buffer& buffer { buffers.at (normal) };
        const int heightDelta { newVisibleRows - oldVisibleRows };
        const int maxScrollback { buffer.totalRows - newVisibleRows };
        const int cursorRow { state.getRawValue<int> (state.screenKey (normal, Terminal::ID::cursorRow)) };
        const bool pinToBottom { cursorRow >= oldVisibleRows - 1 };

        if (pinToBottom)
        {
            // Viewport expands from top: head stays, reveal more scrollback above
            buffer.scrollbackUsed = juce::jlimit (0, maxScrollback, buffer.scrollbackUsed - heightDelta);
            buffer.allocatedVisibleRows = newVisibleRows;
            state.setCursorRow (normal, juce::jlimit (0, newVisibleRows - 1, cursorRow + heightDelta));
        }
        else
        {
            // Viewport expands from bottom: content stays at top, empty space below
            buffer.head = (buffer.head + heightDelta) & buffer.rowMask;
            buffer.allocatedVisibleRows = newVisibleRows;
            // cursorRow unchanged — content stays at same visible position
        }

        // Adjust alternate buffer metadata (no scrollback, no reflow)
        Buffer& altBuffer { buffers.at (alternate) };
        altBuffer.allocatedVisibleRows = newVisibleRows;

        const int altCursorRow { state.getRawValue<int> (state.screenKey (alternate, Terminal::ID::cursorRow)) };
        state.setCursorRow (alternate, juce::jlimit (0, newVisibleRows - 1, altCursorRow));

        state.setScrollbackUsed (buffer.scrollbackUsed);
        markAllDirty();
        state.setFullRebuild();
    }
    else if (dimensionsChanged)
    {
        // 1. Reflow normal buffer (always has scrollback + wrap chains)
        Buffer newPrimary;
        initBuffer (newPrimary, newCols, newVisibleRows + scrollbackCapacity, newVisibleRows);
        reflow (buffers.at (normal), oldCols, oldVisibleRows, newPrimary, newCols, newVisibleRows);
        buffers.at (normal) = std::move (newPrimary);

        // 2. Resize alternate buffer preserving content (no scrollback, no reflow)
        Buffer newAlternate;
        initBuffer (newAlternate, newCols, newVisibleRows, newVisibleRows);

        const int copyRows { juce::jmin (oldVisibleRows, newVisibleRows) };
        const int copyCols { juce::jmin (oldCols, newCols) };
        const Buffer& oldAlt { buffers.at (alternate) };

        for (int r { 0 }; r < copyRows; ++r)
        {
            const int oldPhys { (oldAlt.head - oldVisibleRows + 1 + r + oldAlt.totalRows) & oldAlt.rowMask };
            const int newPhys { r };

            std::memcpy (newAlternate.cells.get() + newPhys * newCols,
                         oldAlt.cells.get() + oldPhys * oldCols,
                         static_cast<size_t> (copyCols) * sizeof (Cell));
            std::memcpy (newAlternate.graphemes.get() + newPhys * newCols,
                         oldAlt.graphemes.get() + oldPhys * oldCols,
                         static_cast<size_t> (copyCols) * sizeof (Grapheme));
            newAlternate.rowStates[newPhys] = oldAlt.rowStates[oldPhys];
        }

        newAlternate.head = copyRows > 0 ? copyRows - 1 : 0;
        newAlternate.scrollbackUsed = 0;
        buffers.at (alternate) = std::move (newAlternate);

        // Clamp alternate cursor to new dimensions
        const int altCursorRow { state.getRawValue<int> (state.screenKey (alternate, Terminal::ID::cursorRow)) };
        const int altCursorCol { state.getRawValue<int> (state.screenKey (alternate, Terminal::ID::cursorCol)) };
        state.setCursorRow (alternate, juce::jlimit (0, newVisibleRows - 1, altCursorRow));
        state.setCursorCol (alternate, juce::jlimit (0, newCols - 1, altCursorCol));

        state.setScrollbackUsed (buffers.at (normal).scrollbackUsed);
        markAllDirty();
        state.setFullRebuild();
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
    const int mask { totalRows - 1 };
    const int visibleRow { linearRow - scrollbackUsed };
    return (head - oldVisibleRows + 1 + visibleRow + totalRows) & mask;
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
    int result { -1 };

    for (int row { oldVisibleRows - 1 }; row >= 0 and result < 0; --row)
    {
        const int linear { scrollbackUsed + row };
        const int phys { linearToPhysical (head, totalRows, oldVisibleRows, scrollbackUsed, linear) };
        const Cell* rowCells { cells + phys * oldCols };

        for (int col { oldCols - 1 }; col >= 0 and result < 0; --col)
        {
            if (rowCells[col].hasContent())
            {
                result = row;
            }
        }
    }

    return result;
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

// ============================================================================
// Logical line walk — single pass (count + write)
// ============================================================================

/**
 * @struct WalkParams
 * @brief Read-only parameters shared by the logical-line walk functions.
 *
 * Bundles all the old-buffer geometry needed by `nextLogicalLine()` and
 * `reflowPass()` into a single struct to avoid long parameter lists.
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
 * @brief Single-pass reflow: counts output rows, tracks cursor, and optionally
 *        writes reflowed content into the new buffer.
 *
 * Walks every logical line in the old buffer using `nextLogicalLine()` and
 * `flattenLogicalLine()`.  Both the count pass and the write pass call this
 * function — SSOT guarantee: count and write can never disagree.
 *
 * @par Dry-run mode (count only)
 * When `dstCells == nullptr`, the function counts output rows and tracks the
 * cursor position without writing anything.  Used for Pass 2.
 *
 * @par Write mode (count + write)
 * When `dstCells != nullptr`, the function counts output rows, tracks the
 * cursor, and writes reflowed rows into the new buffer via `writeNewRow()`,
 * skipping rows before `rowsToSkip`.  Used for Pass 3.
 *
 * @par Cursor tracking
 * If `cursorLinear` falls within a logical line `[r, r + runLen)`, the
 * function computes which output row and column the cursor lands on after
 * rewrapping.  `*outCursorOutputRow` is initialised to -1; if the cursor was
 * not found in any logical line it remains -1 on return.
 *
 * @param wp                 Walk parameters for the old buffer.
 * @param tempCells          Scratch buffer for `flattenLogicalLine()`; must hold at
 *                           least `wp.linearRows * wp.oldCols` Cell entries.
 * @param tempGraphs         Scratch buffer for `flattenLogicalLine()`; same size.
 * @param newCols            Column count of the new buffer.
 * @param cursorLinear       Linear row index of the cursor in the old buffer.
 * @param cursorCol          Column of the cursor in the old buffer.
 * @param outCursorOutputRow Output: output row the cursor lands on, or -1 if not found.
 * @param outCursorNewCol    Output: column of the cursor after rewrapping.
 * @param dstCells           New buffer cell array, or `nullptr` for dry-run mode.
 * @param dstGraphemes       New buffer grapheme array, or `nullptr` for dry-run mode.
 * @param dstStates          New buffer RowState array, or `nullptr` for dry-run mode.
 * @param newTotalRows       Total allocated rows in the new buffer (write mode only).
 * @param rowsToSkip         Rows at the top of output to skip (write mode only).
 * @return Total number of output rows the reflowed content occupies.
 */
static int reflowPass (const WalkParams& wp, Cell* tempCells, Grapheme* tempGraphs,
                       int newCols,
                       int cursorLinear, int cursorCol,
                       int* outCursorOutputRow, int* outCursorNewCol,
                       Cell* dstCells, Grapheme* dstGraphemes, RowState* dstStates,
                       int newTotalRows, int rowsToSkip) noexcept
{
    *outCursorOutputRow = -1;
    int total { 0 };
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

        if (*outCursorOutputRow < 0 and cursorLinear >= r and cursorLinear < r + runLen)
        {
            const int flatCursorOffset { (cursorLinear - r) * wp.oldCols + cursorCol };
            const int maxOffset { flatLen - 1 };
            const int clampedOffset { juce::jmin (flatCursorOffset, juce::jmax (maxOffset, 0)) };
            *outCursorOutputRow = total + clampedOffset / newCols;
            *outCursorNewCol = clampedOffset % newCols;
        }

        const int effectiveLen { flatLen };
        const int rowsNeeded { (effectiveLen > 0) ? (effectiveLen + newCols - 1) / newCols : 1 };

        if (dstCells != nullptr)
        {
            for (int row { 0 }; row < rowsNeeded; ++row)
            {
                if (outputRowsSoFar + row >= rowsToSkip)
                {
                    const int offset { row * newCols };
                    const int remaining { effectiveLen - offset };
                    const int count { juce::jmax (0, juce::jmin (remaining, newCols)) };
                    const bool wrapped { row < rowsNeeded - 1 };

                    writeNewRow (dstCells, dstGraphemes, dstStates, newCols,
                                 writePhys, tempCells, tempGraphs, offset, count, wrapped);
                    writePhys = (writePhys + 1) % newTotalRows;
                }
            }

            outputRowsSoFar += rowsNeeded;
        }

        total += rowsNeeded;
        r += runLen;
    }

    return total;
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
 * `reflowPass()` (dry-run mode) computes the total number of output rows.
 * From this, the function computes:
 * - `newScrollback` — how many rows of scrollback the reflowed content would
 *   produce.
 * - `scClamped` — `newScrollback` clamped to the new buffer's scrollback
 *   capacity (`newBuffer.totalRows - newVisibleRows`).
 * - `rowsToSkip` — rows at the top of the output to discard so that the
 *   written content fits exactly into `scClamped + newVisibleRows` rows.
 *
 * @par Step 4 — Write pass
 * `reflowPass()` (write mode) writes the reflowed rows into `newBuffer`,
 * starting at physical row 0 and advancing the write head modulo
 * `newBuffer.totalRows`.  The identical function body guarantees count and
 * write cannot disagree.
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
 * @note MESSAGE THREAD — allocates temporary heap memory for scratch buffers.
 * @see resize(), reflowPass()
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

        const int cursorRow { state.getRawValue<int> (state.screenKey (normal, Terminal::ID::cursorRow)) };
        const int cursorCol { state.getRawValue<int> (state.screenKey (normal, Terminal::ID::cursorCol)) };
        const int cursorLinear { scrollbackUsed + cursorRow };

        // Pass 1: count output rows (dry run — no writing)
        int cursorOutputRowDry { -1 };
        int cursorNewColDry { 0 };

        const int totalOutputRows { reflowPass (wp, tempCells.get(), tempGraphs.get(), newCols,
                                                cursorLinear, cursorCol,
                                                &cursorOutputRowDry, &cursorNewColDry,
                                                nullptr, nullptr, nullptr, 0, 0) };

        const int newScrollback { juce::jmax (0, totalOutputRows - newVisibleRows) };
        const int maxSc { newBuffer.totalRows - newVisibleRows };
        const int scClamped { juce::jmin (newScrollback, maxSc) };
        const int rowsToSkip { juce::jmax (0, totalOutputRows - (scClamped + newVisibleRows)) };

        // Pass 2: write reflowed content (same function — count and write cannot disagree)
        int cursorOutputRow { -1 };
        int cursorNewCol { 0 };

        reflowPass (wp, tempCells.get(), tempGraphs.get(), newCols,
                    cursorLinear, cursorCol,
                    &cursorOutputRow, &cursorNewCol,
                    newBuffer.cells.get(), newBuffer.graphemes.get(), newBuffer.rowStates.get(),
                    newBuffer.totalRows, rowsToSkip);

        if (cursorOutputRow < 0)
        {
            cursorOutputRow = totalOutputRows - 1;
            cursorNewCol = cursorCol;
        }

        const int written { scClamped + newVisibleRows };
        newBuffer.head = ((written - 1) % newBuffer.totalRows + newBuffer.totalRows) % newBuffer.totalRows;
        newBuffer.scrollbackUsed = scClamped;

        const int newCursorVisibleRow { cursorOutputRow - rowsToSkip - scClamped };
        state.setCursorRow (normal, juce::jlimit (0, newVisibleRows - 1, newCursorVisibleRow));

        state.setCursorCol (normal, juce::jlimit (0, newCols - 1, cursorNewCol));
    }
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Terminal
