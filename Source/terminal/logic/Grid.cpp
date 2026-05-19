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
// Static helpers — reflow
// =============================================================================

static void wrapCursorPosition (const jam::Buffer<jam::Row>& buf,
                                int screen, int screenHead, int screenNumRows,
                                int mask, int cursorCol, int cursorViewportRow,
                                int& wx, int& wy) noexcept
{
    const int absoluteCursorRow { screenNumRows + cursorViewportRow };
    int paragraphCol { 0 };
    int paragraphRow { 0 };

    for (int i { 0 }; i < absoluteCursorRow; ++i)
    {
        const int physical { (screenHead - screenNumRows + i) & mask };
        const jam::Row* row { buf.getReadPointer (screen, physical) };
        const bool isWrapped { (row->flags & jam::Row::wrapped) != 0 };

        if (not isWrapped)
        {
            ++paragraphRow;
            paragraphCol = 0;
        }
        else
        {
            paragraphCol += static_cast<int> (row->usedCols);
        }
    }

    wx = paragraphCol + cursorCol;
    wy = paragraphRow;
}

static void unwrapCursorPosition (const jam::Buffer<jam::Row>& scratch,
                                  int screen, int newHead, int newNumRows,
                                  int newViewportRows, int newMask,
                                  int newCols, int wx, int wy,
                                  int& cursorRow, int& cursorCol) noexcept
{
    const int totalRows { newNumRows + newViewportRows };
    int foundParagraph { 0 };
    int colsRemaining { wx };
    bool found { false };

    for (int i { 0 }; i < totalRows and not found; ++i)
    {
        const int physical { (newHead - newNumRows + i) & newMask };
        const jam::Row* row { scratch.getReadPointer (screen, physical) };
        const bool isWrapped { (row->flags & jam::Row::wrapped) != 0 };

        if (foundParagraph == wy)
        {
            const int rowCols { isWrapped ? newCols : static_cast<int> (row->usedCols) };

            if (colsRemaining <= rowCols or not isWrapped)
            {
                const int absoluteRow { i };
                found = true;

                if (absoluteRow >= newNumRows)
                {
                    cursorRow = absoluteRow - newNumRows;
                    cursorCol = colsRemaining;
                }
                else
                {
                    cursorRow = 0;
                    cursorCol = 0;
                }
            }
            else
            {
                colsRemaining -= rowCols;
            }
        }
        else if (not isWrapped)
        {
            ++foundParagraph;
            colsRemaining = wx;
        }
    }

    if (not found)
    {
        cursorRow = 0;
        cursorCol = 0;
    }
}

static void reflowJoin (const jam::Buffer<jam::Row>& oldBuf, jam::Buffer<jam::Row>& scratch,
                        int screen, int oldHead, int oldNumRows, int oldMask,
                        int newMask, int newCols,
                        int sourceIndex, int totalOldRows, int cellOffset,
                        int& writeIdx, int& consumed, int& nextCellOffset) noexcept
{
    const int srcPhysical { (oldHead - oldNumRows + sourceIndex) & oldMask };
    const jam::Row* srcRow { oldBuf.getReadPointer (screen, srcPhysical) };
    const int srcUsed { static_cast<int> (srcRow->usedCols) - cellOffset };
    jam::Row* destRow { scratch.getWritePointer (screen, writeIdx & newMask) };
    std::memcpy (destRow->cells, srcRow->cells + cellOffset, static_cast<size_t> (srcUsed) * sizeof (jam::Cell));
    destRow->usedCols = static_cast<uint16_t> (srcUsed);
    int destOffset { srcUsed };
    consumed = 0;
    nextCellOffset = 0;

    for (int next { sourceIndex + 1 }; next < totalOldRows; ++next)
    {
        const int nextPhysical { (oldHead - oldNumRows + next) & oldMask };
        const jam::Row* nextSrcRow { oldBuf.getReadPointer (screen, nextPhysical) };
        const int nextUsed { static_cast<int> (nextSrcRow->usedCols) };
        const int space { newCols - destOffset };
        const int take { juce::jmin (nextUsed, space) };

        if (take > 0)
        {
            std::memcpy (destRow->cells + destOffset, nextSrcRow->cells, static_cast<size_t> (take) * sizeof (jam::Cell));
            destOffset += take;
        }

        destRow->usedCols = static_cast<uint16_t> (destOffset);
        const bool nextIsWrapped { (nextSrcRow->flags & jam::Row::wrapped) != 0 };

        if (take < nextUsed)
        {
            nextCellOffset = take;
            destRow->flags |= jam::Row::wrapped;
            break;
        }

        ++consumed;

        if (not nextIsWrapped or destOffset >= newCols)
        {
            if (destOffset >= newCols and nextIsWrapped)
                destRow->flags |= jam::Row::wrapped;
            else if (destOffset < newCols)
                destRow->flags = static_cast<uint8_t> (destRow->flags & ~ jam::Row::wrapped);

            break;
        }
    }

    ++writeIdx;
}

static void reflowSplit (const jam::Buffer<jam::Row>& oldBuf, jam::Buffer<jam::Row>& scratch,
                         int screen, int oldHead, int oldNumRows, int oldMask,
                         int newMask, int newCols,
                         int sourceIndex, int totalOldRows, int cellOffset,
                         int& writeIdx, int& consumed, int& nextCellOffset) noexcept
{
    const int srcPhysical { (oldHead - oldNumRows + sourceIndex) & oldMask };
    const jam::Row* srcRow { oldBuf.getReadPointer (screen, srcPhysical) };
    const int usedCols { static_cast<int> (srcRow->usedCols) };
    const bool srcIsWrapped { (srcRow->flags & jam::Row::wrapped) != 0 };
    consumed = 0;
    nextCellOffset = 0;
    int srcOffset { cellOffset };

    while (srcOffset < usedCols)
    {
        const int chunkSize { juce::jmin (newCols, usedCols - srcOffset) };
        jam::Row* destRow { scratch.getWritePointer (screen, writeIdx & newMask) };
        std::memcpy (destRow->cells, srcRow->cells + srcOffset, static_cast<size_t> (chunkSize) * sizeof (jam::Cell));
        destRow->usedCols = static_cast<uint16_t> (chunkSize);
        destRow->flags = 0;
        srcOffset += chunkSize;

        if (srcOffset < usedCols)
            destRow->flags |= jam::Row::wrapped;
        else if (srcIsWrapped)
            destRow->flags |= jam::Row::wrapped;

        ++writeIdx;
    }

    if (srcIsWrapped)
    {
        jam::Row* lastDest { scratch.getWritePointer (screen, (writeIdx - 1) & newMask) };
        int destOffset { static_cast<int> (lastDest->usedCols) };

        for (int next { sourceIndex + 1 }; next < totalOldRows and destOffset < newCols; ++next)
        {
            const int nextPhysical { (oldHead - oldNumRows + next) & oldMask };
            const jam::Row* nextSrc { oldBuf.getReadPointer (screen, nextPhysical) };
            const int nextUsed { static_cast<int> (nextSrc->usedCols) };
            const int space { newCols - destOffset };
            const int take { juce::jmin (nextUsed, space) };
            const bool nextIsWrapped { (nextSrc->flags & jam::Row::wrapped) != 0 };

            if (take > 0)
                std::memcpy (lastDest->cells + destOffset, nextSrc->cells, static_cast<size_t> (take) * sizeof (jam::Cell));

            destOffset += take;
            lastDest->usedCols = static_cast<uint16_t> (destOffset);

            if (take < nextUsed)
            {
                lastDest->flags |= jam::Row::wrapped;
                nextCellOffset = take;
                break;
            }

            ++consumed;

            if (not nextIsWrapped)
            {
                lastDest->flags = static_cast<uint8_t> (lastDest->flags & ~ jam::Row::wrapped);
                break;
            }
        }
    }
}

static void reflowScreen (int screen,
                          int oldHead, int oldNumRows, int oldViewportRows,
                          int oldMask,
                          int newMask, int newCols, int newViewportRows,
                          int scrollbackLineCount,
                          jam::Buffer<jam::Row>& oldBuf,
                          jam::Buffer<jam::Row>& scratch,
                          int& outHead, int& outNumRows) noexcept
{
    const int totalOldRows { oldNumRows + oldViewportRows };
    int writeIdx { 0 };
    int i { 0 };
    int cellOffset { 0 };

    while (i < totalOldRows)
    {
        const int oldPhysical { (oldHead - oldNumRows + i) & oldMask };
        const jam::Row* srcRow { oldBuf.getReadPointer (screen, oldPhysical) };
        const int used { static_cast<int> (srcRow->usedCols) - cellOffset };
        const bool isWrapped { (srcRow->flags & jam::Row::wrapped) != 0 };
        int consumed { 0 };
        int nextOffset { 0 };

        if (used <= 0 and isWrapped)
        {
            cellOffset = 0;
            ++i;
        }
        else if (used > newCols)
        {
            reflowSplit (oldBuf, scratch, screen, oldHead, oldNumRows, oldMask,
                         newMask, newCols, i, totalOldRows, cellOffset,
                         writeIdx, consumed, nextOffset);

            cellOffset = nextOffset;
            i += 1 + consumed;
        }
        else if (used > 0 and isWrapped)
        {
            reflowJoin (oldBuf, scratch, screen, oldHead, oldNumRows, oldMask,
                        newMask, newCols, i, totalOldRows, cellOffset,
                        writeIdx, consumed, nextOffset);

            cellOffset = nextOffset;
            i += 1 + consumed;
        }
        else
        {
            jam::Row* destRow { scratch.getWritePointer (screen, writeIdx & newMask) };

            if (used > 0)
                std::memcpy (destRow->cells, srcRow->cells + cellOffset,
                             static_cast<size_t> (juce::jmin (used, newCols)) * sizeof (jam::Cell));

            destRow->usedCols = static_cast<uint16_t> (juce::jmin (used > 0 ? used : 0, newCols));
            destRow->flags = static_cast<uint8_t> (srcRow->flags & ~ jam::Row::wrapped);
            cellOffset = 0;
            ++writeIdx;
            ++i;
        }
    }

    while (writeIdx < newViewportRows)
    {
        scratch.clear (screen, writeIdx & newMask);
        ++writeIdx;
    }

    const int computedHead { juce::jmax (0, writeIdx - newViewportRows) };
    outNumRows = juce::jmin (computedHead, scrollbackLineCount);
    outHead = computedHead & newMask;

}

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

std::array<int, 2> Grid::reflow (int newViewportRows, int newCols, int scrollbackLines,
                                 int& cursorRow, int& cursorCol) noexcept
{
    jassert (isAllocated());

    const int oldRingMask { ringMask };
    const auto oldHead { head };
    const auto oldNumRows { numRows };
    const int oldViewportRows { viewportRows };

    const int minRing { (scrollbackLines + newViewportRows) * 2 };
    int newRingSize { 1 };
    while (newRingSize < minRing)
        newRingSize <<= 1;

    const int newRingMask { newRingSize - 1 };
    jam::Buffer<jam::Row> scratch;
    scratch.setSize (2, newRingSize, newCols, false, true, false);

    int wx { 0 };
    int wy { 0 };
    static constexpr int cursorScreen { 0 };
    wrapCursorPosition (buffer, cursorScreen, oldHead.at (cursorScreen), oldNumRows.at (cursorScreen),
                        oldRingMask, cursorCol, cursorRow, wx, wy);

    std::array<int, 2> result { 0, 0 };
    std::array<int, 2> newHead { 0, 0 };

    for (int screen { 0 }; screen < 2; ++screen)
        reflowScreen (screen, oldHead.at (screen), oldNumRows.at (screen), oldViewportRows,
                      oldRingMask, newRingMask, newCols, newViewportRows,
                      scrollbackLines, buffer, scratch, newHead.at (screen), result.at (screen));

    buffer.setSize (2, newRingSize, newCols, false, true, false);

    for (int screen { 0 }; screen < 2; ++screen)
    {
        const int totalRows { result.at (screen) + newViewportRows };

        for (int row { 0 }; row < totalRows; ++row)
        {
            const int physRow { (newHead.at (screen) - result.at (screen) + row) & newRingMask };
            buffer.copyFrom (screen, physRow, scratch, screen, physRow);
        }
    }

    head = newHead;
    ringMask = newRingMask;
    viewportRows = newViewportRows;
    this->scrollbackLines = scrollbackLines;
    unwrapCursorPosition (scratch, cursorScreen, newHead.at (cursorScreen), result.at (cursorScreen),
                          newViewportRows, newRingMask, newCols, wx, wy, cursorRow, cursorCol);
    return result;
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

bool Grid::isAllocated() const noexcept
{
    return ringMask > 0;
}

int Grid::getNumRows (int screen) const noexcept
{
    jassert (screen >= 0 and screen < 2);
    return numRows.at (screen);
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Terminal
