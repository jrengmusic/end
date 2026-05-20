/**
 * @file GridResize.h
 * @brief Resize lifecycle manager for the terminal Grid — discrete state switch with timer coalesce.
 *
 * GridResize is the resize analogue of kuassa::dsp::SmoothStateTransition.
 * It coalesces rapid dimension changes into a single apply() call after a
 * fixed quiet period, preventing partial-resize artifacts when cols and rows
 * arrive in separate ValueTree notifications.
 *
 * ### Pattern
 * - `set()` / `setCellSize()` — store pending values and restart the coalesce timer.
 * - `timerCallback()` — fires after the quiet period and calls `apply()`.
 * - `apply()` — executes the actual Grid / Video / State / TTY mutations.
 *
 * ### Thread model
 * All public methods are MESSAGE THREAD only.  The timer fires on the message
 * thread.  apply() is also callable directly from the message thread for
 * immediate allocation (Processor constructor).
 */

#pragma once

#include <JuceHeader.h>
#include "tty/TTY.h"
#include "Grid.h"
#include "Video.h"
#include "State.h"

namespace terminal
{
/*____________________________________________________________________________*/

/**
 * @class GridResize
 * @brief Resize lifecycle manager: coalesces terminal dimension changes and applies them atomically.
 *
 * Stores pending cols, rows, and cell pixel dimensions.  A 50 ms coalesce timer
 * prevents redundant apply() calls when ValueTree fires multiple property changes
 * in rapid succession (e.g. cols then rows then cellWidth then cellHeight).
 *
 * Wired to an optional TTY for SIGWINCH delivery after every apply().
 * IPC sessions pass nullptr — the TTY call is guarded accordingly.
 *
 * @note MESSAGE THREAD only.
 */
class GridResize : private juce::Timer
{
public:
    /**
     * @brief Constructs GridResize with references to the Grid, Video, and State it manages.
     *
     * @param grid   Terminal cell buffer — resized by apply().
     * @param video  VT command processor — receives setDimensions / resize / setCellSize.
     * @param state  Terminal parameter store — receives storeValue for numRows after reflow.
     *
     * @note MESSAGE THREAD.
     */
    GridResize (Grid& grid, Video& video, State& state) noexcept;

    /** Stores pending terminal dimensions. Restarts coalesce timer.
     *  Called by Processor::valueTreePropertyChanged when cols/visibleRows change.
     *
     *  @param cols  New terminal column count.
     *  @param rows  New terminal viewport row count.
     *
     *  @note MESSAGE THREAD. */
    void set (cell cols, cell rows) noexcept;

    /** Stores pending cell pixel dimensions.
     *  Called by Processor::valueTreePropertyChanged when cellWidth/cellHeight change.
     *
     *  @param cellWidth   Cell width in physical pixels.
     *  @param cellHeight  Cell height in physical pixels.
     *
     *  @note MESSAGE THREAD. */
    void setCellSize (int cellWidth, int cellHeight) noexcept;

    /** Sets the scrollback line limit from config.
     *
     *  @param lines  Maximum scrollback history row count.
     *
     *  @note MESSAGE THREAD. */
    void setScrollbackLines (int lines) noexcept;

    /** Wires the TTY for SIGWINCH on resize. Nullable (IPC mode has no TTY).
     *
     *  @param ttyToUse  Owning TTY, or nullptr for IPC sessions.
     *
     *  @note MESSAGE THREAD. */
    void setTTY (TTY* ttyToUse) noexcept;

    /** Applies pending changes immediately without coalesce delay.
     *  Used for initial allocation in Processor constructor.
     *
     *  @note MESSAGE THREAD. */
    void apply() noexcept;

private:
    void timerCallback() override;

    Grid& grid;
    Video& video;
    State& state;
    TTY* tty { nullptr };

    cell pendingCols { 0 };
    cell pendingRows { 0 };
    int pendingCellWidth { 0 };
    int pendingCellHeight { 0 };
    int scrollbackLines { 0 };
    bool hasPendingDimensions { false };
    bool hasPendingCellSize { false };

    static constexpr int coalesceMs { 50 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GridResize)
};

/**______________________________END OF NAMESPACE______________________________*/
} // namespace terminal
