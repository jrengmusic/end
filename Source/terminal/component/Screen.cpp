/**
 * @file Screen.cpp
 * @brief Row buffer renderer — full viewport repaint from State and Buffer.
 *
 * Screen registers as a ValueTree::Listener on State's SESSION root and grafts
 * its TextEditor node into that tree on construction.
 * On every parameter flush, valueTreePropertyChanged:
 *   1. Reads activeScreen, scrollOffset, and numRows from State.
 *   2. Constructs a Block<Row> view from the live buffer.
 *   3. Calls setText (block) — TextEditor wraps at current display width.
 *
 * Screen holds no scroll state. scrollOffset is read from State each flush.
 *
 * @see Screen.h
 * @see terminal::State         — SSOT for scrollOffset, activeScreen, visibleRows, numRows
 * @see jam::Buffer<jam::Row>   — row storage; Screen constructs Block view
 */

#include "Screen.h"

namespace terminal
{
/*____________________________________________________________________________*/

Screen::Screen (State& stateMachine, jam::Buffer<jam::Row>& bufferToUse) noexcept
    : jam::TextEditor ({})
    , terminalState (stateMachine.get())
    , buffer (bufferToUse)
{
    onResized = onCellChanged (stateMachine);

    setWantsKeyboardFocus (false);

    // Screen nodes (NORMAL, ALTERNATE) are owned and grafted by Display before
    // this constructor runs — they are already present in the tree here.
    terminalState.addListener (this);
    terminalState.appendChild (state, nullptr);
}

Screen::~Screen() { terminalState.removeListener (this); }

// ============================================================================
// juce::ValueTree::Listener
// ============================================================================

void Screen::valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&)
{
    const int activeScreen { getValue (terminalState, id::activeScreen) };
    const juce::Identifier screenId { Map::Screen::getContext()->get (activeScreen) };
    auto screenNode { terminalState.getChildWithName (screenId) };

    const auto viewportSize { jam::Bounds::unpack (getValue (terminalState, id::viewport)) };
    const cell viewportRows { viewportSize.height };

    const cell scrollOffset { getValue (screenNode, id::scrollOffset) };
    const cell numRows { getValue (screenNode, id::numRows) };
    const cell cursorCol { getValue (screenNode, id::cursorCol) };
    const cell cursorRow { getValue (screenNode, id::cursorRow) };

    const int scrollbackLines { getValue (AppState::getContext()->get(), app::id::scrollbackLines) };

    if (viewportSize.isValid() and viewportRows.value > 0 and scrollbackLines > 0 and buffer.getNumRows() > 0)
    {
        const cell contentRows { numRows.value + viewportRows.value };
        const cell totalRows { juce::jmin (contentRows.value, scrollbackLines) };
        const cell historyRows { totalRows.value - viewportRows.value };

        const int ringSize { buffer.getNumRows() };
        const int liveStartRow { (ringSize - historyRows.value) % ringSize };
        const jam::Block<jam::Row> block (buffer, activeScreen, liveStartRow, totalRows.value);
        setText (block);

        const auto scrollPos { jam::Cell::Point (0_cell, cell (historyRows.value - scrollOffset.value)) };
        setViewportPosition (0, scrollPos.toLogical<int> (font.bounds).y);
        setCaretPosition (cursorCol, cell (historyRows.value + cursorRow.value));
    }
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace terminal
