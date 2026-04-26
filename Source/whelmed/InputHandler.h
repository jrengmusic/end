/**
 * @file InputHandler.h
 * @brief Keyboard input dispatcher for a Whelmed document pane.
 *
 * Handles navigation (scroll), modal dispatch (selection mode),
 * and falls through to the global Action system for unhandled keys.
 *
 * @see Whelmed::Component
 * @see Action::Registry
 */

#pragma once

#include <JuceHeader.h>
#include "../ModalType.h"
#include "../SelectionType.h"
#include "../AppState.h"
#include "../lua/Engine.h"

namespace Whelmed
{ /*____________________________________________________________________________*/

class Screen;

/**
 * @class InputHandler
 * @brief Keyboard input dispatcher for a Whelmed document pane.
 *
 * @par Thread context
 * All public methods are **MESSAGE THREAD** only.
 */
class InputHandler
{
public:
    /**
     * @brief Constructs the handler with references to the viewport and state tree.
     *
     * @param vp     Viewport for scroll operations.
     * @param scr    Screen component (for dimensions and block count).
     * @param st     Whelmed state ValueTree (DOCUMENT subtree).
     * @note MESSAGE THREAD.
     */
    InputHandler (juce::Viewport& vp, Screen& scr, juce::ValueTree st) noexcept
        : viewport (vp)
        , screen (scr)
        , state (st)
    {
    }

    /**
     * @brief Copies selection key bindings from the scripting engine.
     * @param keys  Parsed selection keys from lua::Engine.
     * @note MESSAGE THREAD.
     */
    void buildKeyMap (const lua::Engine::SelectionKeys& keys) noexcept;

    /**
     * @brief Top-level key handler.
     *
     * Dispatches:
     * 1. Modal intercept (selection mode) when a modal is active.
     * 2. Navigation keys (j/k/gg/G scroll).
     * 3. Action system fallthrough.
     *
     * @param key  The key press to handle.
     * @return true if the key was consumed.
     * @note MESSAGE THREAD.
     */
    bool handleKey (const juce::KeyPress& key) noexcept;

    /**
     * @brief Clears the pending-g flag.
     * @note MESSAGE THREAD.
     */
    void reset() noexcept;

private:
    bool handleCursorMovement (const juce::KeyPress& key) noexcept;
    bool handleSelectionToggle (const juce::KeyPress& key) noexcept;
    bool handleNavigation (const juce::KeyPress& key) noexcept;
    void toggleSelectionType (SelectionType target) noexcept;
    void copyAndClearSelection() noexcept;

    juce::Viewport& viewport;
    Screen& screen;
    juce::ValueTree state;

    /** Pending-g flag for the two-key gg sequence. */
    bool pendingG { false };

    /** Pending prefix character for two-key navigation sequences. */
    juce::juce_wchar pendingPrefix { 0 };

    /** Sticky column x-position for j/k vertical movement. */
    float preferredX { 0.0f };

    struct SelectionKeys
    {
        juce::KeyPress up;
        juce::KeyPress down;
        juce::KeyPress left;
        juce::KeyPress right;
        juce::KeyPress visual;
        juce::KeyPress visualLine;
        juce::KeyPress visualBlock;
        juce::KeyPress copy;
        juce::KeyPress globalCopy;
        juce::KeyPress top;
        juce::KeyPress bottom;
        juce::KeyPress lineStart;
        juce::KeyPress lineEnd;
        juce::KeyPress exit;
    };

    SelectionKeys selectionKeys;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InputHandler)
};

/**_____________________________END OF NAMESPACE______________________________*/
} // namespace Whelmed
