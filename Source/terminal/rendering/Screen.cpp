/**
 * @file Screen.cpp
 * @brief Cell grid renderer — full viewport repaint from State and Grid.
 *
 * Screen registers as a ValueTree::Listener on State's SESSION root.
 * On every parameter flush, valueTreePropertyChanged:
 *   1. Syncs activeScreen and caret position from State.
 *   2. Live mode (scrollOffset == 0): reads all viewport rows from Grid via
 *      getWritePointer, calls setText.
 *   3. Scroll mode (scrollOffset > 0): reads history rows from Grid via getRow,
 *      renders the frozen historical window.
 *   4. Repaints.
 *
 * Screen holds no scroll state. scrollOffset is read from State each flush.
 *
 * @see Screen.h
 * @see Terminal::State  — SSOT for scrollOffset, activeScreen, visibleRows, numRows
 * @see Terminal::Grid   — cell storage; Screen reads via getWritePointer / getRow
 */

#include "Screen.h"
#include "../data/State.h"
#include "../logic/Grid.h"

namespace Terminal
{
/*____________________________________________________________________________*/

Screen::Screen (Terminal::State& stateToUse, Terminal::Grid& gridToUse) noexcept
    : jam::TextEditor ({}, 2)
    , state (stateToUse)
    , grid (gridToUse)
    , stateTree (stateToUse.get())
{
    setWantsKeyboardFocus (false);
    stateTree.addListener (this);
}

Screen::~Screen()
{
    stateTree.removeListener (this);
}

// ============================================================================
// juce::Component
// ============================================================================

void Screen::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& wheel)
{
    const int activeScreenIndex { state.getActiveScreen() };
    const int numRows           { state.getNumRows (activeScreenIndex) };
    const int currentOffset     { state.getScrollOffset (activeScreenIndex) };
    static constexpr int scrollStep { 3 };
    const int scrollLines       { wheel.deltaY > 0.0f ? scrollStep : -scrollStep };
    const int newOffset         { juce::jlimit (0, numRows, currentOffset + scrollLines) };

    if (newOffset != currentOffset)
        state.setScrollOffset (activeScreenIndex, newOffset);
}

// ============================================================================
// juce::ValueTree::Listener
// ============================================================================

void Screen::valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&)
{
    const int activeScreenIndex { state.getActiveScreen() };
    const auto cellArea         { getCellArea() };
    const int viewportRows      { cellArea.height };
    const int numCols           { cellArea.width };
    const int scrollOffset      { state.getScrollOffset (activeScreenIndex) };
    const int numRows           { state.getNumRows (activeScreenIndex) };

    setActiveScreen (activeScreenIndex);
    setCaretPosition (state.getCursorCol(), state.getCursorRow());

    if (numCols > 0 and viewportRows > 0)
    {
        if (scrollOffset == 0)
        {
            // Live mode — repaint all viewport rows from Grid viewport section.
            for (int row { 0 }; row < viewportRows; ++row)
            {
                const jam::Row* r { grid.getWritePointer (activeScreenIndex, row) };
                const jam::Cell* ptr { r->cells };
                jam::Block<jam::Cell> block { &ptr, 1, numCols };
                setText (block, { row, row + 1 });
            }
        }
        else
        {
            // Scroll mode — read from history at absolute offset.
            const int startIndex { numRows - scrollOffset };

            for (int row { 0 }; row < viewportRows; ++row)
            {
                const jam::Row* r { grid.getRow (activeScreenIndex, startIndex + row) };
                const jam::Cell* ptr { r->cells };
                jam::Block<jam::Cell> block { &ptr, 1, numCols };
                setText (block, { row, row + 1 });
            }
        }
    }

    repaint();
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Terminal
