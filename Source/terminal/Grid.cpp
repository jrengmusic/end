/**
 * @file Grid.cpp
 * @brief Implementation of the ring-indexed terminal frame buffer.
 *
 * @see Grid.h for design notes, ring index model, and API documentation.
 */

#include "Grid.h"
#include "../Map.h"

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

        // Check if first cell fits — WIDE needs 2 columns.
        const int firstWidth { (srcRow->cells[0].wide() == jam::Cell::WIDE) ? 2 : 1 };

        if (at + firstWidth > newCols)
            break;

        // Strip any SPACER_HEAD padding from the target row end before appending.
        if (at > 0 and targetRow->cells[at - 1].wide() == jam::Cell::SPACER_HEAD)
            --at;

        // Copy cells from source into target, keeping WIDE+SPACER_TAIL pairs intact.
        lastSrc = srcRow;
        const int srcUsed { static_cast<int> (srcRow->usedCols) };
        want = 0;

        for (int c { 0 }; c < srcUsed; ++c)
        {
            // WIDE needs room for itself + its SPACER_TAIL; all other cells need 1.
            const int cellWidth { (srcRow->cells[c].wide() == jam::Cell::WIDE) ? 2 : 1 };

            if (at + cellWidth > newCols)
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

        // Adjust scroll offset (tmux hscrolled logic). Live mode (0) stays 0.
        if (scrollOffset > 0)
        {
            if (scrollOffset > to + lines)
                scrollOffset -= lines;
            else if (scrollOffset > to)
                scrollOffset = to;
        }
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

    // Chunk source into newCols-width rows, respecting WIDE+SPACER_TAIL pairs.
    int srcOffset { 0 };
    const int startWriteIdx { writeIdx };

    while (srcOffset < usedCols)
    {
        jam::Row* destRow { scratch.getWritePointer (screen, writeIdx & newMask) };
        int destCells { 0 };
        int destWidth { 0 };
        int consumed  { 0 };

        while (srcOffset + consumed < usedCols)
        {
            const jam::Cell& srcCell { srcRow->cells[srcOffset + consumed] };
            const uint8_t w { srcCell.wide() };
            // WIDE claims 2 display columns; SPACER_TAIL shares its WIDE cell's width (0 extra).
            const int cellWidth { (w == jam::Cell::WIDE) ? 2 : (w == jam::Cell::SPACER_TAIL) ? 0 : 1 };

            if (destWidth + cellWidth > newCols)
                break;

            destRow->cells[destCells] = srcCell;
            destWidth += cellWidth;
            ++destCells;
            ++consumed;
        }

        // A WIDE char didn't fit and there is exactly 1 column remaining — insert SPACER_HEAD padding.
        if (consumed < (usedCols - srcOffset) and destWidth < newCols)
        {
            const jam::Cell& nextCell { srcRow->cells[srcOffset + consumed] };

            if (nextCell.wide() == jam::Cell::WIDE)
            {
                destRow->cells[destCells] = jam::Cell::make (0, jam::Cell::CONTENT_CODEPOINT,
                                                              jam::Cell::SPACER_HEAD, 0);
                ++destCells;
            }
        }

        destRow->usedCols = static_cast<uint16_t> (destCells);
        destRow->flags    = 0;
        srcOffset += consumed;

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
    // Use actual write count — uniform cell-width formula is wrong for WIDE pairs.
    const int numChunks { writeIdx - startWriteIdx };

    if (scrollOffset > 0 and yy <= scrollOffset)
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
    head.at (Map::Screen::normal)    = 0;
    head.at (Map::Screen::alternate) = 0;
    numRows.at (Map::Screen::normal)    = 0;
    numRows.at (Map::Screen::alternate) = 0;
}

void Grid::resizeHeight (cell newRows, cell& cursorRow) noexcept
{
    jassert (isAllocated());

    const int oldVP { viewportRows.value };
    const int newVP { newRows.value };

    if (newVP != oldVP)
    {
        for (int screen { 0 }; screen < Map::Screen::count; ++screen)
        {
            if (newVP < oldVP)
            {
                // Shrink: eat empty from bottom, push remainder to scrollback.
                int needed { oldVP - newVP };

                // Eat empty rows below cursor (normal screen only).
                if (screen == Map::Screen::normal)
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

                    if (screen == Map::Screen::normal)
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

                    if (screen == Map::Screen::normal)
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
Grid::reflowFrom (const jam::Buffer<jam::Row>& source,
                   const std::array<int, 2>& sourceHead,
                   const std::array<int, 2>& sourceNumRows,
                   int sourceRingMask, int sourceViewportRows,
                   int newViewportRows, int newCols, int scrollbackLines,
                   int& cursorRow, int& cursorCol, int& scrollOffset) noexcept
{
    jassert (isAllocated());

    // resizeHeight is NOT called here — caller is responsible.
    // tmux order: screen_resize_y THEN grid_reflow on the SAME data.
    // For snapshot-based reflowFrom, resizeHeight must run before captureSnapshot
    // so the snapshot contains height-adjusted state. If called here, resizeHeight
    // modifies live state that reflowFrom overwrites — its effects are clobbered.

    // New ring geometry.
    const int minRing { (scrollbackLines + newViewportRows) * 2 };
    int newRingSize { 1 };
    while (newRingSize < minRing)
        newRingSize <<= 1;

    const int newRingMask { newRingSize - 1 };
    jam::Buffer<jam::Row> scratch;
    scratch.setSize (2, newRingSize, newCols, false, true, false);

    // Mutable working copy of source — static helpers mark consumed rows dead,
    // but the caller's snapshot must remain untouched.
    jam::Buffer<jam::Row> workCopy;
    {
        const int sourceRingSize { sourceRingMask + 1 };
        workCopy.setSize (2, sourceRingSize, source.getNumCols(), false, true, false);

        for (int screen { 0 }; screen < Map::Screen::count; ++screen)
        {
            const int totalRows { sourceNumRows.at (screen) + sourceViewportRows };

            for (int row { 0 }; row < totalRows; ++row)
            {
                const int physRow { (sourceHead.at (screen) - sourceNumRows.at (screen) + row) & sourceRingMask };
                workCopy.copyFrom (screen, physRow, source, screen, physRow);
            }
        }
    }

    // Wrap cursor to paragraph-relative coordinates (normal screen only).
    // Reads from source (the old layout snapshot).
    int wx { 0 };
    int wy { 0 };
    static constexpr int cursorScreen { Map::Screen::normal };
    wrapCursorPosition (source, cursorScreen, sourceHead.at (cursorScreen),
                        sourceNumRows.at (cursorScreen), sourceRingMask,
                        cursorCol, cursorRow, wx, wy);

    // Reflow normal screen — alternate screen rows are moved as-is.
    std::array<int, 2> result { 0, 0 };
    std::array<int, 2> newHead { 0, 0 };

    reflowScreen (Map::Screen::normal,
                  sourceHead.at (Map::Screen::normal), sourceNumRows.at (Map::Screen::normal),
                  sourceViewportRows, sourceRingMask,
                  newRingMask, newCols, newViewportRows, scrollbackLines,
                  workCopy, scratch,
                  newHead.at (Map::Screen::normal), result.at (Map::Screen::normal), scrollOffset);

    // Alternate screen: move rows as-is (no reflow, just resize).
    {
        const int altTotal { sourceNumRows.at (Map::Screen::alternate) + sourceViewportRows };
        int altWrite { 0 };

        for (int i { 0 }; i < altTotal; ++i)
        {
            const int srcPhys { (sourceHead.at (Map::Screen::alternate) - sourceNumRows.at (Map::Screen::alternate) + i) & sourceRingMask };
            const jam::Row* srcRow { workCopy.getReadPointer (Map::Screen::alternate, srcPhys) };

            if ((srcRow->flags & jam::Row::dead) == 0)
            {
                scratch.copyFrom (Map::Screen::alternate, altWrite & newRingMask, workCopy, Map::Screen::alternate, srcPhys);
                ++altWrite;
            }
        }

        while (altWrite < newViewportRows)
        {
            scratch.clear (Map::Screen::alternate, altWrite & newRingMask);
            ++altWrite;
        }

        const int altComputed { juce::jmax (0, altWrite - newViewportRows) };
        result.at (Map::Screen::alternate) = juce::jmin (altComputed, scrollbackLines);
        newHead.at (Map::Screen::alternate) = altComputed & newRingMask;
    }

    // Copy scratch to live buffer.
    buffer.setSize (2, newRingSize, newCols, false, true, false);

    for (int screen { 0 }; screen < Map::Screen::count; ++screen)
    {
        const int totalRows { result.at (screen) + newViewportRows };

        for (int row { 0 }; row < totalRows; ++row)
        {
            const int physRow { (newHead.at (screen) - result.at (screen) + row) & newRingMask };
            buffer.copyFrom (screen, physRow, scratch, screen, physRow);
        }
    }

    // Clamp scrollOffset to valid range after reflow.
    scrollOffset = juce::jlimit (0, result.at (Map::Screen::normal), scrollOffset);

    // Update Grid state.
    head = newHead;
    ringMask = newRingMask;
    viewportRows = cell (newViewportRows);
    this->scrollbackLines = scrollbackLines;
    numRows.at (Map::Screen::normal)    = result.at (Map::Screen::normal);
    numRows.at (Map::Screen::alternate) = result.at (Map::Screen::alternate);
    blockPointers.realloc (static_cast<size_t> (newViewportRows));

    // Unwrap cursor back to viewport-relative coordinates.
    // Reads from live buffer after copy-back (the new layout).
    unwrapCursorPosition (buffer, cursorScreen, newHead.at (cursorScreen),
                          result.at (cursorScreen), newViewportRows, newRingMask,
                          newCols, wx, wy, cursorRow, cursorCol);

    return result;
}

std::array<int, 2>
Grid::reflow (int newViewportRows, int newCols, int scrollbackLines, int& cursorRow, int& cursorCol, int& scrollOffset) noexcept
{
    jassert (isAllocated());

    // tmux order: screen_resize_y before grid_reflow, on the SAME grid.
    if (newViewportRows != viewportRows.value)
    {
        cell cursorRowCell { cursorRow };
        resizeHeight (cell (newViewportRows), cursorRowCell);
        cursorRow = cursorRowCell.value;
    }

    return reflowFrom (buffer, head, numRows, ringMask, viewportRows.value,
                        newViewportRows, newCols, scrollbackLines,
                        cursorRow, cursorCol, scrollOffset);
}

// =============================================================================
// Scroll
// =============================================================================

int Grid::scrollUp (int screen, int scrollTop, int scrollBottom, int count) noexcept
{
    jassert (screen >= 0 and screen < Map::Screen::count);
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
    jassert (screen >= 0 and screen < Map::Screen::count);
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
    jassert (screen >= 0 and screen < Map::Screen::count);

    for (int r { 0 }; r < viewportRows.value; ++r)
        buffer.clear (screen, physicalRow (screen, cell (r)));

    numRows.at (screen) = 0;
}

void Grid::clear (int screen, cell row) noexcept
{
    jassert (screen >= 0 and screen < Map::Screen::count);
    jassert (row.value >= 0 and row.value < viewportRows.value);
    buffer.clear (screen, physicalRow (screen, row));
}

void Grid::clear (int screen, cell row, cell startCol, cell numCols) noexcept
{
    jassert (screen >= 0 and screen < Map::Screen::count);
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
    jassert (screen >= 0 and screen < Map::Screen::count);
    return buffer.getWritePointer (screen, physicalRow (screen, row));
}

const jam::Row* Grid::getRow (int screen, int absoluteIndex) const noexcept
{
    jassert (screen >= 0 and screen < Map::Screen::count);
    jassert (absoluteIndex >= 0 and absoluteIndex < numRows.at (screen) + viewportRows.value);
    return buffer.getReadPointer (screen, (head.at (screen) - numRows.at (screen) + absoluteIndex) & ringMask);
}

bool Grid::isAllocated() const noexcept { return ringMask > 0; }

int Grid::getNumRows (int screen) const noexcept
{
    jassert (screen >= 0 and screen < Map::Screen::count);
    return numRows.at (screen);
}

int Grid::getRingMask() const noexcept { return ringMask; }

int Grid::getHeadPosition (int screen) const noexcept
{
    jassert (screen >= 0 and screen < Map::Screen::count);
    return head.at (screen);
}

cell Grid::getViewportRows() const noexcept { return viewportRows; }

const jam::Buffer<jam::Row>& Grid::getBuffer() const noexcept { return buffer; }

jam::Block<jam::Row> Grid::getBlock (int screen, cell scrollOffset, cell vpRows) const noexcept
{
    jassert (screen >= 0 and screen < Map::Screen::count);

    if (scrollOffset.value == 0)
    {
        // Live mode — viewport rows in logical order.
        for (int r { 0 }; r < vpRows.value; ++r)
            blockPointers[r] = buffer.getReadPointer (screen, physicalRow (screen, cell (r)));  // juce::HeapBlock has no .at() — index verified by loop bound
    }
    else
    {
        // History mode — absolute rows starting from scrollOffset.
        const int startIndex { numRows.at (screen) - scrollOffset.value };

        for (int r { 0 }; r < vpRows.value; ++r)
        {
            const int absIndex { startIndex + r };
            blockPointers[r] = buffer.getReadPointer (screen, (head.at (screen) - numRows.at (screen) + absIndex) & ringMask);  // juce::HeapBlock has no .at() — index verified by loop bound
        }
    }

    return jam::Block<jam::Row> (blockPointers.getData(), vpRows.value, buffer.getNumCols());
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace terminal
