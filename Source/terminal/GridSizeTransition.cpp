#include "GridSizeTransition.h"
#include "../Map.h"

namespace terminal
{
/*____________________________________________________________________________*/

GridSizeTransition::GridSizeTransition (Grid& gridRef,
                                        Video& videoRef,
                                        jam::Function::Map<juce::Identifier, void>& eventsRef) noexcept
    : grid (gridRef)
    , video (videoRef)
    , events (eventsRef)
{
    updateCrossfadeIncrement();
}

//==============================================================================
// SST: process(sample) — timer-driven
void GridSizeTransition::process() noexcept
{
    if (isTransitioning)
    {
        // No dimension interpolation — Grid is already at target dims from applyChange.
        // Timer exists only for SIGWINCH debounce. Advance position toward completion.
        advanceCrossfade();
    }
}

//==============================================================================
// SST: reset()
void GridSizeTransition::reset() noexcept
{
    isTransitioning = false;
    crossfadePosition = 1.0;
    hasPending = false;
    stopTimer();
}

// SST: prepare(sampleRate, blockSize)
void GridSizeTransition::prepare (int scrollbackLinesValue) noexcept
{
    scrollbackLines = scrollbackLinesValue;
    updateCrossfadeIncrement();
    isReady = false;
}

//==============================================================================
// SST: set(name, value)
void GridSizeTransition::set (cell cols, cell rows) noexcept
{
    if (cols.value != targetCols.value or rows.value != targetRows.value)
    {
        targetCols = cols;
        targetRows = rows;

        if (isTransitioning)
        {
            // Replace pending — keep only latest, no queue.
            hasPending = true;
            pendingCols = cols;
            pendingRows = rows;
        }
        else
        {
            applyChange();
        }
    }
}

// SST: flush()
void GridSizeTransition::flush() noexcept
{
    if (hasPending)
    {
        targetCols = pendingCols;
        targetRows = pendingRows;
        applyChange();
    }

    isTransitioning = false;
    crossfadePosition = 1.0;
    hasPending = false;
    stopTimer();
}

//==============================================================================
// SST: applyChange(name, value)
void GridSizeTransition::applyChange() noexcept
{
    // Apply pending cell size.
    if (hasPendingCellSize)
    {
        video.setCellSize (pendingCellWidth, pendingCellHeight);
        hasPendingCellSize = false;
    }

    // tmux order: screen_resize_y BEFORE snapshot BEFORE grid_reflow.
    // resizeHeight adjusts live grid height. Snapshot captures height-adjusted state.
    // reflowFrom reads from height-adjusted snapshot — effects survive.
    int cursorRow { video.getCursorRow().value };
    int cursorCol { video.getCursorCol().value };
    int scrollOffset { 0 };

    if (targetRows.value != grid.getViewportRows().value)
    {
        cell cursorRowCell { cursorRow };
        grid.resizeHeight (targetRows, cursorRowCell);
        cursorRow = cursorRowCell.value;
    }

    // previous = current (height-adjusted snapshot)
    captureSnapshot();

    // Record start dims for interpolation.
    startCols = video.getCols();
    startRows = video.getVisibleRows();

    const auto reflowedNumRows { grid.reflowFrom (previous,
                                                  previousHead,
                                                  previousNumRows,
                                                  previousRingMask,
                                                  previousViewportRows,
                                                  targetRows.value,
                                                  targetCols.value,
                                                  scrollbackLines,
                                                  cursorRow,
                                                  cursorCol,
                                                  scrollOffset) };

    video.setDimensions (targetCols, targetRows);
    video.loadScreenState (cell (cursorRow), cell (cursorCol), true, cell (0), cell (0), false, 0);
    video.resize (targetCols, targetRows);

    // Fire tick event so Processor writes State for the target state.
    events.get (id::resizeTick,
                reflowedNumRows.at (Map::Screen::normal),
                reflowedNumRows.at (Map::Screen::alternate),
                scrollOffset,
                cursorRow,
                cursorCol);

    if (isReady)
    {
        crossfadePosition = 0.0;
        isTransitioning = true;
        startTimer (tickIntervalMs);
    }
    else
    {
        isReady = true;
    }
}

//==============================================================================
// SST: advanceCrossfade()
void GridSizeTransition::advanceCrossfade() noexcept
{
    crossfadePosition += crossfadeIncrement;

    if (crossfadePosition >= 1.0)
    {
        crossfadePosition = 1.0;
        isTransitioning = false;
        stopTimer();

        // Fire resizeEnd — Processor sends SIGWINCH.
        events.get (id::resizeEnd);

        if (hasPending)
        {
            hasPending = false;
            targetCols = pendingCols;
            targetRows = pendingRows;
            applyChange();
        }
    }
}

//==============================================================================
// SST: updateCrossfadeIncrement()
void GridSizeTransition::updateCrossfadeIncrement() noexcept
{
    if (transitionTimeMs > 0.0 and tickIntervalMs > 0)
    {
        const double ticksPerTransition { transitionTimeMs / static_cast<double> (tickIntervalMs) };
        crossfadeIncrement = 1.0 / ticksPerTransition;
    }
}

//==============================================================================
void GridSizeTransition::setCellSize (int cellWidth, int cellHeight) noexcept
{
    pendingCellWidth = cellWidth;
    pendingCellHeight = cellHeight;
    hasPendingCellSize = true;
}

void GridSizeTransition::allocate (cell cols, cell rows) noexcept
{
    if (hasPendingCellSize)
    {
        video.setCellSize (pendingCellWidth, pendingCellHeight);
        hasPendingCellSize = false;
    }

    grid.setSize (rows, cols, cell (scrollbackLines));
    video.setDimensions (cols, rows);
    video.resize (cols, rows);
}

//==============================================================================
void GridSizeTransition::captureSnapshot() noexcept
{
    previousRingMask = grid.getRingMask();
    previousViewportRows = grid.getViewportRows().value;
    previousHead = { grid.getHeadPosition (Map::Screen::normal), grid.getHeadPosition (Map::Screen::alternate) };
    previousNumRows = { grid.getNumRows (Map::Screen::normal), grid.getNumRows (Map::Screen::alternate) };

    const int srcRingSize { previousRingMask + 1 };
    previous.setSize (2, srcRingSize, grid.getBuffer().getNumCols(), false, true, false);

    for (int screen { 0 }; screen < Map::Screen::count; ++screen)
    {
        const int totalRows { previousNumRows.at (screen) + previousViewportRows };

        for (int row { 0 }; row < totalRows; ++row)
        {
            const int physRow { (previousHead.at (screen) - previousNumRows.at (screen) + row) & previousRingMask };
            previous.copyFrom (screen, physRow, grid.getBuffer(), screen, physRow);
        }
    }
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace terminal
