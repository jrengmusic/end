#include "GridResize.h"

namespace terminal
{
/*____________________________________________________________________________*/

GridResize::GridResize (Grid& gridRef, Video& videoRef, State& stateRef) noexcept
    : grid (gridRef)
    , video (videoRef)
    , state (stateRef)
{
}

void GridResize::set (cell cols, cell rows) noexcept
{
    pendingCols = cols;
    pendingRows = rows;
    hasPendingDimensions = true;
    startTimer (coalesceMs);
}

void GridResize::setCellSize (int cellWidth, int cellHeight) noexcept
{
    pendingCellWidth = cellWidth;
    pendingCellHeight = cellHeight;
    hasPendingCellSize = true;
    startTimer (coalesceMs);
}

void GridResize::setScrollbackLines (int lines) noexcept
{
    scrollbackLines = lines;
}

void GridResize::setTTY (TTY* ttyToUse) noexcept
{
    tty = ttyToUse;
}

void GridResize::timerCallback()
{
    stopTimer();
    apply();
}

void GridResize::apply() noexcept
{
    if (hasPendingCellSize)
    {
        video.setCellSize (pendingCellWidth, pendingCellHeight);
        hasPendingCellSize = false;
    }

    if (hasPendingDimensions)
    {
        if (pendingCols.value > 0 and pendingRows.value > 0)
        {
            if (grid.isAllocated())
            {
                int cursorRow { video.getCursorRow().value };
                int cursorCol { video.getCursorCol().value };
                const auto reflowedNumRows { grid.reflow (pendingRows.value, pendingCols.value, scrollbackLines, cursorRow, cursorCol) };

                grid.setNumRows (0, reflowedNumRows.at (0));
                grid.setNumRows (1, reflowedNumRows.at (1));
                state.setNumRows (0, reflowedNumRows.at (0));
                state.setNumRows (1, reflowedNumRows.at (1));

                const int activeScreen { state.getActiveScreen() };
                const bool visible { state.loadCursorVisible (activeScreen) };

                video.setDimensions (pendingCols, pendingRows);
                video.loadScreenState (cursorRow, cursorCol, visible, 0, 0, false, state.getKeyboardFlags());
                video.resize (pendingCols, pendingRows);
            }
            else
            {
                grid.setSize (pendingRows.value, pendingCols.value, scrollbackLines);
                video.setDimensions (pendingCols, pendingRows);
                video.resize (pendingCols, pendingRows);
            }

            if (tty != nullptr and tty->isThreadRunning())
            {
                const int width { state.loadWidth() };
                const int height { state.loadHeight() };
                tty->platformResize (pendingCols, pendingRows, width, height);
            }
        }

        hasPendingDimensions = false;
    }
}

/**______________________________END OF NAMESPACE______________________________*/
} // namespace terminal
