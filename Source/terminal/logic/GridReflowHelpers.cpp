/**
 * @file GridReflowHelpers.cpp
 * @brief File-local reflow helpers and Grid::reflow() private method.
 *
 * Contains all static file-local functions used by the reflow algorithm, the
 * WalkParams parameter bundle, and the Grid::reflow() private method that
 * orchestrates the four-pass reflow.  The public entry point Grid::resize()
 * lives in GridReflow.cpp and calls Grid::reflow() across translation units
 * via the normal linker resolution of Grid member functions.
 *
 * @see GridReflow.cpp — Grid::resize(), the public entry point.
 * @see Grid.h         — class declaration, ring-buffer layout, thread ownership table.
 * @see Cell           — 16-byte terminal cell type.
 * @see RowState       — per-row metadata; `isWrapped()` / `setWrapped()` drive the reflow walk.
 * @see State          — atomic terminal parameter store (new dimensions read here).
 */

#include "Grid.h"
#include "../data/State.h"

namespace Terminal
{ /*____________________________________________________________________________*/

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
 * @param linkIds        Flat linkId array of the old buffer.
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
 * @param tempLinkIds    Output flat linkId buffer; same size requirement.
 * @return Number of non-blank cells at the start of the flat buffer (trailing
 *         blanks are excluded).  Returns 0 if the logical line is entirely blank.
 */
static int flattenLogicalLine (const Cell* cells, const Grapheme* graphemes, const uint16_t* linkIds,
                               int oldCols, int totalRows, int head,
                               int oldVisibleRows, int scrollbackUsed,
                               int startLinear, int lineCount,
                               Cell* tempCells, Grapheme* tempGraphs, uint16_t* tempLinkIds) noexcept
{
    for (int r { 0 }; r < lineCount; ++r)
    {
        const int phys { linearToPhysical (head, totalRows, oldVisibleRows, scrollbackUsed, startLinear + r) };
        const int srcOff { phys * oldCols };
        const int dstOff { r * oldCols };
        std::memcpy (tempCells + dstOff, cells + srcOff, static_cast<size_t> (oldCols) * sizeof (Cell));
        std::memcpy (tempGraphs + dstOff, graphemes + srcOff, static_cast<size_t> (oldCols) * sizeof (Grapheme));
        std::memcpy (tempLinkIds + dstOff, linkIds + srcOff, static_cast<size_t> (oldCols) * sizeof (uint16_t));
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
 * @param dstCells      Flat cell array of the new buffer.
 * @param dstGraphemes  Flat grapheme array of the new buffer.
 * @param dstLinkIds    Flat linkId array of the new buffer.
 * @param dstStates     RowState array of the new buffer (indexed by physical row).
 * @param newCols       Column count of the new buffer.
 * @param writePhys     Physical row index in the new buffer to write into.
 * @param src           Source flat cell array (from `flattenLogicalLine()`).
 * @param srcG          Source flat grapheme array.
 * @param srcL          Source flat linkId array.
 * @param srcOffset     Offset (in cells) into `src` / `srcG` / `srcL` for this output row.
 * @param cellCount     Number of content cells to copy (may be less than `newCols`).
 * @param wrapped       `true` if this output row is followed by another row from
 *                      the same logical line (i.e. the logical line continues).
 */
static void writeNewRow (Cell* dstCells, Grapheme* dstGraphemes, uint16_t* dstLinkIds,
                         RowState* dstStates,
                         int newCols, int writePhys,
                         const Cell* src, const Grapheme* srcG, const uint16_t* srcL,
                         int srcOffset, int cellCount, bool wrapped) noexcept
{
    const int dstOff { writePhys * newCols };
    const int toCopy { juce::jmin (cellCount, newCols) };

    std::memcpy (dstCells + dstOff,     src + srcOffset,  static_cast<size_t> (toCopy) * sizeof (Cell));
    std::memcpy (dstGraphemes + dstOff, srcG + srcOffset, static_cast<size_t> (toCopy) * sizeof (Grapheme));
    std::memcpy (dstLinkIds + dstOff,   srcL + srcOffset, static_cast<size_t> (toCopy) * sizeof (uint16_t));

    if (toCopy < newCols)
    {
        const Cell empty {};
        std::fill (dstCells + dstOff + toCopy,     dstCells + dstOff + newCols,     empty);
        std::fill (dstGraphemes + dstOff + toCopy, dstGraphemes + dstOff + newCols, Grapheme {});
        std::fill (dstLinkIds + dstOff + toCopy,   dstLinkIds + dstOff + newCols,   uint16_t { 0 });
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

    /** Flat linkId array of the old buffer. */
    const uint16_t* linkIds;

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

    /** Total number of linear rows to walk (scrollbackUsed + oldVisibleRows). */
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
 * @param tempLinkIds        Scratch buffer for `flattenLogicalLine()`; same size.
 * @param newCols            Column count of the new buffer.
 * @param cursorLinear       Linear row index of the cursor in the old buffer.
 * @param cursorCol          Column of the cursor in the old buffer.
 * @param outCursorOutputRow Output: output row the cursor lands on, or -1 if not found.
 * @param outCursorNewCol    Output: column of the cursor after rewrapping.
 * @param dstCells           New buffer cell array, or `nullptr` for dry-run mode.
 * @param dstGraphemes       New buffer grapheme array, or `nullptr` for dry-run mode.
 * @param dstLinkIds         New buffer linkId array, or `nullptr` for dry-run mode.
 * @param dstStates          New buffer RowState array, or `nullptr` for dry-run mode.
 * @param newTotalRows       Total allocated rows in the new buffer (write mode only).
 * @param rowsToSkip         Rows at the top of output to skip (write mode only).
 * @return Total number of output rows the reflowed content occupies.
 */
static int reflowPass (const WalkParams& wp, Cell* tempCells, Grapheme* tempGraphs, uint16_t* tempLinkIds,
                       int newCols,
                       int cursorLinear, int cursorCol,
                       int* outCursorOutputRow, int* outCursorNewCol,
                       Cell* dstCells, Grapheme* dstGraphemes, uint16_t* dstLinkIds,
                       RowState* dstStates,
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

        const int flatLen { flattenLogicalLine (wp.cells, wp.graphemes, wp.linkIds,
                                                wp.oldCols, wp.totalRows,
                                                wp.head, wp.oldVisibleRows, wp.scrollbackUsed,
                                                r, runLen, tempCells, tempGraphs, tempLinkIds) };

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

                    writeNewRow (dstCells, dstGraphemes, dstLinkIds, dstStates, newCols,
                                 writePhys, tempCells, tempGraphs, tempLinkIds, offset, count, wrapped);
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
 * described in the file header of GridReflow.cpp.
 *
 * @par Step 1 — Bound the content
 * All visible rows and scrollback are included: `linearRows = scrollbackUsed + oldVisibleRows`.
 * Blank trailing rows are preserved — reflow never drops content.
 * If `linearRows` is 0 (no rows at all), the function returns immediately.
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
    const int linearRows { scrollbackUsed + oldVisibleRows };

    if (linearRows > 0)
    {
        const WalkParams wp { oldBuffer.cells.get(), oldBuffer.graphemes.get(),
                              oldBuffer.linkIds.get(),
                              oldBuffer.rowStates.get(),
                              oldBuffer.head, oldBuffer.totalRows, oldCols, oldVisibleRows,
                              scrollbackUsed, linearRows };

        const int maxFlatLen { linearRows * oldCols };
        juce::HeapBlock<Cell> tempCells (static_cast<size_t> (maxFlatLen));
        juce::HeapBlock<Grapheme> tempGraphs (static_cast<size_t> (maxFlatLen));
        juce::HeapBlock<uint16_t> tempLinkIds (static_cast<size_t> (maxFlatLen));

        const int cursorRow { state.getRawValue<int> (state.screenKey (normal, Terminal::ID::cursorRow)) };
        const int cursorCol { state.getRawValue<int> (state.screenKey (normal, Terminal::ID::cursorCol)) };
        const int cursorLinear { scrollbackUsed + cursorRow };

        // Pass 1: count output rows (dry run — no writing)
        int cursorOutputRowDry { -1 };
        int cursorNewColDry { 0 };

        const int totalOutputRows { reflowPass (wp, tempCells.get(), tempGraphs.get(), tempLinkIds.get(),
                                                newCols,
                                                cursorLinear, cursorCol,
                                                &cursorOutputRowDry, &cursorNewColDry,
                                                nullptr, nullptr, nullptr, nullptr, 0, 0) };

        const int newScrollback { juce::jmax (0, totalOutputRows - newVisibleRows) };
        const int maxSc { newBuffer.totalRows - newVisibleRows };
        const int scClamped { juce::jmin (newScrollback, maxSc) };
        const int rowsToSkip { juce::jmax (0, totalOutputRows - (scClamped + newVisibleRows)) };

        // Pass 2: write reflowed content (same function — count and write cannot disagree)
        int cursorOutputRow { -1 };
        int cursorNewCol { 0 };

        reflowPass (wp, tempCells.get(), tempGraphs.get(), tempLinkIds.get(),
                    newCols,
                    cursorLinear, cursorCol,
                    &cursorOutputRow, &cursorNewCol,
                    newBuffer.cells.get(), newBuffer.graphemes.get(), newBuffer.linkIds.get(),
                    newBuffer.rowStates.get(),
                    newBuffer.totalRows, rowsToSkip);

        jassert (cursorOutputRow >= 0);

        const int written { scClamped + newVisibleRows };
        newBuffer.head = ((written - 1) % newBuffer.totalRows + newBuffer.totalRows) % newBuffer.totalRows;
        newBuffer.scrollbackUsed = scClamped;

        const int newCursorVisibleRow { cursorOutputRow - rowsToSkip - scClamped };
        state.setCursorRow (normal, juce::jlimit (0, newVisibleRows - 1, newCursorVisibleRow));
        state.setCursorCol (normal, juce::jlimit (0, newCols - 1, cursorNewCol));
    }
}

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Terminal
