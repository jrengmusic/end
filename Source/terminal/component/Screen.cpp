/**
 * @file Screen.cpp
 * @brief Cell buffer renderer — full viewport repaint from State and Buffer.
 *
 * Screen registers as a ValueTree::Listener on State's SESSION root and grafts
 * its TextEditor node into that tree on construction.
 * On every parameter flush, valueTreePropertyChanged:
 *   1. Reads activeScreen, scrollOffset, and numRows from State.
 *   2. Constructs a Block<Cell> view from Buffer for the visible window.
 *   3. Calls setText (block) — single call replaces the old per-row loop.
 *
 * Screen holds no scroll state. scrollOffset is read from State each flush.
 *
 * @see Screen.h
 * @see terminal::State         — SSOT for scrollOffset, activeScreen, visibleRows, numRows
 * @see jam::Buffer<jam::Cell>  — cell storage; Screen constructs Block view
 */

#include "Screen.h"

namespace terminal
{
/*____________________________________________________________________________*/

Screen::Screen (State& stateMachine, jam::Buffer<jam::Cell>& bufferToUse) noexcept
    : jam::TextEditor ({})
    , terminal (stateMachine)
    , buffer (bufferToUse)
{
    setWantsKeyboardFocus (false);

    // Screen nodes (NORMAL, ALTERNATE) are owned and grafted by Display before
    // this constructor runs — they are already present in the tree here.
    terminal.get().addListener (this);
    terminal.get().appendChild (state, nullptr);
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
    const int activeScreen { terminal.getActiveScreen() };
    const int viewportRows { terminal.getVisibleRows().value };
    const int numCols { terminal.getCols().value };

    const juce::Identifier screenId { Map::Screen::getContext()->get (activeScreen) };
    auto screenNode { terminal.get().getChildWithName (screenId) };
    const int scrollOffset { static_cast<int> (
        jam::ValueTree::getValueFromChildWithID (screenNode, id::scrollOffset).getValue()) };
    const int numRows { static_cast<int> (
        jam::ValueTree::getValueFromChildWithID (screenNode, id::numRows).getValue()) };
    const cell cursorCol { static_cast<int> (
        jam::ValueTree::getValueFromChildWithID (screenNode, id::cursorCol).getValue()) };
    const cell cursorRow { static_cast<int> (
        jam::ValueTree::getValueFromChildWithID (screenNode, id::cursorRow).getValue()) };

    const int scrollbackLines { static_cast<int> (
        jam::ValueTree::getValueFromChildWithID (AppState::getContext()->get(), app::id::scrollbackLines).getValue()) };

    if (numCols > 0 and viewportRows > 0 and buffer.getNumRows() > 0 and scrollbackLines > 0)
    {
        const int totalRows { juce::jmin (numRows + viewportRows, scrollbackLines) };
        const int startRow { buffer.getNumRows() - (totalRows - viewportRows) };
        const jam::Block<jam::Cell> block (buffer, activeScreen, startRow, totalRows);
        setText (block);

        const int historyRows { totalRows - viewportRows };
        const int viewportY { (historyRows - scrollOffset) * font.bounds.height };
        setViewportPosition (0, viewportY);
        setCaretPosition (cursorCol, cell (historyRows + cursorRow.value));
    }
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace terminal
