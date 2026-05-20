/**
 * @file Screen.cpp
 * @brief Cell grid renderer — full viewport repaint from State and Grid.
 *
 * Screen registers as a ValueTree::Listener on State's SESSION root and grafts
 * its TextEditor node into that tree on construction.
 * On every parameter flush, valueTreePropertyChanged:
 *   1. Reads activeScreen, scrollOffset, and numRows from State.
 *   2. Calls grid.getBlock() to obtain a Block<Row> for the visible window.
 *   3. Calls setText (block) — single call replaces the old per-row loop.
 *   4. Calls setScrollRange to size the scrollbar thumb.
 *
 * Screen holds no scroll terminal. scrollOffset is read from State each flush.
 *
 * @see Screen.h
 * @see terminal::State  — SSOT for scrollOffset, activeScreen, visibleRows, numRows
 * @see terminal::Grid   — cell storage; Screen reads via getBlock
 */

#include "Screen.h"

namespace terminal
{
/*____________________________________________________________________________*/

Screen::Screen (State& stateMachine, Grid& gridToUse) noexcept
    : jam::TextEditor ({})
    , terminal (stateMachine)
    , grid (gridToUse)
{
    setWantsKeyboardFocus (false);

    // Screen nodes (NORMAL, ALTERNATE) are owned and grafted by Display before
    // this constructor runs — they are already present in the tree here.
    terminal.get().addListener (this);
    terminal.get().appendChild (state, nullptr);
    setViewportMode (ViewportMode::proportional);

    const int activeScreen { terminal.getActiveScreen() };
    const juce::Identifier screenId { Map::Screen::getContext()->get (activeScreen) };
    auto screenNode { terminal.get().getChildWithName (screenId) };
    auto scrollValue { jam::ValueTree::getValueFromChildWithID (screenNode, id::scrollOffset) };
    attach (scrollValue);
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
    const int scrollOffset { static_cast<int> (jam::ValueTree::getValueFromChildWithID (screenNode, id::scrollOffset).getValue()) };
    const int numRows { static_cast<int> (jam::ValueTree::getValueFromChildWithID (screenNode, id::numRows).getValue()) };
    const cell cursorCol { static_cast<int> (jam::ValueTree::getValueFromChildWithID (screenNode, id::cursorCol).getValue()) };
    const cell cursorRow { static_cast<int> (jam::ValueTree::getValueFromChildWithID (screenNode, id::cursorRow).getValue()) };

    if (numCols > 0 and viewportRows > 0)
    {
        const auto block { grid.getBlock (activeScreen, scrollOffset, viewportRows) };
        setText (block);
        setScrollRange (numRows);
        setCaretPosition (cursorCol, cursorRow);
    }

    auto scrollValue { jam::ValueTree::getValueFromChildWithID (screenNode, id::scrollOffset) };
    attach (scrollValue);
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace terminal
