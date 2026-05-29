/**
 * @file Screen.cpp
 * @brief Terminal viewport renderer — pure renderer, no content ownership.
 *
 * Display calls setText(TextLineArray) on every content update. Screen renders
 * via Arrangement shaped during setText. Screen never writes to TextLineArray.
 *
 * @see Screen.h
 * @see jam::TextLineArray      — SSOT content storage (owned by Processor)
 */

#include "Screen.h"

namespace terminal
{
/*____________________________________________________________________________*/

/**
 * @brief Constructs Screen and registers as a ValueTree listener.
 *
 * Screen does not allocate cell storage.  Display calls setText(TextLineArray)
 * on every content update.  Processor owns TextLineArray.
 *
 * @param stateMachine  Terminal parameter store — owned by Session.
 * @note MESSAGE THREAD.
 */
Screen::Screen (State& stateMachine, const jam::Font& font) noexcept
    : jam::TextEditor (font)
    , terminalState (stateMachine.get())
{
    terminalState.addListener (this);
    terminalState.appendChild (state, nullptr);
}

Screen::~Screen() { terminalState.removeListener (this); }

// ============================================================================
// juce::ValueTree::Listener
// ============================================================================

void Screen::valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&)
{
    const int activeScreen { static_cast<int> (
        jam::ValueTree::getValueFromChildWithID (terminalState, id::activeScreen).getValue()) };
    const juce::Identifier screenId { Map::Screen::getContext()->get (activeScreen) };
    const auto screenNode { terminalState.getChildWithName (screenId) };

    const CursorState cursorState { CursorState::unpack (
        static_cast<int> (jam::ValueTree::getValueFromChildWithID (screenNode, id::cursor).getValue())) };

    const int64_t packed { static_cast<int64_t> (state.getProperty (properties.at (viewportId), 0)) };
    const int visibleTerminalRows { jam::Cell::Rectangle::unpack (packed).getHeight().value };

    if (visibleTerminalRows > 0 and projectedRows > 0)
    {
        const int historyRows { std::max (0, projectedRows - visibleTerminalRows) };
        const int mappedRow { jam::Value::map (cursorState.row, 0, visibleTerminalRows - 1,
                                               historyRows, historyRows + visibleTerminalRows - 1) };

        setCaretPosition (cell (cursorState.col), cell (mappedRow));
    }
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace terminal
