/**
 * @file Grid.cpp
 * @brief Implementation of the ring-indexed terminal frame buffer.
 *
 * @see Grid.h for design notes, ring index model, and API documentation.
 */

#include "Grid.h"

namespace Terminal
{
/*____________________________________________________________________________*/

// =============================================================================
// Private helper
// =============================================================================

int Grid::physicalRow (int screen, int row) const noexcept
{
    return (head.at (screen) + row) & ringMask;
}

// =============================================================================
// Told — Processor tells Grid what to do
// =============================================================================

void Grid::setSize (int viewportRowCount, int numCols, int scrollbackLineCount) noexcept
{
    jassert (viewportRowCount > 0 and numCols > 0 and scrollbackLineCount >= 0);

    const int minRing { (scrollbackLineCount + viewportRowCount) * 2 };
    int ringSize { 1 };
    while (ringSize < minRing)
        ringSize <<= 1;

    buffer.setSize (2, ringSize, numCols, false, true, false);
    ringMask = ringSize - 1;
    viewportRows = viewportRowCount;
    scrollbackLines = scrollbackLineCount;
    head.at (0) = 0;
    head.at (1) = 0;
    numRows.at (0) = 0;
    numRows.at (1) = 0;
}

void Grid::setNumRows (int screen, int value) noexcept
{
    jassert (screen >= 0 and screen < 2);
    numRows.at (screen) = value;
}

// =============================================================================
// Scroll
// =============================================================================

int Grid::scrollUp (int screen, int scrollTop, int scrollBottom, int count) noexcept
{
    jassert (screen >= 0 and screen < 2);
    jassert (scrollTop >= 0 and scrollBottom < viewportRows and scrollTop <= scrollBottom);

    const int clampedCount { juce::jmin (count, scrollBottom - scrollTop + 1) };

    if (clampedCount > 0)
    {
        const bool isFullScreen { scrollTop == 0 and scrollBottom == viewportRows - 1 };

        if (isFullScreen)
        {
            for (int n { 0 }; n < clampedCount; ++n)
            {
                // Advance head — old viewport-top stays in place as history.
                head.at (screen) = (head.at (screen) + 1) & ringMask;

                // Clear new bottom viewport row.
                buffer.clear (screen, physicalRow (screen, viewportRows - 1));
            }
        }
        else
        {
            for (int n { 0 }; n < clampedCount; ++n)
            {
                for (int r { scrollTop }; r < scrollBottom; ++r)
                    buffer.copyFrom (screen, physicalRow (screen, r),
                                     buffer, screen, physicalRow (screen, r + 1));

                buffer.clear (screen, physicalRow (screen, scrollBottom));
            }
        }
    }

    return clampedCount;
}

void Grid::scrollDown (int screen, int scrollTop, int scrollBottom, int count) noexcept
{
    jassert (screen >= 0 and screen < 2);
    jassert (scrollTop >= 0 and scrollBottom < viewportRows and scrollTop <= scrollBottom);

    const int clampedCount { juce::jmin (count, scrollBottom - scrollTop + 1) };

    if (clampedCount > 0)
    {
        const bool isFullScreen { scrollTop == 0 and scrollBottom == viewportRows - 1 };

        if (isFullScreen)
        {
            for (int n { 0 }; n < clampedCount; ++n)
            {
                // Retreat head — clear new top viewport row.
                head.at (screen) = (head.at (screen) - 1) & ringMask;
                buffer.clear (screen, physicalRow (screen, 0));
            }
        }
        else
        {
            for (int n { 0 }; n < clampedCount; ++n)
            {
                for (int r { scrollBottom }; r > scrollTop; --r)
                    buffer.copyFrom (screen, physicalRow (screen, r),
                                     buffer, screen, physicalRow (screen, r - 1));

                buffer.clear (screen, physicalRow (screen, scrollTop));
            }
        }
    }
}

// =============================================================================
// Clear
// =============================================================================

void Grid::clear (int screen) noexcept
{
    jassert (screen >= 0 and screen < 2);

    for (int r { 0 }; r < viewportRows; ++r)
        buffer.clear (screen, physicalRow (screen, r));
}

void Grid::clear (int screen, int row) noexcept
{
    jassert (screen >= 0 and screen < 2);
    jassert (row >= 0 and row < viewportRows);
    buffer.clear (screen, physicalRow (screen, row));
}

void Grid::clear (int screen, int row, int startCol, int numCols) noexcept
{
    jassert (screen >= 0 and screen < 2);
    jassert (row >= 0 and row < viewportRows);

    const int clampedCount { juce::jmin (numCols, buffer.getNumCols() - startCol) };

    if (clampedCount > 0)
        buffer.clear (screen, physicalRow (screen, row), startCol, clampedCount);
}

// =============================================================================
// Asked — storage access only
// =============================================================================

jam::Row* Grid::getWritePointer (int screen, int row) noexcept
{
    jassert (screen >= 0 and screen < 2);
    jassert (row >= 0 and row < viewportRows);
    return buffer.getWritePointer (screen, physicalRow (screen, row));
}

const jam::Row* Grid::getRow (int screen, int absoluteIndex) const noexcept
{
    jassert (screen >= 0 and screen < 2);
    jassert (absoluteIndex >= 0 and absoluteIndex < numRows.at (screen) + viewportRows);
    return buffer.getReadPointer (screen, (head.at (screen) - numRows.at (screen) + absoluteIndex) & ringMask);
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Terminal
