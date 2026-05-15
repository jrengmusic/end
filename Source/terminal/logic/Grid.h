/**
 * @file Grid.h
 * @brief Live frame buffer of jam::Cell records — the AudioBuffer of the terminal.
 *
 * Grid is a dumb buffer. Parser writes cells in-place on the reader thread.
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
 * Three flat jam::Buffer<jam::Cell> instances:
 * - normal     — normal screen visible frame
 * - alternate  — alternate screen visible frame
 * - scrollOff  — scroll-off transit ring (normal screen, scrollBufferSize rows)
 *
 * ### Thread model
 * - Parser writes via getWritePointer / scrollUp / scrollDown / clear — READER THREAD.
 * - Display reads via getReadPointer / scroll-off drain — MESSAGE THREAD.
 * - setSize — called from MESSAGE THREAD (Processor::resized).
 *
 * ### Scroll-off FIFO
 * scrollOff is a flat Buffer<Cell>.  Parser writes via AbstractFifo (lock-free SPSC);
 * Display reads via AbstractFifo then advances the read head.
 * State::scrolledRows carries the cross-thread count signal.
 *
 * ### Screen parameter
 * Callers pass the active screen index (0 = normal, 1 = alternate) to every call,
 * mirroring AudioBuffer where the caller passes the channel index.  Grid holds no
 * active-screen state — the caller (Video) is the authority on which screen is live.
 *
 * @see Parser   — sole writer (reader thread)
 * @see Display  — sole reader (message thread)
 * @see State    — carries scrolledRows atomic counter
 */

#pragma once

#include <JuceHeader.h>
#include <jam_tui/jam_tui.h>
#include <cstring>

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
     *  @param scrollBufferSize    Row count for the scroll-off ring (normal screen only).
     *                             Caller-supplied — typically config.nexus.terminal.scrollbackLines.
     *  @param keepExistingContent Preserve existing cell data up to the smaller of old/new dims.
     *  @param clearExtraSpace     Zero-init any newly exposed cells.
     *  @param avoidReallocating   Reuse existing allocation if large enough.
     */
    void setSize (int numRows, int numCols, int scrollBufferSize = 0,
                  bool keepExistingContent = false,
                  bool clearExtraSpace = false,
                  bool avoidReallocating = false) noexcept;

    /** Returns the row count for the given screen index (0 = normal, 1 = alternate). */
    int getNumRows (int screen) const noexcept;

    /** Returns the column count for the given screen index (0 = normal, 1 = alternate). */
    int getNumCols (int screen) const noexcept;

    ///@}

    //==========================================================================
    /** @name Pointer access — mirrors AudioBuffer::getReadPointer / getWritePointer */
    ///@{

    /** Returns a read-only pointer to the first Cell in the given row on the given screen
     *  (0 = normal, 1 = alternate). */
    const jam::Cell* getReadPointer (int screen, int row) const noexcept;

    /** Returns a read-only pointer to a specific Cell position on the given screen
     *  (0 = normal, 1 = alternate). */
    const jam::Cell* getReadPointer (int screen, int row, int col) const noexcept;

    /** Returns a writable pointer to the first Cell in the given row on the given screen. */
    jam::Cell* getWritePointer (int screen, int row) noexcept;

    /** Returns a writable pointer to a specific Cell position on the given screen. */
    jam::Cell* getWritePointer (int screen, int row, int col) noexcept;

    ///@}

    //==========================================================================
    /** @name Clear — mirrors AudioBuffer::clear / hasBeenCleared */
    ///@{

    /** Clears all rows of the given screen buffer. */
    void clear (int screen) noexcept;

    /** Clears an entire row of the given screen buffer. */
    void clear (int screen, int row) noexcept;

    /** Clears a range of cells within a row of the given screen buffer. */
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
     *  On the normal screen (screen == 0) with a full-screen region (scrollTop == 0 and
     *  scrollBottom == numRows - 1), captures scroll-off rows into the scrollOff ring.
     *  Uses memmove for all cases — flat, no ring head advance on the visible buffer.
     *
     *  On the alternate screen or a partial region, no scroll-off capture.
     *
     *  Clears the new bottom row(s).
     *
     *  @param screen        Screen index (0 = normal, 1 = alternate).
     *  @param scrollTop     First row of the scroll region (zero-based).
     *  @param scrollBottom  Last row of the scroll region (zero-based).
     *  @param count         Number of rows to scroll (default 1).
     *  @return              Number of scroll-off rows captured (0 on alternate or partial region).
     */
    int scrollUp (int screen, int scrollTop, int scrollBottom, int count = 1) noexcept;

    /** Scrolls rows down within the given scroll region on the given screen (reverse scroll).
     *
     *  Always uses memmove. No scroll-off capture. Clears the new top row(s).
     *
     *  @param screen        Screen index (0 = normal, 1 = alternate).
     *  @param scrollTop     First row of the scroll region (zero-based).
     *  @param scrollBottom  Last row of the scroll region (zero-based).
     *  @param count         Number of rows to scroll (default 1).
     */
    void scrollDown (int screen, int scrollTop, int scrollBottom, int count = 1) noexcept;

    ///@}

    //==========================================================================
    /** @name Scroll-off FIFO — Display-side drain */
    ///@{

    /** @brief Prepares a batch read of scroll-off rows. Returns two contiguous spans.
     *  Caller must call advanceScrollOff() after consuming the rows.
     *  @param count      Number of rows to read.
     *  @param readStart  Start index of first span.
     *  @param readCount  Size of first span.
     *  @param wrapStart  Start index of second span (wrap-around).
     *  @param wrapCount  Size of second span. */
    void prepareScrollOffRead (int count, int& readStart, int& readCount,
                                int& wrapStart, int& wrapCount) const noexcept;

    /** @brief Returns a read-only pointer to a scroll-off row by physical buffer index.
     *  Use indices from prepareScrollOffRead spans. */
    const jam::Cell* getScrollOffReadPointer (int physicalRow) const noexcept;

    /** Advances the read position in the scrollOff ring by count.
     *  Called by Display after draining. */
    void advanceScrollOff (int count) noexcept;

    ///@}

private:
    //==========================================================================

    jam::Buffer<jam::Cell> normal;     ///< Normal screen visible frame.
    jam::Buffer<jam::Cell> alternate;  ///< Alternate screen visible frame.
    jam::Buffer<jam::Cell> scrollOff;  ///< Scroll-off transit buffer.

    juce::AbstractFifo scrollOffFifo { 1 }; ///< Lock-free SPSC index manager for scrollOff.

    //==========================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Grid)
};

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Terminal
