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
 * ### TETRIS contract
 * Processor sets, Grid uses. `numRows[screen]` is a calculation input set by Processor
 * after scroll events — same pattern as DSP core storing `sampleRate` from `prepareToPlay`.
 * Grid only uses numRows for absolute history access (getRow). Viewport-relative operations
 * (getWritePointer, clear, scrollUp) use head only.
 *
 * ### Thread model
 * - Video writes via `getWritePointer` / `scrollUp` / `scrollDown` / `clear` — READER THREAD.
 * - Screen reads via `getRow` — MESSAGE THREAD (after State flush).
 * - `setSize` / `setNumRows` — called from Processor on READER THREAD.
 *
 * @see Video    — sole viewport writer (reader thread)
 * @see Screen   — sole history reader (message thread)
 * @see Processor — sole arbiter of numRows and Grid lifecycle
 */

#pragma once

#include <JuceHeader.h>

namespace Terminal
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

    /** Sets the history row count for the given screen.
     *
     *  Processor calls this after `scrollUp` events to update the calculation input.
     *  Only used by `getRow()` for absolute history access.
     *
     *  @param screen  Screen index (0 = normal, 1 = alternate).
     *  @param value   New history row count (capped at `scrollbackLines` by Processor).
     */
    void setNumRows (int screen, int value) noexcept;

    /** Scrolls rows up within the given scroll region on the given screen.
     *
     *  Full-screen (scrollTop == 0 and scrollBottom == viewportRows - 1):
     *  Advances `head[screen]` by `clampedCount`. Clears the new bottom viewport row(s).
     *  O(1) — no data movement. The old viewport-top row stays in place behind head
     *  and becomes history (Processor manages numRows).
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

    /** Clears all viewport rows of the given screen. Does not reset head or numRows. */
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

    ///@}

private:
    //==========================================================================

    /** Maps a viewport-relative row to a physical ring position. */
    int physicalRow (int screen, int row) const noexcept;

    //==========================================================================

    jam::Buffer<jam::Row> buffer;           ///< 2 channels: normal (0), alternate (1). Ring-sized.
    std::array<int, 2> head { 0, 0 };       ///< Physical position of viewport row 0 per screen.
    std::array<int, 2> numRows { 0, 0 };    ///< History row count per screen. Calculation input for getRow.
    int ringMask { 0 };                     ///< Power-of-two bitmask for ring indexing.
    int viewportRows { 0 };                 ///< Visible row count.
    int scrollbackLines { 0 };              ///< Maximum history row count from config.

    //==========================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Grid)
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Terminal
