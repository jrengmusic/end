/**
 * @file Grid.h
 * @brief Ring-indexed terminal frame buffer — the AudioBuffer of the terminal.
 *
 * Grid is dumb storage. Processor tells Grid everything. Grid uses values as calculation
 * inputs. Grid never reads or writes State.
 *
 * ### AudioBuffer analogy
 * | AudioBuffer            | Grid                              | Role                     |
 * |------------------------|-----------------------------------|--------------------------|
 * | getWritePointer(ch)    | getWritePointer(screen, row)      | Writable viewport row    |
 * | getReadPointer(ch)     | getRow(screen, absoluteIndex)     | Absolute row read access |
 * | clear()                | clear(screen)                     | Zero viewport            |
 * | setSize()              | setSize()                         | Allocate / resize        |
 *
 * ### Memory layout
 * One `jam::Buffer<jam::Row>` with 2 channels (normal=0, alternate=1). Ring-sized to the
 * next power of two >= `(scrollbackLines + viewportRows) * 2`.
 *
 * ### Ring index model
 * `head[screen]` is the physical position of viewport row 0.
 * History grows BEHIND head in the ring.
 *
 * Viewport-relative: `physicalRow(screen, row) = (head[screen] + row) & ringMask`
 * Absolute (history): `(head[screen] - numRows[screen] + absoluteIndex) & ringMask`
 *
 * Full-screen scrollUp: advance `head` by count, clear new bottom viewport row(s). O(1).
 * The old viewport-top row stays in place — it becomes history behind the new head.
 * Partial scroll region: `buffer.copyFrom()` row-by-row within region. No head change.
 *
 * ### numRows ownership
 * Grid owns `numRows[screen]`. It is incremented by `scrollUp` (full-screen path) and
 * reset by `clear(screen)` and `setSize`. `reflow` updates it and also returns the values
 * so Processor can sync State. Processor never writes numRows — it reads via `getNumRows`.
 *
 * ### Thread model
 * - Video writes via `getWritePointer` / `scrollUp` / `scrollDown` / `clear` — READER THREAD.
 * - Screen reads via `getRow` — MESSAGE THREAD (after State flush).
 * - `setSize` / `reflow` — called from Processor on READER THREAD.
 *
 * @see Video    — sole viewport writer (reader thread)
 * @see Screen   — sole history reader (message thread)
 * @see Processor — Grid lifecycle manager; reads numRows via getNumRows to sync State
 */

#pragma once

#include <JuceHeader.h>

namespace terminal
{
/*____________________________________________________________________________*/

class Grid
{
public:
    Grid() = default;

    //==========================================================================
    /** @name Told — Processor tells Grid what to do */
    ///@{

    /** Allocates or resizes the grid.
     *
     *  Ring size = next power of two >= `(scrollbackLines + numRows) * 2`.
     *  Content is always cleared on resize — Grid is a live frame buffer, not a document.
     *
     *  @param numRows          Visible row count (logical viewport height).
     *  @param numCols          Column count per row.
     *  @param scrollbackLines  Maximum history row count from config.
     */
    void setSize (int numRows, int numCols, int scrollbackLines) noexcept;

    /** Content-preserving resize — tmux-conformant row-by-row reflow.
     *
     *  Walks all rows (history + viewport) and dispatches each to one of
     *  three operations based on usedCols vs newCols and the wrapped flag:
     *
     *  - **move**: usedCols <= newCols, not wrapped — copy row as-is.
     *  - **join**: usedCols <= newCols, wrapped — append next row(s) cells.
     *  - **split**: usedCols > newCols — break into multiple rows.
     *
     *  Writes into a scratch buffer, then copies back to the live buffer.
     *  Head and numRows are updated for the new ring layout.
     *
     *  Cursor position is converted to paragraph-relative coordinates before
     *  reflow and unwrapped back after. cursorRow and cursorCol are modified
     *  in place to reflect the post-reflow position.
     *
     *  @param newViewportRows  New visible row count.
     *  @param newCols          New column count.
     *  @param scrollbackLines  Maximum history row count from config.
     *  @param cursorRow        Viewport-relative cursor row (modified in place).
     *  @param cursorCol        Cursor column (modified in place).
     *  @return New numRows per screen {normal, alternate} — Grid sets these internally;
     *          Processor reads the return to sync State.
     */
    std::array<int, 2> reflow (int newViewportRows, int newCols, int scrollbackLines,
                               int& cursorRow, int& cursorCol) noexcept;

    /** Scrolls rows up within the given scroll region on the given screen.
     *
     *  Full-screen (scrollTop == 0 and scrollBottom == viewportRows - 1):
     *  Advances `head[screen]` by `clampedCount`. Clears the new bottom viewport row(s).
     *  Increments `numRows[screen]` per iteration, capped at `scrollbackLines`.
     *  O(1) — no data movement. The old viewport-top row stays in place behind head
     *  and becomes history.
     *
     *  Partial region: per-row `buffer.copyFrom()` within the region. Clears vacated rows.
     *
     *  @param screen        Screen index (0 = normal, 1 = alternate).
     *  @param scrollTop     First row of the scroll region (zero-based, viewport-relative).
     *  @param scrollBottom  Last row of the scroll region (zero-based, viewport-relative).
     *  @param count         Number of rows to scroll (default 1).
     *  @return              Actual number of rows scrolled (clampedCount).
     */
    int scrollUp (int screen, int scrollTop, int scrollBottom, int count = 1) noexcept;

    /** Scrolls rows down within the given scroll region on the given screen (reverse scroll).
     *
     *  Full-screen: retreats `head[screen]` by `clampedCount`. Clears the new top row(s).
     *  Partial region: per-row `buffer.copyFrom()` within the region. Clears vacated rows.
     *
     *  @param screen        Screen index (0 = normal, 1 = alternate).
     *  @param scrollTop     First row of the scroll region (zero-based, viewport-relative).
     *  @param scrollBottom  Last row of the scroll region (zero-based, viewport-relative).
     *  @param count         Number of rows to scroll (default 1).
     */
    void scrollDown (int screen, int scrollTop, int scrollBottom, int count = 1) noexcept;

    /** Clears all viewport rows of the given screen and resets numRows to 0. Does not reset head. */
    void clear (int screen) noexcept;

    /** Clears an entire viewport-relative row of the given screen. */
    void clear (int screen, int row) noexcept;

    /** Clears a range of cells within a viewport-relative row. */
    void clear (int screen, int row, int startCol, int numCols) noexcept;

    ///@}

    //==========================================================================
    /** @name Asked — storage access only */
    ///@{

    /** Returns a writable pointer to the given viewport-relative row.
     *
     *  Physical = `(head[screen] + row) & ringMask`.
     *  Video writes cells via `rowPtr->cells[col]`.
     *
     *  @param screen  Screen index (0 = normal, 1 = alternate).
     *  @param row     Viewport-relative row (0 = top of viewport).
     */
    jam::Row* getWritePointer (int screen, int row) noexcept;

    /** Returns a read-only pointer to the given absolute logical row.
     *
     *  Absolute index 0 = oldest history.
     *  Physical = `(head[screen] - numRows[screen] + absoluteIndex) & ringMask`.
     *  Used by Screen in scroll mode to read history rows.
     *
     *  @param screen         Screen index (0 = normal, 1 = alternate).
     *  @param absoluteIndex  Absolute logical row (0 = oldest history, numRows-1 = newest history,
     *                        numRows = viewport top).
     */
    const jam::Row* getRow (int screen, int absoluteIndex) const noexcept;

    /** Returns true if the grid buffer has been allocated (setSize called). */
    bool isAllocated() const noexcept;

    /** Returns the history row count for the given screen.
     *  Processor reads this after reflow to sync State.
     *
     *  @param screen  Screen index (0 = normal, 1 = alternate).
     */
    int getNumRows (int screen) const noexcept;

    /** Returns a non-owning Block<Row> view of the viewport or history region.
     *
     *  Grid owns the pointer array (blockPointers). Block borrows from it.
     *  Lifetime: Grid outlives Screen — all MESSAGE thread.
     *
     *  @param screen         Screen index (0 = normal, 1 = alternate).
     *  @param scrollOffset   0 = live viewport, >0 = history offset.
     *  @param viewportRows   Number of rows in the view.
     *  @return Non-owning Block<Row> over the requested region.
     */
    jam::Block<jam::Row> getBlock (int screen, int scrollOffset, int viewportRows) const noexcept;

    ///@}

private:
    //==========================================================================

    /** Maps a viewport-relative row to a physical ring position. */
    int physicalRow (int screen, int row) const noexcept;

    //==========================================================================

    jam::Buffer<jam::Row> buffer;                           ///< 2 channels: normal (0), alternate (1). Ring-sized.
    std::array<int, 2> head { 0, 0 };                      ///< Physical position of viewport row 0 per screen.
    std::array<int, 2> numRows { 0, 0 };                   ///< History row count per screen. Calculation input for getRow.
    int ringMask { 0 };                                    ///< Power-of-two bitmask for ring indexing.
    int viewportRows { 0 };                                ///< Visible row count.
    int scrollbackLines { 0 };                             ///< Maximum history row count from config.
    mutable juce::HeapBlock<const jam::Row*> blockPointers; ///< Logical-order row pointer array for getBlock. Grid-owned.

    //==========================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Grid)
};

/**______________________________END OF NAMESPACE______________________________*/
} // namespace terminal
