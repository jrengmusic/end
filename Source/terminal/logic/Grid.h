/**
 * @file Grid.h
 * @brief Live frame buffer of jam::Cell records — the AudioBuffer of the terminal.
 *
 * Grid is a dumb buffer. Parser writes cells in-place on the reader thread.
 * Display reads dirty rows on the message thread via VBlank.
 *
 * ### AudioBuffer analogy
 * | AudioBuffer          | Grid                  | Role                       |
 * |----------------------|-----------------------|----------------------------|
 * | getReadPointer(ch)   | getReadPointer(row)   | Read-only row access       |
 * | getWritePointer(ch)  | getWritePointer(row)  | Writable row access        |
 * | clear()              | clear()               | Zero the buffer            |
 * | hasBeenCleared()     | hasBeenCleared()      | Optimisation gate          |
 * | setSize()            | setSize()             | Allocate / resize          |
 *
 * ### Memory layout per buffer
 * Single `HeapBlock<char, true>` (SIMD-aligned). Flat array of jam::Cell records,
 * `capacity * alignedCols` cells total. Ring-buffer indexing on the normal screen
 * (head advances on scroll, no memmove). Fixed buffer on the alternate screen.
 *
 * ### Thread model
 * - Parser writes via getWritePointer / scrollUp / scrollDown / clear — READER THREAD.
 * - Display reads via getReadPointer / consumeDirtyRows / scroll-off — MESSAGE THREAD.
 * - setSize — called from READER THREAD (Parser detects dimension change in State).
 *
 * @see Parser   — sole writer (reader thread)
 * @see Display  — sole reader (message thread)
 * @see State    — atomic parameter store (cursor, modes, dimensions)
 */

#pragma once

#include <JuceHeader.h>
#include <jam_tui/jam_tui.h>
#include <array>
#include <atomic>

namespace Terminal
{ /*____________________________________________________________________________*/

class Grid
{
public:
    Grid() = default;

    //==========================================================================
    /** @name Size — mirrors AudioBuffer::setSize / getNumChannels / getNumSamples */
    ///@{

    /** Allocates or resizes the grid.
     *
     *  @param numRows             Visible row count.
     *  @param numCols             Column count per row.
     *  @param keepExistingContent Preserve existing cell data up to the smaller of old/new dims.
     *  @param clearExtraSpace     Zero-init any newly exposed cells.
     *  @param avoidReallocating   Reuse existing allocation if large enough.
     */
    void setSize (int numRows, int numCols,
                  bool keepExistingContent = false,
                  bool clearExtraSpace = false,
                  bool avoidReallocating = false) noexcept;

    int getNumRows() const noexcept;
    int getNumCols() const noexcept;

    ///@}

    //==========================================================================
    /** @name Pointer access — mirrors AudioBuffer::getReadPointer / getWritePointer */
    ///@{

    /** Returns a read-only pointer to the first Cell in the given row. */
    const jam::Cell* getReadPointer (int row) const noexcept;

    /** Returns a read-only pointer to a specific Cell position. */
    const jam::Cell* getReadPointer (int row, int col) const noexcept;

    /** Returns a writable pointer to the first Cell in the given row.
     *  Marks the row dirty. */
    jam::Cell* getWritePointer (int row) noexcept;

    /** Returns a writable pointer to a specific Cell position.
     *  Marks the row dirty. */
    jam::Cell* getWritePointer (int row, int col) noexcept;

    ///@}

    //==========================================================================
    /** @name Element access — mirrors AudioBuffer::getSample / setSample */
    ///@{

    /** Returns the Cell at the given position (read-only). */
    const jam::Cell& getCell (int row, int col) const noexcept;

    /** Writes a Cell at the given position. Marks the row dirty. */
    void setCell (int row, int col, const jam::Cell& cell) noexcept;

    ///@}

    //==========================================================================
    /** @name Clear — mirrors AudioBuffer::clear / hasBeenCleared */
    ///@{

    /** Clears all rows of the active buffer. Sets isClear flag. */
    void clear() noexcept;

    /** Clears an entire row. */
    void clear (int row) noexcept;

    /** Clears a range of cells within a row. */
    void clear (int row, int startCol, int numCols) noexcept;

    /** Returns true if the active buffer has been cleared and no writes occurred since. */
    bool hasBeenCleared() const noexcept;

    ///@}

    //==========================================================================
    /** @name Dual screen — terminal-specific */
    ///@{

    /** Swaps the active buffer. */
    void setScreen (bool alternate) noexcept;

    /** Returns true if the alternate screen buffer is active. */
    bool isAlternateScreen() const noexcept;

    ///@}

    //==========================================================================
    /** @name Dirty tracking — reader to message thread sync */
    ///@{

    /** Atomically exchanges the dirty-row bitmask, returning the previous value.
     *  Called by Display on the message thread to discover which rows changed. */
    uint64_t consumeDirtyRows() noexcept;

    ///@}

    //==========================================================================
    /** @name Scroll — terminal-specific */
    ///@{

    /** Scrolls rows up within the given scroll region.
     *
     *  On the normal screen with a full-screen region (scrollTop == 0 and
     *  scrollBottom == numRows - 1), uses ring-buffer head advance — no memmove.
     *  Scroll-off rows are captured for Display to drain.
     *
     *  On the alternate screen or a partial region, uses memmove.
     *  No scroll-off capture.
     *
     *  Clears the new bottom row(s). Marks all rows in the region dirty.
     *
     *  @param scrollTop     First row of the scroll region (zero-based).
     *  @param scrollBottom  Last row of the scroll region (zero-based).
     *  @param count         Number of rows to scroll (default 1).
     */
    void scrollUp (int scrollTop, int scrollBottom, int count = 1) noexcept;

    /** Scrolls rows down within the given scroll region (reverse scroll).
     *
     *  Always uses memmove. No scroll-off capture. Clears the new top row(s).
     *  Marks all rows in the region dirty.
     *
     *  @param scrollTop     First row of the scroll region (zero-based).
     *  @param scrollBottom  Last row of the scroll region (zero-based).
     *  @param count         Number of rows to scroll (default 1).
     */
    void scrollDown (int scrollTop, int scrollBottom, int count = 1) noexcept;

    ///@}

    //==========================================================================
    /** @name Scroll-off capture — terminal-specific */
    ///@{

    /** Returns the number of unconsumed scroll-off rows available for reading. */
    int getNumScrolledRows() const noexcept;

    /** Returns a read-only pointer to a scroll-off row (0 = oldest unconsumed).
     *  Display calls this to drain scroll-off rows into Screen as jam::Cells. */
    const jam::Cell* getScrolledReadPointer (int index) const noexcept;

    /** Marks scroll-off rows as consumed. Called by Display after draining. */
    void consumeScrolledRows (int count) noexcept;

    ///@}

private:
    //==========================================================================
    /** @brief Internal per-screen buffer storage. */
    struct Buffer
    {
        juce::HeapBlock<char, true> allocation;   ///< SIMD-aligned raw storage.
        jam::Cell* cells { nullptr };              ///< Points into allocation.
        int numRows { 0 };                         ///< Logical visible row count.
        int numCols { 0 };                         ///< Logical column count.
        int alignedCols { 0 };                     ///< numCols rounded up to multiple of 4.
        int capacity { 0 };                        ///< Total row slots (numRows + scrollMargin).
        int scrollMargin { 0 };                    ///< Extra rows for scroll-off ring headroom.
        int head { 0 };                            ///< Ring head index (normal screen only).
        std::atomic<int> scrolledCount { 0 };      ///< Unconsumed scroll-off rows.
        std::atomic<uint64_t> dirtyRows { 0 };     ///< Per-row dirty bitmask.
        bool isClear { true };                     ///< Optimisation gate (AudioBuffer pattern).
        int allocatedCapacity { 0 };               ///< Physical allocation size in rows.
        int allocatedCols { 0 };                   ///< Physical allocation width in aligned cols.

        /** Returns a pointer to the first Cell of the given logical row. */
        jam::Cell* rowPtr (int logicalRow) noexcept;

        /** Returns a const pointer to the first Cell of the given logical row. */
        const jam::Cell* rowPtr (int logicalRow) const noexcept;

        /** Marks a logical row as dirty in the bitmask. */
        void markDirty (int logicalRow) noexcept;

        /** Marks all visible rows dirty. */
        void markAllDirty() noexcept;
    };

    static constexpr int defaultScrollMargin { 256 };
    static constexpr int simdCellAlignment { 4 };  ///< Cells per row rounded to this multiple (4 cells = 64 bytes = cache line).

    std::array<Buffer, 2> buffers;                 ///< 0 = normal, 1 = alternate.
    std::atomic<int> activeIndex { 0 };            ///< 0 = normal, 1 = alternate.

    Buffer& activeBuffer() noexcept;
    const Buffer& activeBuffer() const noexcept;

    static int alignCols (int cols) noexcept;

    void allocateBuffer (Buffer& buf, int numRows, int numCols, int scrollMargin,
                         bool keepContent, bool clearExtra, bool avoidRealloc) noexcept;

    //==========================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Grid)
};

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Terminal
