/**
 * @file GridReflow.cpp
 * @brief Grid::resize() — public entry point for terminal resize and content reflow.
 *
 * This file implements `Grid::resize()`, the public resize entry point called
 * from the message thread.  The reflow algorithm itself (file-local helpers,
 * WalkParams, and Grid::reflow()) lives in GridReflowHelpers.cpp.
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
 * @see GridReflowHelpers.cpp — file-local helpers and Grid::reflow().
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
 * changed, no work is performed.
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
            std::memcpy (newAlternate.linkIds.get() + newPhys * newCols,
                         oldAlt.linkIds.get() + oldPhys * oldCols,
                         static_cast<size_t> (copyCols) * sizeof (uint16_t));
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

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Terminal
