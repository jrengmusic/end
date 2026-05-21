/**
 * @file Grid.cpp
 * @brief Implementation of the ring-indexed terminal frame buffer.
 *
 * @see Grid.h for design notes, ring index model, and API documentation.
 */

#include "Grid.h"

namespace terminal
{
/*____________________________________________________________________________*/

// =============================================================================
// Static helpers — reflow
// =============================================================================

static void wrapCursorPosition (const jam::Buffer<jam::Row>& buf,
                                int screen,
                                int screenHead,
                                int screenNumRows,
                                int mask,
                                int cursorCol,
                                int cursorViewportRow,
                                int& wx,
                                int& wy) noexcept
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

    // Sentinel: cursor at or past end of line → wx = -1 (tmux UINT_MAX equivalent).
    const int cursorPhys { (screenHead - screenNumRows + absoluteCursorRow) & mask };
    const jam::Row* cursorLine { buf.getReadPointer (screen, cursorPhys) };

    if (cursorCol >= static_cast<int> (cursorLine->usedCols))
        wx = -1;
    else
        wx = paragraphCol + cursorCol;

    wy = paragraphRow;
}

static void unwrapCursorPosition (const jam::Buffer<jam::Row>& scratch,
                                  int screen,
                                  int newHead,
                                  int newNumRows,
                                  int newViewportRows,
                                  int newMask,
                                  int newCols,
                                  int wx,
                                  int wy,
                                  int& cursorRow,
                                  int& cursorCol) noexcept
{
    const int totalRows { newNumRows + newViewportRows };
    int foundParagraph { 0 };
    bool found { false };

    // Find the first physical row of the target paragraph.
    int yyStart { 0 };

    for (int i { 0 }; i < totalRows and not found; ++i)
    {
        const int physical { (newHead - newNumRows + i) & newMask };
        const jam::Row* row { scratch.getReadPointer (screen, physical) };
        const bool isWrapped { (row->flags & jam::Row::wrapped) != 0 };

        if (foundParagraph == wy)
        {
            yyStart = i;
            found = true;
        }
        else if (not isWrapped)
        {
            ++foundParagraph;
        }
    }

    if (not found)
    {
        cursorRow = 0;
        cursorCol = 0;
    }
    else if (wx == -1)
    {
        // Sentinel: walk to last physical row of logical line, cursor at usedCols.
        int yy { yyStart };

        for (;;)
        {
            const int physical { (newHead - newNumRows + yy) & newMask };
            const jam::Row* row { scratch.getReadPointer (screen, physical) };

            if ((row->flags & jam::Row::wrapped) == 0 or yy + 1 >= totalRows)
            {
                if (yy >= newNumRows)
                {
                    cursorRow = yy - newNumRows;
                    cursorCol = static_cast<int> (row->usedCols);
                }
                else
                {
                    cursorRow = 0;
                    cursorCol = 0;
                }

                break;
            }

            ++yy;
        }
    }
    else
    {
        // Normal unwrap: walk forward consuming wx.
        int colsRemaining { wx };
        bool placed { false };

        for (int i { yyStart }; i < totalRows and not placed; ++i)
        {
            const int physical { (newHead - newNumRows + i) & newMask };
            const jam::Row* row { scratch.getReadPointer (screen, physical) };
            const bool isWrapped { (row->flags & jam::Row::wrapped) != 0 };
            const int rowCols { isWrapped ? newCols : static_cast<int> (row->usedCols) };

            if (colsRemaining <= rowCols or not isWrapped)
            {
                placed = true;

                if (i >= newNumRows)
                {
                    cursorRow = i - newNumRows;
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

        if (not placed)
        {
            cursorRow = 0;
            cursorCol = 0;
        }
    }
}

static void reflowDead (jam::Buffer<jam::Row>& buf, int screen, int physRow) noexcept
{
    buf.clear (screen, physRow);
    jam::Row* row { buf.getWritePointer (screen, physRow) };
    row->flags = jam::Row::dead;
}

static void reflowMove (jam::Buffer<jam::Row>& oldBuf, jam::Buffer<jam::Row>& scratch,
                        int screen, int srcPhys, int destPhys) noexcept
{
    scratch.copyFrom (screen, destPhys, oldBuf, screen, srcPhys);
    reflowDead (oldBuf, screen, srcPhys);
}

static void reflowJoin (jam::Buffer<jam::Row>& oldBuf,
                        jam::Buffer<jam::Row>& scratch,
                        int screen,
                        int oldHead,
                        int oldNumRows,
                        int oldMask,
                        int newMask,
                        int newCols,
                        int yy,
                        int totalOldRows,
                        int& writeIdx,
                        bool already,
                        int& scrollOffset) noexcept
{
    int to { 0 };

    if (not already)
    {
        to = writeIdx;
        const int srcPhys { (oldHead - oldNumRows + yy) & oldMask };
        reflowMove (oldBuf, scratch, screen, srcPhys, to & newMask);
        ++writeIdx;
    }
    else
    {
        to = writeIdx - 1;
    }

    jam::Row* targetRow { scratch.getWritePointer (screen, to & newMask) };
    int at { static_cast<int> (targetRow->usedCols) };
    int lines { 0 };
    int wrapped { 1 };
    int want { 0 };
    const jam::Row* lastSrc { nullptr };

    for (int line { yy + 1 }; line < totalOldRows; ++line)
    {
        const int srcPhys { (oldHead - oldNumRows + line) & oldMask };
        const jam::Row* srcRow { oldBuf.getReadPointer (screen, srcPhys) };

        if ((srcRow->flags & jam::Row::wrapped) == 0)
            wrapped = 0;

        if (srcRow->usedCols == 0)
        {
            if (not wrapped)
                break;

            ++lines;
            lastSrc = srcRow;
            want = 0;
            continue;
        }

        // Check if first cell fits.
        if (at + 1 > newCols)
            break;

        // Copy cells from source into target.
        lastSrc = srcRow;
        const int srcUsed { static_cast<int> (srcRow->usedCols) };
        want = 0;

        for (int c { 0 }; c < srcUsed; ++c)
        {
            if (at + 1 > newCols)
                break;

            targetRow->cells[at] = srcRow->cells[c];
            ++at;
            ++want;
        }

        ++lines;

        if (want != srcUsed or not wrapped or at >= newCols)
            break;
    }

    if (lines > 0)
    {
        targetRow->usedCols = static_cast<uint16_t> (at);

        // Handle partial consumption of last source row.
        if (lastSrc != nullptr)
        {
            const int srcUsed { static_cast<int> (lastSrc->usedCols) };
            const int left { srcUsed - want };

            if (left > 0)
            {
                // Shift remaining cells left in the source row.
                const int lastLine { yy + lines };
                const int lastPhys { (oldHead - oldNumRows + lastLine) & oldMask };
                jam::Row* lastSrcMut { oldBuf.getWritePointer (screen, lastPhys) };
                std::memmove (lastSrcMut->cells, lastSrcMut->cells + want,
                              static_cast<size_t> (left) * sizeof (jam::Cell));
                lastSrcMut->usedCols = static_cast<uint16_t> (left);
                --lines;
            }
            else if (not wrapped)
            {
                targetRow->flags = static_cast<uint8_t> (targetRow->flags & ~jam::Row::wrapped);
            }
        }

        // Dead all fully consumed source rows.
        for (int i { yy + 1 }; i < yy + 1 + lines; ++i)
        {
            const int physRow { (oldHead - oldNumRows + i) & oldMask };
            reflowDead (oldBuf, screen, physRow);
        }

        // Adjust scroll offset (tmux hscrolled logic).
        if (scrollOffset > to + lines)
            scrollOffset -= lines;
        else if (scrollOffset > to)
            scrollOffset = to;
    }
}

static void reflowSplit (jam::Buffer<jam::Row>& oldBuf,
                         jam::Buffer<jam::Row>& scratch,
                         int screen,
                         int oldHead,
                         int oldNumRows,
                         int oldMask,
                         int newMask,
                         int newCols,
                         int yy,
                         int totalOldRows,
                         int& writeIdx,
                         int& scrollOffset) noexcept
{
    const int srcPhys { (oldHead - oldNumRows + yy) & oldMask };
    const jam::Row* srcRow { oldBuf.getReadPointer (screen, srcPhys) };
    const int usedCols { static_cast<int> (srcRow->usedCols) };
    const int srcFlags { srcRow->flags };
    const bool srcWrapped { (srcFlags & jam::Row::wrapped) != 0 };

    // Chunk source into newCols-width rows.
    int srcOffset { 0 };

    while (srcOffset < usedCols)
    {
        const int chunkSize { juce::jmin (newCols, usedCols - srcOffset) };
        jam::Row* destRow { scratch.getWritePointer (screen, writeIdx & newMask) };
        std::memcpy (destRow->cells, srcRow->cells + srcOffset,
                     static_cast<size_t> (chunkSize) * sizeof (jam::Cell));
        destRow->usedCols = static_cast<uint16_t> (chunkSize);
        destRow->flags = 0;
        srcOffset += chunkSize;

        // All chunks except the last get WRAPPED.
        if (srcOffset < usedCols)
            destRow->flags |= jam::Row::wrapped;

        ++writeIdx;
    }

    // If source was wrapped, last chunk inherits WRAPPED.
    if (srcWrapped)
    {
        jam::Row* lastDest { scratch.getWritePointer (screen, (writeIdx - 1) & newMask) };
        lastDest->flags |= jam::Row::wrapped;
    }

    // Dead the source row.
    reflowDead (oldBuf, screen, srcPhys);

    // Adjust scroll offset: each split adds (numChunks - 1) new rows.
    const int numChunks { (usedCols + newCols - 1) / newCols };

    if (yy <= scrollOffset)
        scrollOffset += numChunks - 1;

    // If source was wrapped and last chunk has room, try to join with next lines.
    if (srcWrapped)
    {
        jam::Row* lastDest { scratch.getWritePointer (screen, (writeIdx - 1) & newMask) };
        const int lastUsed { static_cast<int> (lastDest->usedCols) };

        if (lastUsed < newCols)
        {
            reflowJoin (oldBuf, scratch, screen, oldHead, oldNumRows, oldMask,
                        newMask, newCols, yy, totalOldRows, writeIdx, true, scrollOffset);
        }
    }
}

static void reflowScreen (int screen,
                          int oldHead,
                          int oldNumRows,
                          int oldViewportRows,
                          int oldMask,
                          int newMask,
                          int newCols,
                          int newViewportRows,
                          int scrollbackLineCount,
                          jam::Buffer<jam::Row>& oldBuf,
                          jam::Buffer<jam::Row>& scratch,
                          int& outHead,
                          int& outNumRows,
                          int& scrollOffset) noexcept
{
    const int totalOldRows { oldNumRows + oldViewportRows };
    int writeIdx { 0 };

    for (int yy { 0 }; yy < totalOldRows; ++yy)
    {
        const int physRow { (oldHead - oldNumRows + yy) & oldMask };
        const jam::Row* srcRow { oldBuf.getReadPointer (screen, physRow) };

        // Skip dead rows (consumed by join/split).
        if ((srcRow->flags & jam::Row::dead) != 0)
            continue;

        const int usedCols { static_cast<int> (srcRow->usedCols) };
        const bool isWrapped { (srcRow->flags & jam::Row::wrapped) != 0 };

        if (usedCols == newCols)
        {
            // Exact fit — move as-is.
            reflowMove (oldBuf, scratch, screen, physRow, writeIdx & newMask);
            ++writeIdx;
        }
        else if (usedCols > newCols)
        {
            // Too wide — split into multiple rows (may join tail if wrapped).
            reflowSplit (oldBuf, scratch, screen, oldHead, oldNumRows, oldMask,
                         newMask, newCols, yy, totalOldRows, writeIdx, scrollOffset);
        }
        else if (isWrapped)
        {
            // Narrower and wrapped — join with successor rows.
            reflowJoin (oldBuf, scratch, screen, oldHead, oldNumRows, oldMask,
                        newMask, newCols, yy, totalOldRows, writeIdx, false, scrollOffset);
        }
        else
        {
            // Short unwrapped line — move as-is.
            reflowMove (oldBuf, scratch, screen, physRow, writeIdx & newMask);
            ++writeIdx;
        }
    }

    // Pad target to at least newViewportRows.
    while (writeIdx < newViewportRows)
    {
        scratch.clear (screen, writeIdx & newMask);
        ++writeIdx;
    }

    // Compute output ring state.
    const int computedHead { juce::jmax (0, writeIdx - newViewportRows) };
    outNumRows = juce::jmin (computedHead, scrollbackLineCount);
    outHead = computedHead & newMask;
}

// =============================================================================
// Private helper
// =============================================================================

int Grid::physicalRow (int screen, cell row) const noexcept { return (head.at (screen) + row.value) & ringMask; }

// =============================================================================
// Told — Processor tells Grid what to do
// =============================================================================

void Grid::setSize (cell viewportRowCount, cell numCols, cell scrollbackLineCount) noexcept
{
    jassert (viewportRowCount.value > 0 and numCols.value > 0 and scrollbackLineCount.value >= 0);

    const int minRing { (scrollbackLineCount.value + viewportRowCount.value) * 2 };
    int ringSize { 1 };
    while (ringSize < minRing)
        ringSize <<= 1;

    buffer.setSize (2, ringSize, numCols.value, false, true, false);
    ringMask = ringSize - 1;
    viewportRows = viewportRowCount;
    scrollbackLines = scrollbackLineCount.value;
    blockPointers.realloc (static_cast<size_t> (viewportRowCount.value));
    head.at (0) = 0;
    head.at (1) = 0;
    numRows.at (0) = 0;
    numRows.at (1) = 0;
}

void Grid::resizeHeight (cell newRows, cell& cursorRow) noexcept
{
    jassert (isAllocated());

    const int oldVP { viewportRows.value };
    const int newVP { newRows.value };

    if (newVP != oldVP)
    {
        for (int screen { 0 }; screen < 2; ++screen)
        {
            if (newVP < oldVP)
            {
                // Shrink: eat empty from bottom, push remainder to scrollback.
                int needed { oldVP - newVP };

                // Eat empty rows below cursor (screen 0 only).
                if (screen == 0)
                {
                    const int belowCursor { oldVP - 1 - cursorRow.value };
                    const int available { juce::jmin (belowCursor, needed) };
                    needed -= available;
                }

                // Push remaining from top into scrollback by advancing head.
                if (needed > 0)
                {
                    head.at (screen) = (head.at (screen) + needed) & ringMask;
                    numRows.at (screen) = juce::jmin (numRows.at (screen) + needed, scrollbackLines);

                    if (screen == 0)
                        cursorRow = cell (cursorRow.value - needed);
                }
            }
            else
            {
                // Grow: pull from scrollback, fill remaining with blanks.
                int needed { newVP - oldVP };

                // Pull from scrollback.
                const int available { juce::jmin (numRows.at (screen), needed) };

                if (available > 0)
                {
                    numRows.at (screen) -= available;
                    head.at (screen) = (head.at (screen) - available) & ringMask;

                    if (screen == 0)
                        cursorRow = cell (cursorRow.value + available);
                }

                needed -= available;

                // Fill remaining with blanks at bottom of new viewport.
                for (int r { oldVP + available }; r < newVP; ++r)
                    buffer.clear (screen, (head.at (screen) + r) & ringMask);
            }
        }

        viewportRows = newRows;
    }
}

std::array<int, 2>
Grid::reflow (int newViewportRows, int newCols, int scrollbackLines, int& cursorRow, int& cursorCol, int& scrollOffset) noexcept
{
    jassert (isAllocated());

    const int oldRingMask { ringMask };
    const auto oldHead { head };
    const auto oldNumRows { numRows };
    const int oldViewportRows { viewportRows.value };

    // New ring geometry.
    const int minRing { (scrollbackLines + newViewportRows) * 2 };
    int newRingSize { 1 };
    while (newRingSize < minRing)
        newRingSize <<= 1;

    const int newRingMask { newRingSize - 1 };
    jam::Buffer<jam::Row> scratch;
    scratch.setSize (2, newRingSize, newCols, false, true, false);

    // Wrap cursor to paragraph-relative coordinates (screen 0 only).
    int wx { 0 };
    int wy { 0 };
    static constexpr int cursorScreen { 0 };
    wrapCursorPosition (buffer, cursorScreen, oldHead.at (cursorScreen),
                        oldNumRows.at (cursorScreen), oldRingMask,
                        cursorCol, cursorRow, wx, wy);

    // Reflow screen 0 (normal) only — alternate screen is NOT reflowed per PLAN.
    std::array<int, 2> result { 0, 0 };
    std::array<int, 2> newHead { 0, 0 };

    reflowScreen (0, oldHead.at (0), oldNumRows.at (0), oldViewportRows, oldRingMask,
                  newRingMask, newCols, newViewportRows, scrollbackLines,
                  buffer, scratch, newHead.at (0), result.at (0), scrollOffset);

    // Alternate screen: move rows as-is (no reflow, just resize).
    {
        const int altTotal { oldNumRows.at (1) + oldViewportRows };
        int altWrite { 0 };

        for (int i { 0 }; i < altTotal; ++i)
        {
            const int srcPhys { (oldHead.at (1) - oldNumRows.at (1) + i) & oldRingMask };
            const jam::Row* srcRow { buffer.getReadPointer (1, srcPhys) };

            if ((srcRow->flags & jam::Row::dead) == 0)
            {
                scratch.copyFrom (1, altWrite & newRingMask, buffer, 1, srcPhys);
                ++altWrite;
            }
        }

        while (altWrite < newViewportRows)
        {
            scratch.clear (1, altWrite & newRingMask);
            ++altWrite;
        }

        const int altComputed { juce::jmax (0, altWrite - newViewportRows) };
        result.at (1) = juce::jmin (altComputed, scrollbackLines);
        newHead.at (1) = altComputed & newRingMask;
    }

    // Copy scratch to live buffer.
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

    // Clamp scrollOffset to valid range after reflow.
    scrollOffset = juce::jlimit (0, result.at (0), scrollOffset);

    // Update Grid state.
    head = newHead;
    ringMask = newRingMask;
    viewportRows = cell (newViewportRows);
    this->scrollbackLines = scrollbackLines;
    numRows.at (0) = result.at (0);
    numRows.at (1) = result.at (1);
    blockPointers.realloc (static_cast<size_t> (newViewportRows));

    // Unwrap cursor back to viewport-relative coordinates.
    unwrapCursorPosition (buffer, cursorScreen, newHead.at (cursorScreen),
                          result.at (cursorScreen), newViewportRows, newRingMask,
                          newCols, wx, wy, cursorRow, cursorCol);

    return result;
}

// =============================================================================
// Scroll
// =============================================================================

int Grid::scrollUp (int screen, int scrollTop, int scrollBottom, int count) noexcept
{
    jassert (screen >= 0 and screen < 2);
    jassert (scrollTop >= 0 and scrollBottom < viewportRows.value and scrollTop <= scrollBottom);

    const int clampedCount { juce::jmin (count, scrollBottom - scrollTop + 1) };

    if (clampedCount > 0)
    {
        const bool isFullScreen { scrollTop == 0 and scrollBottom == viewportRows.value - 1 };

        if (isFullScreen)
        {
            for (int n { 0 }; n < clampedCount; ++n)
            {
                // Advance head — old viewport-top stays in place as history.
                head.at (screen) = (head.at (screen) + 1) & ringMask;

                // Clear new bottom viewport row.
                buffer.clear (screen, physicalRow (screen, viewportRows - cell (1)));

                // Grow history count — each head advance adds one row to history.
                numRows.at (screen) = juce::jmin (numRows.at (screen) + 1, scrollbackLines);
            }
        }
        else
        {
            for (int n { 0 }; n < clampedCount; ++n)
            {
                for (int r { scrollTop }; r < scrollBottom; ++r)
                    buffer.copyFrom (screen, physicalRow (screen, cell (r)), buffer, screen, physicalRow (screen, cell (r + 1)));

                buffer.clear (screen, physicalRow (screen, cell (scrollBottom)));
            }
        }
    }

    return clampedCount;
}

void Grid::scrollDown (int screen, int scrollTop, int scrollBottom, int count) noexcept
{
    jassert (screen >= 0 and screen < 2);
    jassert (scrollTop >= 0 and scrollBottom < viewportRows.value and scrollTop <= scrollBottom);

    const int clampedCount { juce::jmin (count, scrollBottom - scrollTop + 1) };

    if (clampedCount > 0)
    {
        const bool isFullScreen { scrollTop == 0 and scrollBottom == viewportRows.value - 1 };

        if (isFullScreen)
        {
            for (int n { 0 }; n < clampedCount; ++n)
            {
                // Retreat head — clear new top viewport row.
                head.at (screen) = (head.at (screen) - 1) & ringMask;
                buffer.clear (screen, physicalRow (screen, 0_cell));
            }
        }
        else
        {
            for (int n { 0 }; n < clampedCount; ++n)
            {
                for (int r { scrollBottom }; r > scrollTop; --r)
                    buffer.copyFrom (screen, physicalRow (screen, cell (r)), buffer, screen, physicalRow (screen, cell (r - 1)));

                buffer.clear (screen, physicalRow (screen, cell (scrollTop)));
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

    for (int r { 0 }; r < viewportRows.value; ++r)
        buffer.clear (screen, physicalRow (screen, cell (r)));

    numRows.at (screen) = 0;
}

void Grid::clear (int screen, cell row) noexcept
{
    jassert (screen >= 0 and screen < 2);
    jassert (row.value >= 0 and row.value < viewportRows.value);
    buffer.clear (screen, physicalRow (screen, row));
}

void Grid::clear (int screen, cell row, cell startCol, cell numCols) noexcept
{
    jassert (screen >= 0 and screen < 2);
    jassert (row.value >= 0 and row.value < viewportRows.value);

    const int clampedCount { juce::jmin (numCols.value, buffer.getNumCols() - startCol.value) };

    if (clampedCount > 0)
        buffer.clear (screen, physicalRow (screen, row), startCol.value, clampedCount);
}

// =============================================================================
// Asked — storage access only
// =============================================================================

jam::Row* Grid::getWritePointer (int screen, cell row) noexcept
{
    jassert (screen >= 0 and screen < 2);
    return buffer.getWritePointer (screen, physicalRow (screen, row));
}

const jam::Row* Grid::getRow (int screen, int absoluteIndex) const noexcept
{
    jassert (screen >= 0 and screen < 2);
    jassert (absoluteIndex >= 0 and absoluteIndex < numRows.at (screen) + viewportRows.value);
    return buffer.getReadPointer (screen, (head.at (screen) - numRows.at (screen) + absoluteIndex) & ringMask);
}

bool Grid::isAllocated() const noexcept { return ringMask > 0; }

int Grid::getNumRows (int screen) const noexcept
{
    jassert (screen >= 0 and screen < 2);
    return numRows.at (screen);
}

jam::Block<jam::Row> Grid::getBlock (int screen, cell scrollOffset, cell vpRows) const noexcept
{
    jassert (screen >= 0 and screen < 2);
    jassert (vpRows.value >= 0 and vpRows.value <= viewportRows.value);
    jassert (scrollOffset.value >= 0 and scrollOffset.value <= numRows.at (screen));

    if (scrollOffset.value == 0)
    {
        // Live mode — viewport rows in logical order.
        for (int r { 0 }; r < vpRows.value; ++r)
            blockPointers[r] = buffer.getReadPointer (screen, physicalRow (screen, cell (r)));
    }
    else
    {
        // History mode — absolute rows starting from scrollOffset.
        const int startIndex { numRows.at (screen) - scrollOffset.value };

        for (int r { 0 }; r < vpRows.value; ++r)
        {
            const int absIndex { startIndex + r };
            blockPointers[r] = buffer.getReadPointer (screen, (head.at (screen) - numRows.at (screen) + absIndex) & ringMask);
        }
    }

    return jam::Block<jam::Row> (blockPointers.getData(), vpRows.value, buffer.getNumCols());
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace terminal
