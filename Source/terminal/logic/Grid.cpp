/**
 * @file Grid.cpp
 * @brief Implementation of the ring-index terminal cell grid.
 *
 * @see Grid.h for design notes and API documentation.
 */

#include "Grid.h"

namespace Terminal
{
/*____________________________________________________________________________*/

// =============================================================================
// Private helper
// =============================================================================

int Grid::physicalRow (int screen, int logicalRow) const noexcept { return (head.at (screen) + logicalRow) & ringMask; }

// =============================================================================
// Size
// =============================================================================

void Grid::setSize (int numRows, int numCols) noexcept
{
    jassert (numRows > 0 and numCols > 0);

    const int minRing { numRows * 2 };
    int ringSize { 1 };
    while (ringSize < minRing)
        ringSize <<= 1;

    cells.setSize (2, ringSize, numCols, false, true, false);
    ringMask = ringSize - 1;
    viewportRows = cell (numRows);
    head.at (0) = 0;
    head.at (1) = 0;
}

int Grid::getNumRows (int /*screen*/) const noexcept { return viewportRows.value; }

int Grid::getNumCols (int /*screen*/) const noexcept { return cells.getNumCols(); }

int Grid::getRingSize() const noexcept { return ringMask + 1; }

// =============================================================================
// Pointer access
// =============================================================================

const jam::Cell* Grid::getReadPointer (int screen, int row) const noexcept
{
    jassert (screen >= 0 and screen < 2);
    jassert (row >= 0 and row < viewportRows.value);
    return cells.getReadPointer (screen, physicalRow (screen, row));
}

const jam::Cell* Grid::getReadPointer (int screen, int row, int col) const noexcept
{
    jassert (screen >= 0 and screen < 2);
    jassert (row >= 0 and row < viewportRows.value);
    jassert (col >= 0 and col < cells.getNumCols());
    return cells.getReadPointer (screen, physicalRow (screen, row), col);
}

jam::Cell* Grid::getWritePointer (int screen, int row) noexcept
{
    jassert (screen >= 0 and screen < 2);
    jassert (row >= 0 and row < viewportRows.value);
    return cells.getWritePointer (screen, physicalRow (screen, row));
}

jam::Cell* Grid::getWritePointer (int screen, int row, int col) noexcept
{
    jassert (screen >= 0 and screen < 2);
    jassert (row >= 0 and row < viewportRows.value);
    jassert (col >= 0 and col < cells.getNumCols());
    return cells.getWritePointer (screen, physicalRow (screen, row), col);
}

const jam::Buffer<jam::Cell>& Grid::getBuffer() const noexcept { return cells; }

int Grid::getHead (int screen) const noexcept { return head.at (screen); }

// =============================================================================
// Clear
// =============================================================================

void Grid::clear (int screen) noexcept
{
    jassert (screen >= 0 and screen < 2);
    cells.clear (screen);
    head.at (screen) = 0;
}

void Grid::clear (int screen, int row) noexcept
{
    jassert (screen >= 0 and screen < 2);
    cells.clear (screen, physicalRow (screen, row));
}

void Grid::clear (int screen, int row, int startCol, int numCols) noexcept
{
    jassert (screen >= 0 and screen < 2);
    const int clampedCount { juce::jmin (numCols, cells.getNumCols() - startCol) };

    if (clampedCount > 0)
        cells.clear (screen, physicalRow (screen, row), startCol, clampedCount);
}

bool Grid::hasBeenCleared (int screen) const noexcept
{
    jassert (screen >= 0 and screen < 2);
    return cells.hasBeenCleared (screen);
}

// =============================================================================
// Scroll
// =============================================================================

int Grid::scrollUp (int screen, int scrollTop, int scrollBottom, int count) noexcept
{
    jassert (screen >= 0 and screen < 2);

    const int clampedCount { juce::jmin (count, scrollBottom - scrollTop + 1) };

    if (clampedCount > 0)
    {
        const bool isFullScreen { scrollTop == 0 and scrollBottom == viewportRows.value - 1 };

        if (isFullScreen)
        {
            for (int n { 0 }; n < clampedCount; ++n)
            {
                // The row at head is the oldest viewport row — clear it to become the new blank bottom.
                cells.clear (screen, head.at (screen));
                head.at (screen) = (head.at (screen) + 1) & ringMask;
            }

        }
        else
        {
            for (int n { 0 }; n < clampedCount; ++n)
            {
                for (int r { scrollTop }; r < scrollBottom; ++r)
                    cells.copyFrom (screen, physicalRow (screen, r), cells, screen, physicalRow (screen, r + 1));

                cells.clear (screen, physicalRow (screen, scrollBottom));
            }
        }
    }

    return clampedCount;
}

void Grid::scrollDown (int screen, int scrollTop, int scrollBottom, int count) noexcept
{
    jassert (screen >= 0 and screen < 2);

    const int clampedCount { juce::jmin (count, scrollBottom - scrollTop + 1) };

    if (clampedCount > 0)
    {
        const bool isFullScreen { scrollTop == 0 and scrollBottom == viewportRows.value - 1 };

        if (isFullScreen)
        {
            for (int n { 0 }; n < clampedCount; ++n)
            {
                head.at (screen) = (head.at (screen) - 1) & ringMask;
                cells.clear (screen, physicalRow (screen, 0));
            }
        }
        else
        {
            for (int n { 0 }; n < clampedCount; ++n)
            {
                for (int r { scrollBottom }; r > scrollTop; --r)
                    cells.copyFrom (screen, physicalRow (screen, r), cells, screen, physicalRow (screen, r - 1));

                cells.clear (screen, physicalRow (screen, scrollTop));
            }
        }
    }
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Terminal
