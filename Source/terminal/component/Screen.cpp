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

/**
 * @brief Lossless reflow: reads rows from source, wraps/unwraps at dest column width, writes to dest.
 *
 * Normal screen (channel 0): reflows ALL content (history + viewport rows).
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
    const int newCols { dest.getNumCols() };

    jam::debug::Log::write ("[Screen::reflow] srcCols=" + juce::String (source.getNumCols())
                            + " newCols=" + juce::String (newCols)
                            + " oldVisRows=" + juce::String (oldVisibleRows)
                            + " newVisRows=" + juce::String (newVisibleRows)
                            + " histNormal=" + juce::String (numHistoryNormal)
                            + " histAlt=" + juce::String (numHistoryAlternate)
                            + " cursorRow=" + juce::String (cursorRow)
                            + " scrollback=" + juce::String (scrollbackLines));

    // ===== Normal screen (channel 0) — reflow content up to and including cursor row =====
    // Rows below cursorRow are viewport padding — not reflowed, not preserved.

    const int totalSourceRows { numHistoryNormal + cursorRow + 1 };
    int srcRow { 0 };
    int dstRow { 0 };
    int diagLineCount { 0 }; // DIAG

    while (srcRow < totalSourceRows and dstRow < scrollbackLines)
    {
        // --- Build logical line by joining consecutive wrapped rows ---
        const int lineStart { srcRow };
        int totalCells { 0 };
        bool lastWrapped { true };

        while (srcRow < totalSourceRows and lastWrapped)
        {
            const auto* row { source.getReadPointer (0, srcRow) };
            totalCells += static_cast<int> (row->usedCols);
            lastWrapped = (row->flags & jam::Row::wrapped) != 0;
            ++srcRow;
        }

        if (totalCells == 0)
        {
            if (diagLineCount < 30) // DIAG
            {                       // DIAG
                jam::debug::Log::write ("[Screen::reflow] line srcRows " + juce::String (lineStart) + "-" + juce::String (srcRow - 1) // DIAG
                                        + " totalCells=0 action=empty dstRow=" + juce::String (dstRow));                               // DIAG
                ++diagLineCount;                                                                                                         // DIAG
            }                                                                                                                           // DIAG

            // Empty logical line — one blank row
            dest.clear (0, dstRow);
            ++dstRow;
        }
        else if (totalCells <= newCols)
        {
            if (diagLineCount < 30) // DIAG
            {                       // DIAG
                jam::debug::Log::write ("[Screen::reflow] line srcRows " + juce::String (lineStart) + "-" + juce::String (srcRow - 1)        // DIAG
                                        + " totalCells=" + juce::String (totalCells) + " action=unwrap dstRow=" + juce::String (dstRow));     // DIAG
                ++diagLineCount;                                                                                                               // DIAG
            }                                                                                                                                 // DIAG

            // Fits in a single row — unwrap
            auto* destRow { dest.getWritePointer (0, dstRow) };
            int destCol { 0 };

            for (int r { lineStart }; r < srcRow; ++r)
            {
                const auto* src { source.getReadPointer (0, r) };
                const int used { static_cast<int> (src->usedCols) };

                for (int c { 0 }; c < used; ++c)
                    destRow->cells[destCol++] = src->cells[c];
            }

            // Upsize padding: if logical line was full-width at source cols and
            // fits in newCols with room to spare, expand interior whitespace runs.
            // This handles prompts with left-pinned and right-pinned content.
            const int srcCols { source.getNumCols() };
            const int extraCols { newCols - destCol };

            if (extraCols > 0 and destCol == srcCols and destCol > 0)
            {
                // Find first and last non-blank cell
                int firstContent { 0 };
                int lastContent { destCol - 1 };

                while (firstContent < destCol and destRow->cells[firstContent].codepoint() <= 0x20)
                    ++firstContent;

                while (lastContent > firstContent and destRow->cells[lastContent].codepoint() <= 0x20)
                    --lastContent;

                // Count interior whitespace runs (>= 2 consecutive blanks between content)
                int totalInteriorWhitespace { 0 };
                int numRuns { 0 };

                int scanPos { firstContent + 1 };

                while (scanPos <= lastContent)
                {
                    if (destRow->cells[scanPos].codepoint() <= 0x20)
                    {
                        const int runStart { scanPos };

                        while (scanPos <= lastContent and destRow->cells[scanPos].codepoint() <= 0x20)
                            ++scanPos;

                        const int runLen { scanPos - runStart };

                        if (runLen >= 2)
                        {
                            totalInteriorWhitespace += runLen;
                            ++numRuns;
                        }
                    }
                    else
                    {
                        ++scanPos;
                    }
                }

                // Expand runs proportionally into a temporary cell array
                if (numRuns > 0 and totalInteriorWhitespace > 0)
                {
                    static constexpr int maxTerminalCols { 1024 };
                    jassert (newCols <= maxTerminalCols);

                    jam::Cell padded[maxTerminalCols];
                    int srcC { 0 };
                    int dstC { 0 };
                    int extraRemaining { extraCols };
                    int wsRemaining { totalInteriorWhitespace };
                    bool inInterior { false };

                    while (srcC < destCol and dstC < newCols)
                    {
                        if (srcC >= firstContent and srcC <= lastContent)
                            inInterior = true;

                        if (inInterior and destRow->cells[srcC].codepoint() <= 0x20)
                        {
                            const int runStart { srcC };

                            while (srcC < destCol and srcC <= lastContent and destRow->cells[srcC].codepoint() <= 0x20)
                                ++srcC;

                            const int runLen { srcC - runStart };

                            if (runLen >= 2 and wsRemaining > 0)
                            {
                                // Proportional extra: (runLen / totalWS) * extraCols
                                const int extra { (extraRemaining * runLen + wsRemaining - 1) / wsRemaining };
                                const int expandedLen { runLen + extra };
                                wsRemaining -= runLen;
                                extraRemaining -= extra;

                                const jam::Cell blank { jam::Cell::make (0x20, jam::Cell::CONTENT_CODEPOINT, jam::Cell::NARROW, 0) };

                                for (int w { 0 }; w < expandedLen and dstC < newCols; ++w)
                                    padded[dstC++] = blank;
                            }
                            else
                            {
                                for (int w { runStart }; w < srcC and dstC < newCols; ++w)
                                    padded[dstC++] = destRow->cells[w];
                            }
                        }
                        else
                        {
                            padded[dstC++] = destRow->cells[srcC++];
                        }
                    }

                    for (int c { 0 }; c < dstC; ++c)
                        destRow->cells[c] = padded[c];

                    destCol = dstC;
                }
            }

            destRow->usedCols = static_cast<uint16_t> (destCol);
            destRow->flags = 0;
            ++dstRow;
        }
        else
        {
            if (diagLineCount < 30) // DIAG
            {                       // DIAG
                jam::debug::Log::write ("[Screen::reflow] line srcRows " + juce::String (lineStart) + "-" + juce::String (srcRow - 1)     // DIAG
                                        + " totalCells=" + juce::String (totalCells) + " action=wrap dstRow=" + juce::String (dstRow));   // DIAG
                ++diagLineCount;                                                                                                            // DIAG
            }                                                                                                                              // DIAG

            // Needs wrapping at newCols — stream cells from source rows into dest rows
            int curSrcR { lineStart };
            int curSrcC { 0 };
            int remaining { totalCells };

            while (remaining > 0 and dstRow < scrollbackLines)
            {
                auto* destRow { dest.getWritePointer (0, dstRow) };
                int destCol { 0 };

                while (destCol < newCols and remaining > 0)
                {
                    // Advance past exhausted source rows
                    const auto* curSrc { source.getReadPointer (0, curSrcR) };

                    while (curSrcC >= static_cast<int> (curSrc->usedCols) and curSrcR + 1 < srcRow)
                    {
                        ++curSrcR;
                        curSrcC = 0;
                        curSrc = source.getReadPointer (0, curSrcR);
                    }

                    if (curSrcC < static_cast<int> (curSrc->usedCols))
                    {
                        // Wide char boundary: if WIDE char at last column, insert SPACER_HEAD
                        if (destCol == newCols - 1
                            and curSrc->cells[curSrcC].wide() == jam::Cell::WIDE)
                        {
                            destRow->cells[destCol] = jam::Cell::make (0, jam::Cell::CONTENT_CODEPOINT,
                                                                       jam::Cell::SPACER_HEAD, 0);
                            ++destCol;
                        }
                        else
                        {
                            destRow->cells[destCol] = curSrc->cells[curSrcC];
                            ++destCol;
                            ++curSrcC;
                            --remaining;

                            // Copy SPACER_TAIL for wide chars
                            if (destRow->cells[destCol - 1].wide() == jam::Cell::WIDE
                                and destCol < newCols
                                and curSrcC < static_cast<int> (curSrc->usedCols))
                            {
                                destRow->cells[destCol] = curSrc->cells[curSrcC];
                                ++destCol;
                                ++curSrcC;
                                --remaining;
                            }
                        }
                    }
                }

                destRow->usedCols = static_cast<uint16_t> (destCol);
                destRow->flags = (remaining > 0) ? jam::Row::wrapped : 0;
                ++dstRow;
            }
        }
    }

    // newHistoryNormal = total reflowed rows minus viewport rows the shell will redraw
    const int newHistoryNormal { juce::jmax (0, dstRow - newVisibleRows) };

    // ===== Alternate screen (channel 1) — copy viewport rows only (apps redraw) =====
    const int altViewportCopy { juce::jmin (oldVisibleRows, newVisibleRows) };

    for (int v { 0 }; v < altViewportCopy; ++v)
    {
        const int srcIdx { numHistoryAlternate + v };

        if (srcIdx < source.getNumRows())
            dest.copyFrom (1, v, source, 1, srcIdx);
    }

    jam::debug::Log::write ("[Screen::reflow] done srcRow=" + juce::String (srcRow) + " dstRow=" + juce::String (dstRow) // DIAG
                            + " totalSource=" + juce::String (totalSourceRows)                                             // DIAG
                            + " newHistNormal=" + juce::String (newHistoryNormal)                                          // DIAG
                            + " altCopied=" + juce::String (altViewportCopy));                                            // DIAG

    return newHistoryNormal;
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
    const cell cols { static_cast<int> (
        jam::ValueTree::getValueFromChildWithID (terminal.get(), id::cols).getValue()) };
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

    jam::debug::Log::write ("[Screen::vTPC] active=" + juce::String (activeScreen)       // DIAG
                            + " vpRows=" + juce::String (viewportRows.value)              // DIAG
                            + " numCols=" + juce::String (cols.value)                     // DIAG
                            + " scrollOff=" + juce::String (scrollOffset.value)           // DIAG
                            + " numRows=" + juce::String (numRows.value)                  // DIAG
                            + " scrollback=" + juce::String (scrollbackLines));           // DIAG

    if (cols.value > 0 and viewportRows.value > 0 and scrollbackLines > 0)
    {
        const cell contentRows { numRows.value + viewportRows.value };
        const cell totalRows { juce::jmin (contentRows.value, scrollbackLines) };
        const cell startRow { contentRows.value - totalRows.value };
        const cell historyRows { totalRows.value - viewportRows.value };

        // During transition: render from reflowedContent. Else: render from live buffer.
        if (transitioner.isInTransition() and reflowedContent.getNumRows() > 0)
        {
            const jam::Block<jam::Row> block (reflowedContent, activeScreen, startRow.value, totalRows.value);

            jam::debug::Log::write ("[Screen::vTPC] TRANSITION totalRows=" + juce::String (totalRows.value) // DIAG
                                    + " startRow=" + juce::String (startRow.value)                           // DIAG
                                    + " ch=" + juce::String (activeScreen));                                 // DIAG

            setText (block);
        }
        else
        {
            const jam::Block<jam::Row> block (buffer, activeScreen, startRow.value, totalRows.value);

            jam::debug::Log::write ("[Screen::vTPC] LIVE totalRows=" + juce::String (totalRows.value) // DIAG
                                    + " startRow=" + juce::String (startRow.value)                     // DIAG
                                    + " ch=" + juce::String (activeScreen));                           // DIAG

            setText (block);
        }

        const auto scrollPos { jam::Cell::Point (0_cell, cell (historyRows.value - scrollOffset.value)) };
        setViewportPosition (0, scrollPos.toLogical<int> (font.bounds).y);
        setCaretPosition (cursorCol, cell (historyRows.value + cursorRow.value));
    }
    else
    {
        jam::debug::Log::write ("[Screen::vTPC] GUARD FAILED numCols=" + juce::String (cols.value)   // DIAG
                                + " vpRows=" + juce::String (viewportRows.value)                       // DIAG
                                + " scrollback=" + juce::String (scrollbackLines));                    // DIAG
    }
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace terminal
