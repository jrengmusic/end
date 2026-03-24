/**
 * @file MouseHandler.h
 * @brief Handles all mouse input routing for a terminal session.
 *
 * MouseHandler centralises all mouse event logic previously spread across
 * TerminalComponent: PTY forwarding, selection anchor/drag, link hit-testing,
 * scroll accumulation for smooth trackpad input, and word/line selection.
 *
 * All selection state is written to `State` parameters.  `ScreenSelection`
 * construction happens in `TerminalComponent::onVBlank()` from those State
 * params — MouseHandler never writes to `ScreenSelection` directly.
 *
 * ### Coordinate convention
 * Grid positions received from `Screen::cellAtPoint()` are **visible-row**
 * coordinates (0 = topmost visible row).  MouseHandler converts them to
 * **absolute (scrollback-aware)** coordinates before writing to State, matching
 * the keyboard selection path.
 *
 * @see Terminal::Session
 * @see Terminal::Screen
 * @see LinkManager
 */

#pragma once

#include <JuceHeader.h>

namespace Terminal
{

class Session;
class Screen;
class LinkManager;

/**
 * @class MouseHandler
 * @brief Mouse event dispatcher for a single terminal session.
 *
 * Constructed with references to the session, screen, and link manager.  All
 * references must remain valid for the lifetime of the `MouseHandler`.
 *
 * @par Thread context
 * All public methods are **MESSAGE THREAD** only.
 */
class MouseHandler
{
public:
    /**
     * @brief Scaling factor that converts JUCE normalised trackpad deltas to
     *        line-sized units before accumulation.
     *
     * JUCE normalises smooth-scroll deltas to approximately `1.0 / lines-per-notch`,
     * yielding values around 0.05–0.15 per frame at typical trackpad velocity.
     * After multiplying by the user's `terminalScrollStep` (default 5), the raw
     * product is still sub-line (~0.25–0.75).  This factor bridges the gap so that
     * a natural finger swipe produces a comfortable number of scrolled lines
     * without requiring multiple frames to cross the first whole-line threshold.
     */
    static constexpr float trackpadDeltaScale { 8.0f };

    /**
     * @brief Constructs a MouseHandler.
     * @param session     Terminal session (state, grid, PTY write).
     * @param screen      Terminal renderer (cell coordinate mapping).
     * @param linkManager Link manager (hit-testing, dispatch).
     * @note MESSAGE THREAD.
     */
    MouseHandler (Session& session,
                  Screen& screen,
                  LinkManager& linkManager) noexcept;

    /**
     * @brief Handles a mouse-down event.
     *
     * - **Mouse tracking active**: forwards SGR button-0 press.
     * - **Triple-click**: selects the entire row (visualLine to State).
     * - **Single click on link**: dispatches via LinkManager.
     * - **Single click on non-link**: records drag anchor; clears selection.
     *
     * @param event  The mouse event.
     * @note MESSAGE THREAD.
     */
    void handleDown (const juce::MouseEvent& event);

    /**
     * @brief Handles a mouse double-click event.
     *
     * - **Mouse tracking active**: forwards SGR button-0 press.
     * - **No tracking**: selects the word under the cursor (visual to State).
     *
     * @param event  The mouse event.
     * @note MESSAGE THREAD.
     */
    void handleDoubleClick (const juce::MouseEvent& event);

    /**
     * @brief Handles a mouse drag event.
     *
     * - **Motion tracking active**: forwards SGR button-32 (drag).
     * - **No tracking**: extends the drag selection via State params once
     *   the 2-cell Manhattan distance threshold is crossed.
     *
     * @param event  The mouse event.
     * @note MESSAGE THREAD.
     */
    void handleDrag (const juce::MouseEvent& event);

    /**
     * @brief Handles a mouse-up event.
     *
     * - **Mouse tracking active**: forwards SGR button-0 release.
     * - **No tracking**: resets `State::dragActive`; keeps `screenSelection`
     *   visible so the user can Cmd+C to copy.
     *
     * @param event  The mouse event.
     * @note MESSAGE THREAD.
     */
    void handleUp (const juce::MouseEvent& event);

    /**
     * @brief Handles mouse move (hover) events.
     *
     * Hit-tests the hovered cell against `linkManager`'s current clickable spans.
     * Sets `PointingHandCursor` over a link, `NormalCursor` otherwise.
     * No-op when mouse tracking or a modal is active.
     *
     * @param event     The mouse event.
     * @param component The JUCE component to set the cursor on.
     * @note MESSAGE THREAD.
     */
    void handleMove (const juce::MouseEvent& event, juce::Component& component);

    /**
     * @brief Handles mouse wheel scroll events.
     *
     * - **Alternate screen + mouse tracking**: forwards SGR scroll button.
     * - **Discrete wheel**: adjusts scrollback offset by a fixed step.
     * - **Smooth trackpad**: accumulates fractional deltas and scrolls whole lines only.
     *
     * @param event         The mouse event.
     * @param wheel         Wheel details; `deltaY > 0` = scroll up.
     * @param setScrollFn   Callable for clamped scroll offset writes: `void(int)`.
     * @note MESSAGE THREAD.
     */
    void handleWheel (const juce::MouseEvent& event,
                      const juce::MouseWheelDetails& wheel,
                      std::function<void (int)> setScrollFn);

private:
    /**
     * @brief Returns `true` when any mouse-tracking mode is active.
     *
     * Reads `mouseTracking`, `mouseMotionTracking`, and `mouseAllTracking`
     * from State.
     *
     * @return `true` if mouse events should be forwarded to the PTY.
     */
    bool shouldForwardToPty() const noexcept;

    /**
     * @brief Converts a visible row to an absolute (scrollback-aware) row.
     *
     * @param visibleRow  0-based visible row (0 = topmost visible row).
     * @return Absolute row index in the scrollback+screen coordinate space.
     */
    int toAbsoluteRow (int visibleRow) const noexcept;

    /** @brief Terminal session — provides state, grid, and PTY write access. */
    Session& session;

    /** @brief Terminal renderer — provides cell coordinate mapping. */
    Screen& screen;

    /** @brief Link manager — click hit-testing and dispatch. */
    LinkManager& linkManager;

    /**
     * @brief Fractional scroll accumulator for smooth (trackpad) input.
     *
     * Collects sub-line deltas and only scrolls when a whole-line threshold is
     * crossed, preventing overscroll on fast finger swipes.
     */
    float scrollAccumulator { 0.0f };
};

} // namespace Terminal
