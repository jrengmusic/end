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
 * Screen holds no scroll state. scrollOffset is read from State each flush.
 *
 * @see Screen.h
 * @see terminal::State  — SSOT for scrollOffset, activeScreen, visibleRows, numRows
 * @see terminal::Grid   — cell storage; Screen reads via getBlock
 */

#include "Screen.h"

namespace terminal
{
/*____________________________________________________________________________*/

Screen::Screen (terminal::State& stateToUse, terminal::Grid& gridToUse) noexcept
    : jam::TextEditor ({})
    , state (stateToUse)
    , grid (gridToUse)
    , stateTree (stateToUse.get())
{
    setWantsKeyboardFocus (false);
    stateTree.addListener (this);
    stateToUse.get().appendChild (getNode(), nullptr);
    setViewportMode (ViewportMode::proportional);
    rebindScroll (stateToUse.getActiveScreen());
}

Screen::~Screen()
{
    stateTree.removeListener (this);

    auto parentTree { getNode().getParent() };

    if (parentTree.isValid())
        parentTree.removeChild (getNode(), nullptr);
}

// ============================================================================
// juce::ValueTree::Listener
// ============================================================================

void Screen::valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&)
{
    const int activeScreenIndex { state.getActiveScreen() };
    const int viewportRows      { state.getVisibleRows().value };
    const int numCols           { state.getCols().value };
    const int scrollOffset      { state.getScrollOffset (activeScreenIndex) };
    const int numRows           { state.getNumRows (activeScreenIndex) };

    if (numCols > 0 and viewportRows > 0)
    {
        const auto block { grid.getBlock (activeScreenIndex, scrollOffset, viewportRows) };
        setText (block);
        setScrollRange (numRows);
        setCaretPosition (state.getCursorCol(), state.getCursorRow());
    }

    if (activeScreenIndex != boundScreenIndex)
        rebindScroll (activeScreenIndex);
}

void Screen::rebindScroll (int screenIndex) noexcept
{
    const juce::Identifier screenId { ScreenMap::getContext()->get (screenIndex) };
    auto screenNode { state.get().getChildWithName (screenId) };
    auto scrollValue { jam::ValueTree::getValueFromChildWithID (screenNode, id::scrollOffset) };
    bindScroll (scrollValue);
    boundScreenIndex = screenIndex;
}

/**______________________________END OF NAMESPACE______________________________*/
} // namespace terminal
