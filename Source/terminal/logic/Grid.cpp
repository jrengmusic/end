/**
 * @file Grid.cpp
 * @brief Implementation of the AudioBuffer-pattern terminal cell grid.
 *
 * @see Grid.h for design notes and API documentation.
 */

#include "Grid.h"

namespace Terminal
{ /*____________________________________________________________________________*/

// =============================================================================
// Size
// =============================================================================

void Grid::setSize (int numRows, int numCols, int scrollBufferSize,
                    bool keepExistingContent,
                    bool clearExtraSpace,
                    bool avoidReallocating) noexcept
{
    jassert (numRows > 0 and numCols > 0);
    jassert (scrollBufferSize >= 0);

    normal.setSize    (numRows, numCols, keepExistingContent, clearExtraSpace, avoidReallocating);
    alternate.setSize (numRows, numCols, keepExistingContent, clearExtraSpace, avoidReallocating);

    const bool scrollSizeChanged { scrollBufferSize != scrollOff.getNumRows()
                                   or numCols != scrollOff.getNumCols() };

    if (scrollSizeChanged)
    {
        // AbstractFIFO sacrifices one slot (full vs empty disambiguation).
        // Buffer must be sized to FIFO's total size so all returned indices are valid.
        const int fifoSize { scrollBufferSize > 0 ? scrollBufferSize + 1 : 1 };
        scrollOff.setSize (fifoSize, numCols, false, false, false);
        scrollOffFifo.setTotalSize (fifoSize);
    }
}

int Grid::getNumRows (int screen) const noexcept
{
    return (screen == 0 ? normal : alternate).getNumRows();
}

int Grid::getNumCols (int screen) const noexcept
{
    return (screen == 0 ? normal : alternate).getNumCols();
}

// =============================================================================
// Pointer access
// =============================================================================

const jam::Cell* Grid::getReadPointer (int screen, int row) const noexcept
{
    const auto& buf { screen == 0 ? normal : alternate };
    jassert (row >= 0 and row < buf.getNumRows());
    return buf.getReadPointer (row);
}

const jam::Cell* Grid::getReadPointer (int screen, int row, int col) const noexcept
{
    const auto& buf { screen == 0 ? normal : alternate };
    jassert (row >= 0 and row < buf.getNumRows());
    jassert (col >= 0 and col < buf.getNumCols());
    return buf.getReadPointer (row, col);
}

jam::Cell* Grid::getWritePointer (int screen, int row) noexcept
{
    auto& buf { screen == 0 ? normal : alternate };
    jassert (row >= 0 and row < buf.getNumRows());
    return buf.getWritePointer (row);
}

jam::Cell* Grid::getWritePointer (int screen, int row, int col) noexcept
{
    auto& buf { screen == 0 ? normal : alternate };
    jassert (row >= 0 and row < buf.getNumRows());
    jassert (col >= 0 and col < buf.getNumCols());
    return buf.getWritePointer (row, col);
}

// =============================================================================
// Clear
// =============================================================================

void Grid::clear (int screen) noexcept
{
    (screen == 0 ? normal : alternate).clear();
}

void Grid::clear (int screen, int row) noexcept
{
    (screen == 0 ? normal : alternate).clear (row);
}

void Grid::clear (int screen, int row, int startCol, int numCols) noexcept
{
    auto& buf { screen == 0 ? normal : alternate };
    const int clampedCount { juce::jmin (numCols, buf.getNumCols() - startCol) };

    if (clampedCount > 0)
        buf.clear (row, startCol, clampedCount);
}

bool Grid::hasBeenCleared (int screen) const noexcept
{
    return (screen == 0 ? normal : alternate).hasBeenCleared();
}

// =============================================================================
// Scroll
// =============================================================================

int Grid::scrollUp (int screen, int scrollTop, int scrollBottom, int count) noexcept
{
    auto& buf { screen == 0 ? normal : alternate };
    const int clampedCount { juce::jmin (count, scrollBottom - scrollTop + 1) };
    int captured { 0 };

    if (clampedCount > 0)
    {
        const bool isFullScreen { scrollTop == 0 and scrollBottom == buf.getNumRows() - 1 };
        const bool isNormal     { screen == 0 };

        if (isFullScreen and isNormal)
        {
            if (scrollOff.getNumRows() > 0)
            {
                for (int n { 0 }; n < clampedCount; ++n)
                {
                    int writeStart, writeCount, wrapStart, wrapCount;
                    scrollOffFifo.prepareToWrite (1, writeStart, writeCount, wrapStart, wrapCount);

                    if (writeCount > 0)
                    {
                        scrollOff.copyFrom (writeStart, buf, n);
                        scrollOffFifo.finishedWrite (1);
                        ++captured;
                    }
                }
            }
        }

        // Move rows up within the scroll region.
        const int rowsToMove { scrollBottom - scrollTop + 1 - clampedCount };

        if (rowsToMove > 0)
            buf.moveFrom (scrollTop, scrollTop + clampedCount, rowsToMove);

        for (int r { scrollBottom - clampedCount + 1 }; r <= scrollBottom; ++r)
            buf.clear (r);
    }

    return captured;
}

void Grid::scrollDown (int screen, int scrollTop, int scrollBottom, int count) noexcept
{
    auto& buf { screen == 0 ? normal : alternate };
    const int clampedCount { juce::jmin (count, scrollBottom - scrollTop + 1) };

    if (clampedCount > 0)
    {
        const int rowsToMove { scrollBottom - scrollTop + 1 - clampedCount };

        if (rowsToMove > 0)
            buf.moveFrom (scrollTop + clampedCount, scrollTop, rowsToMove);

        for (int r { scrollTop }; r < scrollTop + clampedCount; ++r)
            buf.clear (r);
    }
}

// =============================================================================
// Scroll-off FIFO
// =============================================================================

void Grid::prepareScrollOffRead (int count, int& readStart, int& readCount,
                                  int& wrapStart, int& wrapCount) const noexcept
{
    scrollOffFifo.prepareToRead (count, readStart, readCount, wrapStart, wrapCount);
}

const jam::Cell* Grid::getScrollOffReadPointer (int physicalRow) const noexcept
{
    jassert (physicalRow >= 0 and physicalRow < scrollOff.getNumRows());
    return scrollOff.getReadPointer (physicalRow);
}

void Grid::advanceScrollOff (int count) noexcept
{
    if (count > 0)
        scrollOffFifo.finishedRead (count);
}

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Terminal
