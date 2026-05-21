#include "GridResize.h"
#include "../Map.h"

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

                // Read current scroll state from active screen.
                const int activeScreen { state.getActiveScreen() };
                const juce::Identifier activeScreenId { Map::Screen::getContext()->get (activeScreen) };
                auto activeNode { state.get().getChildWithName (activeScreenId) };
                int scrollOffset { static_cast<int> (jam::ValueTree::getValueFromChildWithID (activeNode, id::scrollOffset).getValue()) };

                // Height resize step (before reflow).
                cell cursorRowCell { cursorRow };
                grid.resizeHeight (pendingRows, cursorRowCell);
                cursorRow = cursorRowCell.value;

                // Width reflow.
                const auto reflowedNumRows { grid.reflow (pendingRows.value, pendingCols.value, scrollbackLines, cursorRow, cursorCol, scrollOffset) };

                // Write numRows per screen to State.
                const juce::Identifier normalScreenId { Map::Screen::getContext()->get (Map::Screen::normal) };
                const juce::Identifier alternateScreenId { Map::Screen::getContext()->get (Map::Screen::alternate) };
                auto normalNode { state.get().getChildWithName (normalScreenId) };
                auto normalNumRowsParam { jam::ValueTree::getChildWithID (normalNode, id::numRows.toString()) };
                normalNumRowsParam.setProperty (id::value, reflowedNumRows.at (0), nullptr);

                auto alternateNode { state.get().getChildWithName (alternateScreenId) };
                auto alternateNumRowsParam { jam::ValueTree::getChildWithID (alternateNode, id::numRows.toString()) };
                alternateNumRowsParam.setProperty (id::value, reflowedNumRows.at (1), nullptr);

                // Write adjusted scrollOffset back to active screen.
                auto activeScrollParam { jam::ValueTree::getChildWithID (activeNode, id::scrollOffset.toString()) };
                activeScrollParam.setProperty (id::value, scrollOffset, nullptr);

                // Restore cursor and video state.
                const bool visible { static_cast<int> (jam::ValueTree::getValueFromChildWithID (activeNode, id::cursorVisible).getValue()) != 0 };
                const uint32_t kbFlags { static_cast<uint32_t> (static_cast<int> (jam::ValueTree::getValueFromChildWithID (activeNode, id::keyboardFlags).getValue())) };

                video.setDimensions (pendingCols, pendingRows);
                video.loadScreenState (cell (cursorRow), cell (cursorCol), visible, 0_cell, 0_cell, false, kbFlags);
                video.resize (pendingCols, pendingRows);
            }
            else
            {
                grid.setSize (pendingRows, pendingCols, cell (scrollbackLines));
                video.setDimensions (pendingCols, pendingRows);
                video.resize (pendingCols, pendingRows);
            }

            if (tty != nullptr and tty->isThreadRunning())
            {
                const int width { static_cast<int> (jam::ValueTree::getValueFromChildWithID (state.get(), jam::ID::width).getValue()) };
                const int height { static_cast<int> (jam::ValueTree::getValueFromChildWithID (state.get(), jam::ID::height).getValue()) };
                tty->platformResize (pendingCols, pendingRows, width, height);
            }
        }

        hasPendingDimensions = false;
    }
}

/**______________________________END OF NAMESPACE______________________________*/
} // namespace terminal
