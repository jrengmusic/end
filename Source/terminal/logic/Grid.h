/**
 * @file Grid.h
 * @brief Live frame buffer of jam::Cell records — the AudioBuffer of the terminal.
 *
 * Grid is a dumb ring buffer. Parser writes cells in-place on the reader thread.
 * Display reads dirty rows on the message thread via VBlank.
 *
 * ### AudioBuffer analogy
 * | AudioBuffer          | Grid                        | Role                       |
 * |----------------------|-----------------------------|----------------------------|
 * | getReadPointer(ch)   | getReadPointer(screen, row) | Read-only row access       |
 * | getWritePointer(ch)  | getWritePointer(screen, row)| Writable row access        |
 * | clear()              | clear(screen)               | Zero the buffer            |
 * | hasBeenCleared()     | hasBeenCleared(screen)      | Optimisation gate          |
 * | setSize()            | setSize()                   | Allocate / resize          |
 *
 * ### Memory layout
 * One jam::Buffer<jam::Cell> with 2 channels (normal=0, alternate=1), sized to the next
 * power of two >= viewportRows. The extra rows above viewportRows form the staging area:
 * rows evicted by scrollUp accumulate there until Display drains them under lock.
 *
 * ### Ring index
 * Each screen (channel) has an independent head[screen] ring index.
 * physicalRow(screen, logicalRow) = (head[screen] + logicalRow) & ringMask.
 * Full-screen scrollUp/scrollDown advances/retreats head — O(1), no data movement.
 * Partial scroll regions use row-by-row copyFrom within the affected logical range.
 *
 * ### Thread model
 * - Parser writes via getWritePointer / scrollUp / scrollDown / clear — READER THREAD.
 * - Display reads via getReadPointer / getStagingReadPointer — MESSAGE THREAD.
 * - setSize — called from READER THREAD (Processor::process, deferred via atomic flag).
 * - State's atomic pattern handles all coordination — no lock in Grid.
 *
 * ### Staging area
 * scrolledCount[screen] tracks how many rows have been evicted since the last Display drain.
 * Display reads getStagingReadPointer() for each evicted row, then calls resetScrolledCount().
 * The effective staging depth is clamped to min(scrolledCount, ringSize - viewportRows)
 * to guard against overwrite if the ring wraps before Display drains.
 *
 * ### Screen parameter
 * Callers pass the active screen index (0 = normal, 1 = alternate) to every call,
 * mirroring AudioBuffer where the caller passes the channel index. Grid holds no
 * active-screen state — the caller (Video) is the authority on which screen is live.
 *
 * @see Parser   — sole writer (reader thread)
 * @see Display  — sole reader (message thread)
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
    /** @name Size — mirrors AudioBuffer::setSize / getNumChannels / getNumSamples */
    ///@{

    /** Allocates or resizes the grid.
     *
     *  Ring size is rounded up to the next power of two >= numRows.
     *  Content is always cleared on resize — Grid is a live frame buffer, not a document.
     *
     *  @param numRows  Visible row count (logical viewport height).
     *  @param numCols  Column count per row.
     */
    void setSize (int numRows, int numCols) noexcept;

    /** Returns the logical viewport row count (pre-power-of-two rounding). */
    int getNumRows (int screen) const noexcept;

    /** Returns the column count. */
    int getNumCols (int screen) const noexcept;

    /** Returns the ring size (power of two). */
    int getRingSize() const noexcept;

    ///@}

    //==========================================================================
    /** @name Pointer access — mirrors AudioBuffer::getReadPointer / getWritePointer */
    ///@{

    /** Returns a read-only pointer to the first Cell in the given logical row on the given screen
     *  (0 = normal, 1 = alternate). Translates logical row through the ring head. */
    const jam::Cell* getReadPointer (int screen, int row) const noexcept;

    /** Returns a read-only pointer to a specific Cell position on the given screen
     *  (0 = normal, 1 = alternate). Translates logical row through the ring head. */
    const jam::Cell* getReadPointer (int screen, int row, int col) const noexcept;

    /** Returns a writable pointer to the first Cell in the given logical row on the given screen.
     *  Translates logical row through the ring head. */
    jam::Cell* getWritePointer (int screen, int row) noexcept;

    /** Returns a writable pointer to a specific Cell position on the given screen.
     *  Translates logical row through the ring head. */
    jam::Cell* getWritePointer (int screen, int row, int col) noexcept;

    ///@}

    //==========================================================================
    /** @name Clear — mirrors AudioBuffer::clear / hasBeenCleared */
    ///@{

    /** Clears all rows of the given screen buffer and resets the ring head. */
    void clear (int screen) noexcept;

    /** Clears an entire logical row of the given screen buffer. */
    void clear (int screen, int row) noexcept;

    /** Clears a range of cells within a logical row of the given screen buffer. */
    void clear (int screen, int row, int startCol, int numCols) noexcept;

    /** Returns true if the given screen's buffer has been cleared and no writes occurred since.
     *  (0 = normal, 1 = alternate). */
    bool hasBeenCleared (int screen) const noexcept;

    ///@}

    //==========================================================================
    /** @name Scroll — terminal-specific */
    ///@{

    /** Scrolls rows up within the given scroll region on the given screen.
     *
     *  Full-screen (scrollTop == 0 and scrollBottom == viewportRows - 1):
     *  clears the physical row at head[screen] (the recycled bottom), advances head — O(1).
     *  Increments scrolledCount[screen] so Display knows how many rows to drain.
     *
     *  Partial region: per-row copyFrom within the region.
     *
     *  @param screen        Screen index (0 = normal, 1 = alternate).
     *  @param scrollTop     First row of the scroll region (zero-based, logical).
     *  @param scrollBottom  Last row of the scroll region (zero-based, logical).
     *  @param count         Number of rows to scroll (default 1).
     *  @return              Number of rows scrolled (clampedCount). Display uses this
     *                       as a hint; authoritative count is getScrolledCount().
     */
    int scrollUp (int screen, int scrollTop, int scrollBottom, int count = 1) noexcept;

    /** Scrolls rows down within the given scroll region on the given screen (reverse scroll).
     *
     *  Full-screen: retreats the ring head — O(1). No scrolledCount change.
     *  Partial region: per-row copyFrom within the region.
     *  Clears the new top row(s). No staging capture.
     *
     *  @param screen        Screen index (0 = normal, 1 = alternate).
     *  @param scrollTop     First row of the scroll region (zero-based, logical).
     *  @param scrollBottom  Last row of the scroll region (zero-based, logical).
     *  @param count         Number of rows to scroll (default 1).
     */
    void scrollDown (int screen, int scrollTop, int scrollBottom, int count = 1) noexcept;

    ///@}

    //==========================================================================
    /** @name Staging — Display-side drain of evicted rows */
    ///@{

    /** Returns the number of rows scrolled off the viewport since the last Display drain. */
    int getScrolledCount (int screen) const noexcept;

    /** Resets the scrolled count after Display drains all staging rows. */
    void resetScrolledCount (int screen) noexcept;

    /** Returns a read-only pointer to a staging row (evicted row behind the viewport).
     *
     *  stagingIndex 0 = oldest evicted row, (effectiveScrolled - 1) = newest evicted row.
     *  Caller must clamp the index range to min(getScrolledCount(screen), ringSize - viewportRows).
     *
     *  Physical address: (head[screen] - effectiveScrolled + stagingIndex) & ringMask.
     */
    const jam::Cell* getStagingReadPointer (int screen, int stagingIndex) const noexcept;

    ///@}

private:
    //==========================================================================

    /** Maps a logical row to a physical row in the ring for the given screen. */
    int physicalRow (int screen, int logicalRow) const noexcept;

    //==========================================================================

    jam::Buffer<jam::Cell> cells;///< 2 channels: normal (0), alternate (1). Ring-sized (power of two).
    int head[2] { 0, 0 };///< Ring head per screen (normal=0, alternate=1).
    int ringMask { 0 };///< Power-of-two bitmask for ring indexing.
    int viewportRows { 0 };///< Logical viewport row count (pre-rounding).
    int scrolledCount[2] { 0, 0 };///< Rows scrolled since last Display drain, per channel.

    //==========================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Grid)
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Terminal
