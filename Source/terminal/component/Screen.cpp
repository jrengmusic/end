/**
 * @file Screen.cpp
 * @brief Row buffer renderer — owns double-buffered Buffer<Row>, atomic Block swap, renders ring into TextEditor.
 *
 * Screen owns two jam::Buffer<Row> instances and two pairs of jam::Block<Row> views.
 * On resize, Screen builds new blocks on the inactive set then swaps the atomic pointer
 * so Video loads the new blocks lock-free at the next process() entry.
 * Display parents Screen for rendering via addAndMakeVisible.
 *
 * On every parameter flush, valueTreePropertyChanged:
 *   1. Reads activeScreen, scrollOffset, and writeHead from State.
 *   2. Constructs a transient Block<Row> view from the live buffer at the flushed head.
 *   3. Calls setText (block) — TextEditor wraps at current display width.
 *
 * Screen holds no scroll state. scrollOffset is read from State each flush.
 *
 * @see Screen.h
 * @see terminal::State         — SSOT for scrollOffset, activeScreen, visibleRows, writeHead
 * @see jam::Buffer<jam::Row>   — row storage owned by Screen
 * @see jam::Block<jam::Row>    — per-channel view into Buffer; shared with Video via atomic pointer
 */

#include "Screen.h"

namespace terminal
{
/*____________________________________________________________________________*/

/**
 * @brief Constructs Screen, allocates buffers[0], constructs blockSets[0], and sets activeBlocks.
 *
 * Reads scrollbackLines from AppState to determine the initial ring size.
 * Buffer is allocated with 2 channels, (scrollbackLines + rows) ring depth, and cols width.
 * Block views at index 0 (normal) and 1 (alternate) are constructed from buffers[0].
 * activeBlocks is stored as blockSets[0].data() — the atomic Video will load.
 *
 * Screen nodes (NORMAL, ALTERNATE) are owned and grafted by Display before this
 * constructor runs — they are already present in the tree.
 *
 * @param stateMachine  Terminal parameter store — owned by Session.
 * @param cols          Initial column count.
 * @param rows          Initial row count.
 * @note MESSAGE THREAD.
 */
Screen::Screen (State& stateMachine, cell cols, cell rows) noexcept
    : jam::TextEditor ({})
    , terminalState (stateMachine.get())
{
    const int scrollbackLines { AppState::getContext()->getRawParameterValue<int> (app::id::scrollbackLines)->load() };
    const int ringSize { scrollbackLines + rows.value };
    buffers[0].setSize (2, ringSize, cols.value);

    blockSets[0].at (0) = jam::Block<jam::Row> (buffers[0], 0);
    blockSets[0].at (1) = jam::Block<jam::Row> (buffers[0], 1);
    activeBlocks.store (blockSets[0].data(), std::memory_order_release);

    setWantsKeyboardFocus (false);

    // Screen nodes (NORMAL, ALTERNATE) are owned and grafted by Display before
    // this constructor runs — they are already present in the tree here.
    terminalState.addListener (this);
    terminalState.appendChild (state, nullptr);
}

Screen::~Screen() { terminalState.removeListener (this); }

// ============================================================================

/**
 * @brief Resizes buffers with content preservation.
 *
 * Allocates buffers[nextIndex] with new dimensions, copies content from
 * buffers[activeIndex] per-channel in ring order (oldest row first),
 * constructs new blockSets, swaps activeBlocks, and updates activeIndex.
 *
 * The head for each channel is positioned at the end of the copied content.
 * Empty rows (when old ring was not full) copy as zeros — Screen renders
 * only as many rows as WriteHead.historyRows + viewportRows, so empty rows
 * beyond that boundary are never displayed.
 *
 * @note MESSAGE THREAD — called under suspendProcessing.
 */
std::array<int, 2> Screen::resizeBuffers (int newRingSize, int newCols, const std::array<int, 2>& writePositions) noexcept
{
    const int nextIndex { 1 - activeIndex };
    buffers[nextIndex].setSize (2, newRingSize, newCols);

    const jam::Buffer<jam::Row>& oldBuffer { buffers[activeIndex] };
    const int oldRingSize { oldBuffer.getNumRows() };
    const int rowsToCopy { juce::jmin (oldRingSize, newRingSize) };

    std::array<int, 2> newWritePositions {};

    for (int ch { 0 }; ch < 2; ++ch)
    {
        const int wp { writePositions.at (static_cast<size_t> (ch)) };

        // Copy in ring order: oldest row first (wp % oldRingSize), newest last.
        for (int r { 0 }; r < rowsToCopy; ++r)
        {
            const int srcRow { (wp + r) % oldRingSize };
            buffers[nextIndex].copyFrom (ch, r, oldBuffer, ch, srcRow);
        }

        // New head is at the end of copied content.
        newWritePositions.at (static_cast<size_t> (ch)) = rowsToCopy % newRingSize;
        buffers[nextIndex].setHead (ch, newWritePositions.at (static_cast<size_t> (ch)));
    }

    blockSets[nextIndex].at (0) = jam::Block<jam::Row> (buffers[nextIndex], 0);
    blockSets[nextIndex].at (1) = jam::Block<jam::Row> (buffers[nextIndex], 1);
    activeBlocks.store (blockSets[nextIndex].data(), std::memory_order_release);
    activeIndex = nextIndex;

    return newWritePositions;
}

// ============================================================================
// juce::ValueTree::Listener
// ============================================================================

void Screen::valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&)
{
    const int activeScreen { getValue (terminalState, id::activeScreen) };
    const juce::Identifier screenId { Map::Screen::getContext()->get (activeScreen) };
    auto screenNode { terminalState.getChildWithName (screenId) };

    const auto viewportSize { jam::Bounds::unpack (static_cast<int> (state.getProperty (jam::TextEditor::properties.at (jam::TextEditor::viewportId)))) };
    const cell viewportRows { viewportSize.height };

    const cell scrollOffset { getValue (screenNode, id::scrollOffset) };
    const jam::WriteHead wh { jam::WriteHead::unpack (getValue (screenNode, id::writeHead)) };
    const CursorState cursorState { CursorState::unpack (getValue (screenNode, id::cursor)) };
    const cell cursorCol { cursorState.col };
    const cell cursorRow { cursorState.row };

    const int scrollbackLines { getValue (AppState::getContext()->get(), app::id::scrollbackLines) };

    // activeIndex is message-thread-only — safe to read here.
    const jam::Buffer<jam::Row>& activeBuffer { buffers[activeIndex] };

    if (viewportSize.isValid() and viewportRows.value > 0 and scrollbackLines > 0 and activeBuffer.getNumRows() > 0)
    {
        const cell contentRows { wh.historyRows + viewportRows.value };
        const cell totalRows { juce::jmin (contentRows.value, scrollbackLines) };
        const cell historyRows { totalRows.value - viewportRows.value };

        const int ringSize { activeBuffer.getNumRows() };
        const int liveStartRow { (ringSize - historyRows.value) % ringSize };
        const jam::Block<jam::Row> block (
            activeBuffer.getChannelPointer (activeScreen),
            wh.position,
            liveStartRow,
            totalRows.value,
            activeBuffer.getNumCols(),
            activeBuffer.getRowStrideBytes(),
            ringSize);
        setText (block);

        const auto scrollPos { jam::Cell::Point (0_cell, cell (historyRows.value - scrollOffset.value)) };
        setViewportPosition (0, scrollPos.toLogical<int> (font.bounds).y);
        setCaretPosition (cursorCol, cell (historyRows.value + cursorRow.value));
    }
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace terminal
