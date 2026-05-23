/**
 * @file Screen.cpp
 * @brief Row buffer renderer — full viewport repaint from State and Buffer.
 *
 * Screen registers as a ValueTree::Listener on State's SESSION root and grafts
 * its TextEditor node into that tree on construction.
 * On every parameter flush, valueTreePropertyChanged:
 *   1. Reads activeScreen, scrollOffset, and numRows from State.
 *   2. Constructs a Block<Row> view from reflowedContent (during transition) or
 *      buffer (live) for the visible window.
 *   3. Calls setText (block) — single call replaces the old per-row loop.
 *
 * Screen holds no scroll state. scrollOffset is read from State each flush.
 *
 * @see Screen.h
 * @see terminal::State         — SSOT for scrollOffset, activeScreen, visibleRows, numRows
 * @see jam::Buffer<jam::Row>   — row storage; Screen constructs Block view
 */

#include "Screen.h"

// ============================================================================
// Reflow: layout mapping model
// ============================================================================

/**
 * @brief Map a cell position from source width to dest width.
 *
 * Uses integer linear interpolation: newCol = col * destCols / sourceCols.
 * Monotonically non-decreasing — safe for sequential cell projection.
 */
static int mapPosition (const int col, const int sourceCols, const int destCols) noexcept
{
    return col * destCols / sourceCols;
}

// ----------------------------------------------------------------------------

/**
 * @brief Lossless reflow: reads rows from source, wraps/unwraps at dest column width, writes to dest.
 *
 * Normal screen (channel 0): reflows ALL content (history + viewport rows) using a
 * two-path layout mapping model:
 *
 *   EXPAND path (totalUsedCells <= destCols — content fits, possibly unwrapping):
 *     Maps each cell's flat position from the logical line's total source width to destCols.
 *     Items land at proportional positions; FLEX_GAP fills newly created gaps.
 *     Trailing empty space absorbs its proportional share of the extra width naturally.
 *
 *   CONTRACT path (totalUsedCells > destCols — content must wrap):
 *     Streams cells sequentially into dest rows, wrapping at destCols.
 *     FLEX_GAP runs contract proportionally (minimum 1 cell each).
 *     Items copy verbatim; wide chars wrap cleanly.
 *
 * Alternate screen (channel 1): copies viewport rows only (apps redraw after SIGWINCH).
 *
 * Source column width = source.getNumCols(); destination column width = dest.getNumCols().
 * dest must be pre-allocated by the caller.
 *
 * @param dest                 Pre-allocated destination buffer at new dimensions (cleared).
 * @param source               DST snapshot — pre-resize content.
 * @param scrollbackLines      Maximum history rows from config.
 * @param oldVisibleRows       Viewport rows before resize.
 * @param newVisibleRows       Viewport rows after resize.
 * @param numHistoryNormal     History row count for normal screen before resize.
 * @param numHistoryAlternate  History row count for alternate screen before resize.
 * @param cursorRow            Cursor row within the viewport before resize (0-based).
 *                             Content extent = numHistoryNormal + cursorRow + 1.
 * @return New history row count for normal screen after reflow.
 */
int terminal::Screen::reflow (jam::Buffer<jam::Row>& dest,
                              const jam::Buffer<jam::Row>& source,
                              int scrollbackLines,
                              int oldVisibleRows,
                              int newVisibleRows,
                              int numHistoryNormal,
                              int numHistoryAlternate,
                              int cursorRow) noexcept
{
    const int destCols { dest.getNumCols() };
    const int sourceCols { source.getNumCols() };
    const int totalSourceRows { numHistoryNormal + cursorRow + 1 };

    int srcRow { 0 };
    int dstRow { 0 };

    // ===== Normal screen (channel 0) =====

    while (srcRow < totalSourceRows and dstRow < scrollbackLines)
    {
        // Find logical line boundary (rows joined by flexWrap).
        const int lineStart { srcRow };

        while (srcRow < totalSourceRows and (source.getReadPointer (0, srcRow)->flags & jam::Row::flexWrap) != 0)
        {
            ++srcRow;
        }

        ++srcRow;// consume the terminating (non-wrapped) row
        const int lineEnd { srcRow };// exclusive
        const int numSrcRows { lineEnd - lineStart };

        // Check if logical line is entirely empty.
        bool allEmpty { true };

        for (int r { lineStart }; r < lineEnd and allEmpty; ++r)
            allEmpty = (source.getReadPointer (0, r)->usedCols == 0);

        if (allEmpty)
        {
            dest.clear (0, dstRow);
            ++dstRow;
        }
        else
        {
            // Compute total used cells across all wrapped rows of this logical line.
            int totalUsedCells { 0 };

            for (int r { lineStart }; r < lineEnd; ++r)
                totalUsedCells += static_cast<int> (source.getReadPointer (0, r)->usedCols);

            // Total virtual source width for this logical line.
            const int totalSourceWidth { numSrcRows * sourceCols };

            if (totalUsedCells <= destCols)
            {
                // === EXPAND path: all content fits on one dest row ===
                // Map SEGMENT boundaries (not individual cells) from source to dest width.
                // Items copy verbatim at mapped positions; gaps fill between items.
                auto* destRowPtr { dest.getWritePointer (0, dstRow) };
                int destCol { 0 };

                // Build flat segment list: (flatOffset, width, isGap) for items and gaps.
                // Then map each segment's start position proportionally.
                static constexpr int maxSegs { 341 };
                int segOffsets[maxSegs];
                int segWidths[maxSegs];
                bool segIsGap[maxSegs];
                int segCount { 0 };

                // --- Flatten source rows into segments ---
                int flatPos { 0 };
                bool inGap { false };

                for (int r { lineStart }; r < lineEnd; ++r)
                {
                    const auto* srcRowPtr { source.getReadPointer (0, r) };
                    const int used { static_cast<int> (srcRowPtr->usedCols) };

                    for (int c { 0 }; c < used; ++c)
                    {
                        const bool cellGap { srcRowPtr->cells[c].contentTag() == jam::Cell::FLEX_GAP };

                        if (segCount == 0)
                        {
                            segOffsets[0] = flatPos;
                            segWidths[0] = 1;
                            segIsGap[0] = cellGap;
                            segCount = 1;
                            inGap = cellGap;
                        }
                        else if (cellGap == inGap)
                        {
                            segWidths[segCount - 1] += 1;
                        }
                        else if (segCount < maxSegs)
                        {
                            segOffsets[segCount] = flatPos;
                            segWidths[segCount] = 1;
                            segIsGap[segCount] = cellGap;
                            ++segCount;
                            inGap = cellGap;
                        }

                        ++flatPos;
                    }
                }

                // --- Map segment boundaries and write ---
                // Source cursor for reading cells.
                int curSrcRow { lineStart };
                int curSrcCol { 0 };
                int curFlat { 0 };

                for (int s { 0 }; s < segCount; ++s)
                {
                    // Map this segment's start position from source width to dest width.
                    const int mappedStart { mapPosition (segOffsets[s], totalSourceWidth, destCols) };
                    // Ensure monotonic — never go backwards.
                    const int segStart { juce::jmax (mappedStart, destCol) };

                    if (segIsGap[s])
                    {
                        // Skip source gap cells.
                        const int gapEnd { segOffsets[s] + segWidths[s] };

                        while (curFlat < gapEnd)
                        {
                            const auto* sr { source.getReadPointer (0, curSrcRow) };

                            while (curSrcCol >= static_cast<int> (sr->usedCols) and curSrcRow + 1 < lineEnd)
                            {
                                ++curSrcRow;
                                curSrcCol = 0;
                            }

                            ++curSrcCol;
                            ++curFlat;
                        }

                        // Compute gap end: if there's a next segment, fill up to its mapped start.
                        // Otherwise fill up to current position (preserve original gap width proportionally).
                        int gapDestEnd { segStart + juce::jmax (1, segWidths[s]) };

                        if (s + 1 < segCount)
                        {
                            const int nextMapped { mapPosition (segOffsets[s + 1], totalSourceWidth, destCols) };
                            gapDestEnd = juce::jmax (segStart + 1, nextMapped);
                        }

                        // Write FLEX_GAP cells from destCol to gapDestEnd.
                        for (int g { destCol }; g < gapDestEnd and g < destCols; ++g)
                        {
                            destRowPtr->cells[g] = jam::Cell::make (0x20, jam::Cell::FLEX_GAP, jam::Cell::NARROW, 0);
                        }

                        destCol = juce::jmin (gapDestEnd, destCols);
                    }
                    else
                    {
                        // Fill any space before this item with FLEX_GAP (from previous item end).
                        for (int g { destCol }; g < segStart and g < destCols; ++g)
                        {
                            destRowPtr->cells[g] = jam::Cell::make (0x20, jam::Cell::FLEX_GAP, jam::Cell::NARROW, 0);
                        }

                        destCol = juce::jmax (destCol, segStart);

                        // Copy item cells verbatim.
                        const int itemEnd { segOffsets[s] + segWidths[s] };

                        while (curFlat < itemEnd and destCol < destCols)
                        {
                            const auto* sr { source.getReadPointer (0, curSrcRow) };

                            while (curSrcCol >= static_cast<int> (sr->usedCols) and curSrcRow + 1 < lineEnd)
                            {
                                ++curSrcRow;
                                curSrcCol = 0;
                                sr = source.getReadPointer (0, curSrcRow);
                            }

                            if (curSrcCol < static_cast<int> (source.getReadPointer (0, curSrcRow)->usedCols))
                            {
                                destRowPtr->cells[destCol] = source.getReadPointer (0, curSrcRow)->cells[curSrcCol];
                                ++destCol;
                                ++curSrcCol;
                                ++curFlat;
                            }
                            else
                            {
                                curFlat = itemEnd;
                            }
                        }
                    }
                }

                destRowPtr->usedCols = static_cast<uint16_t> (destCol);
                destRowPtr->flags = 0;
                ++dstRow;
            }
            else
            {
                // === CONTRACT path: content exceeds destCols — stream with wrap ===
                // FLEX_GAP runs contract proportionally (minimum 1 cell).
                // Items copy verbatim and wrap at dest row boundary.
                auto* destRowPtr { dest.getWritePointer (0, dstRow) };
                int destCol { 0 };

                for (int r { lineStart }; r < lineEnd and dstRow < scrollbackLines; ++r)
                {
                    const auto* srcRowPtr { source.getReadPointer (0, r) };
                    const int used { static_cast<int> (srcRowPtr->usedCols) };
                    int c { 0 };

                    while (c < used and dstRow < scrollbackLines)
                    {
                        // Wrap dest row when full.
                        if (destCol >= destCols)
                        {
                            destRowPtr->usedCols = static_cast<uint16_t> (destCol);
                            destRowPtr->flags = jam::Row::flexWrap;
                            ++dstRow;
                            destRowPtr = dest.getWritePointer (0, dstRow);
                            destCol = 0;
                        }

                        const jam::Cell& srcCell { srcRowPtr->cells[c] };

                        if (srcCell.contentTag() == jam::Cell::FLEX_GAP)
                        {
                            // Measure gap run length.
                            const int gapStart { c };

                            while (c < used and srcRowPtr->cells[c].contentTag() == jam::Cell::FLEX_GAP)
                            {
                                ++c;
                            }

                            const int gapLen { c - gapStart };
                            // Contract proportionally — minimum 1 cell preserved.
                            const int newGapLen { juce::jmax (1, gapLen * destCols / sourceCols) };

                            for (int g { 0 }; g < newGapLen and dstRow < scrollbackLines; ++g)
                            {
                                if (destCol >= destCols)
                                {
                                    destRowPtr->usedCols = static_cast<uint16_t> (destCol);
                                    destRowPtr->flags = jam::Row::flexWrap;
                                    ++dstRow;
                                    destRowPtr = dest.getWritePointer (0, dstRow);
                                    destCol = 0;
                                }

                                destRowPtr->cells[destCol] =
                                    jam::Cell::make (0x20, jam::Cell::FLEX_GAP, jam::Cell::NARROW, 0);
                                ++destCol;
                            }
                        }
                        else
                        {
                            // Content cell — handle wide char at last column.
                            if (destCol == destCols - 1 and srcCell.wide() == jam::Cell::WIDE)
                            {
                                // Wide char at last column: insert SPACER_HEAD, defer cell.
                                destRowPtr->cells[destCol] =
                                    jam::Cell::make (0, jam::Cell::CONTENT_CODEPOINT, jam::Cell::SPACER_HEAD, 0);
                                ++destCol;
                                // Do NOT advance c — cell will be copied on next iteration
                                // after wrapping to the new row.
                            }
                            else
                            {
                                destRowPtr->cells[destCol] = srcCell;
                                ++destCol;
                                ++c;

                                // Copy SPACER_TAIL immediately following a WIDE cell.
                                if (srcCell.wide() == jam::Cell::WIDE and c < used and destCol < destCols)
                                {
                                    destRowPtr->cells[destCol] = srcRowPtr->cells[c];
                                    ++destCol;
                                    ++c;
                                }
                            }
                        }
                    }
                }

                // Finalize the last dest row of this logical line.
                if (dstRow < scrollbackLines)
                {
                    destRowPtr->usedCols = static_cast<uint16_t> (destCol);
                    destRowPtr->flags = 0;// last row of logical line has no flexWrap
                    ++dstRow;
                }
            }
        }
    }

    // ===== Alternate screen (channel 1) — verbatim viewport copy (apps redraw) =====

    const int altViewportCopy { juce::jmin (oldVisibleRows, newVisibleRows) };

    for (int v { 0 }; v < altViewportCopy; ++v)
    {
        const int srcIdx { numHistoryAlternate + v };

        if (srcIdx < source.getNumRows())
            dest.copyFrom (1, v, source, 1, srcIdx);
    }

    return juce::jmax (0, dstRow - newVisibleRows);
}

namespace terminal
{
/*____________________________________________________________________________*/

Screen::Screen (State& stateMachine, jam::Buffer<jam::Row>& bufferToUse) noexcept
    : jam::TextEditor ({})
    , terminal (stateMachine)
    , buffer (bufferToUse)
    , transitioner (bufferToUse)
{
    setWantsKeyboardFocus (false);

    // Screen nodes (NORMAL, ALTERNATE) are owned and grafted by Display before
    // this constructor runs — they are already present in the tree here.
    terminal.get().addListener (this);
    terminal.get().appendChild (state, nullptr);
    transitioner.prepare();
}

Screen::~Screen()
{
    terminal.get().removeListener (this);

    // Screen nodes (NORMAL, ALTERNATE) are owned by Display — Display's destructor
    // removes them after Screen is destroyed.
    auto parentTree { state.getParent() };

    if (parentTree.isValid())
        parentTree.removeChild (state, nullptr);
}

// ============================================================================
// juce::ValueTree::Listener
// ============================================================================

void Screen::valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&)
{
    // --- Render path: read State, construct Block, setText, viewport, caret ---
    const int activeScreen { static_cast<int> (
        jam::ValueTree::getValueFromChildWithID (terminal.get(), id::activeScreen).getValue()) };

    // Read ALL values from VT — no atomic reads
    const juce::Identifier screenId { Map::Screen::getContext()->get (activeScreen) };
    auto screenNode { terminal.get().getChildWithName (screenId) };

    const cell viewportRows { static_cast<int> (
        jam::ValueTree::getValueFromChildWithID (terminal.get(), id::visibleRows).getValue()) };
    const cell cols { static_cast<int> (jam::ValueTree::getValueFromChildWithID (terminal.get(), id::cols).getValue()) };
    const cell scrollOffset { static_cast<int> (
        jam::ValueTree::getValueFromChildWithID (screenNode, id::scrollOffset).getValue()) };
    const cell numRows { static_cast<int> (
        jam::ValueTree::getValueFromChildWithID (screenNode, id::numRows).getValue()) };
    const cell cursorCol { static_cast<int> (
        jam::ValueTree::getValueFromChildWithID (screenNode, id::cursorCol).getValue()) };
    const cell cursorRow { static_cast<int> (
        jam::ValueTree::getValueFromChildWithID (screenNode, id::cursorRow).getValue()) };

    const int scrollbackLines { static_cast<int> (
        jam::ValueTree::getValueFromChildWithID (AppState::getContext()->get(), app::id::scrollbackLines).getValue()) };

    if (cols.value > 0 and viewportRows.value > 0 and scrollbackLines > 0)
    {
        const cell contentRows { numRows.value + viewportRows.value };
        const cell totalRows { juce::jmin (contentRows.value, scrollbackLines) };
        const cell startRow { contentRows.value - totalRows.value };
        const cell historyRows { totalRows.value - viewportRows.value };

        // Ring-aware start: history rows are before head, wrapping at high logical indices.
        // reflowedContent has head=0 (reflow writes from physical 0), so live path only.
        const int ringSize { buffer.getNumRows() };
        const int liveStartRow { (ringSize - historyRows.value) % ringSize };

        // During transition: render from reflowedContent. Else: render from live buffer.
        if (transitioner.isInTransition() and reflowedContent.getNumRows() > 0)
        {
            const juce::ScopedLock sl (reflowLock);
            const jam::Block<jam::Row> block (reflowedContent, activeScreen, startRow.value, totalRows.value);

            setText (block);
        }
        else
        {
            const jam::Block<jam::Row> block (buffer, activeScreen, liveStartRow, totalRows.value);

            setText (block);
        }

        const auto scrollPos { jam::Cell::Point (0_cell, cell (historyRows.value - scrollOffset.value)) };
        setViewportPosition (0, scrollPos.toLogical<int> (font.bounds).y);
        setCaretPosition (cursorCol, cell (historyRows.value + cursorRow.value));
    }
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace terminal
