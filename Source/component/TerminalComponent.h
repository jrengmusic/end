/**
 * @file TerminalComponent.h
 * @brief UI host component that wires the terminal backend to JUCE input/render.
 *
 * Terminal::Component is the central integration point between the terminal
 * emulation backend (Session + Grid) and the JUCE component tree.  It owns:
 *
 * - **Screen** — GPU-accelerated renderer; attached to this component via
 *   `Screen::attachTo()` once the component becomes visible.
 * - **Session** — pty session, VT parser, and grid state machine.
 * - **CursorComponent** — cursor overlay driven by ValueTree state.
 * - **MessageOverlay** — transient overlay for grid-size and error messages.
 * - **ScreenSelection** — current text selection (null when nothing selected).
 *
 * ### Render loop
 * A `juce::VBlankAttachment` calls `onVBlank()` every display refresh.
 * `onVBlank()` checks `consumeSnapshotDirty()` and, if the grid has changed,
 * calls `Screen::render()` under a `ScopedTryLock` on the resize lock.
 *
 * ### Keyboard handling
 * Application commands (copy, paste, quit, reload, zoom) are handled by
 * MainComponent via ApplicationCommandManager.  `keyPressed()` only handles:
 * | Shortcut        | Action                                      |
 * |-----------------|---------------------------------------------|
 * | Shift+PgUp/Dn   | Scroll scrollback (isScrollNav guard)       |
 * | Shift+Home/End  | Jump to top / bottom of scrollback          |
 * | Any other key   | Forward to Session (TTY passthrough)        |
 *
 * ### Mouse handling
 * Mouse events are forwarded as SGR 1006 sequences when the application has
 * enabled mouse tracking (`mouseTracking`, `mouseMotionTracking`, or
 * `mouseAllTracking` mode).  In normal mode, mouse events drive text selection.
 *
 * @note All methods are called on the **MESSAGE THREAD** unless noted.
 *
 * @see Terminal::Screen
 * @see Terminal::Session
 * @see CursorComponent
 * @see MessageOverlay
 * @see Config
 */

#pragma once
#include <JuceHeader.h>
#include "../terminal/logic/Session.h"
#include "../terminal/rendering/Screen.h"
#include "CursorComponent.h"
#include "MessageOverlay.h"
#include "config/Config.h"

namespace Terminal
{ /*____________________________________________________________________________*/
/**
 * @class Terminal::Component
 * @brief JUCE component that hosts the terminal renderer and handles all input.
 *
 * Inherits `juce::Component` for rendering and layout, `juce::KeyListener` for
 * keyboard input, and `juce::ValueTree::Listener` (private) to react to cursor
 * position and active-screen changes published by the Session state machine.
 *
 * @par Zoom
 * `applyZoom()` scales the base font size by the zoom multiplier and calls
 * `Screen::setFontSize()`.  The `zoomInProgress` flag suppresses the
 * `MessageOverlay` grid-size display during zoom operations.
 *
 * @par Selection
 * `screenSelection` is a `unique_ptr<ScreenSelection>` that is non-null while
 * the user has an active selection.  It is cleared on any non-scroll keypress
 * or when the mouse button is released with a zero-area selection.
 *
 * @par Thread context
 * **MESSAGE THREAD** — all public methods.  `onVBlank()` is called from the
 * VBlank callback which runs on the message thread.
 *
 * @see Terminal::Screen
 * @see Terminal::Session
 * @see CursorComponent
 * @see MessageOverlay
 */
class Component
    : public jreng::GLComponent
    , public juce::KeyListener
    , private juce::ValueTree::Listener
{
public:
    /** @brief Constructs the component, wires Session callbacks, starts VBlank. */
    Component();

    /** @brief Tears down listeners, detaches Screen, resets all children. */
    ~Component() override;

    /**
     * @brief Lays out the screen viewport, notifies Session of new grid size,
     *        and updates the MessageOverlay bounds.
     * @note MESSAGE THREAD — called by JUCE on every resize event.
     */
    void resized() override;

    /**
     * @brief Attaches Screen to this component on first visibility.
     *
     * `Screen::attachTo()` is deferred until the component is showing so that
     * the OpenGL context has a valid native peer to attach to.
     *
     * @note MESSAGE THREAD.
     */
    void visibilityChanged() override;

    /**
     * @brief Dispatches keyboard shortcuts and forwards remaining keys to Session.
     *
     * The `isScrollNav` guard ensures that Shift+PgUp/Dn/Home/End scroll the
     * scrollback buffer instead of being forwarded to the shell.
     *
     * @param key                    The key event.
     * @param originatingComponent   The component that received the event (unused).
     * @return Always @c true — all key events are consumed.
     * @note MESSAGE THREAD.
     */
    bool keyPressed (const juce::KeyPress& key, juce::Component* originatingComponent) override;

    /**
     * @brief Notifies the shell of focus gain and triggers a repaint.
     * @param cause  The focus change cause (unused).
     * @note MESSAGE THREAD.
     */
    void focusGained (FocusChangeType cause) override;

    /**
     * @brief Notifies the shell of focus loss and triggers a repaint.
     * @param cause  The focus change cause (unused).
     * @note MESSAGE THREAD.
     */
    void focusLost (FocusChangeType cause) override;

    /**
     * @brief Scrolls the scrollback buffer or forwards a mouse-wheel SGR event.
     *
     * When mouse tracking is active, encodes the scroll direction as button 64
     * (up) or 65 (down) and writes an SGR 1006 sequence to the pty.  Otherwise
     * adjusts the scroll offset by `Config::Key::scrollbackStep` lines.
     *
     * @param event  The mouse event (position used for SGR cell coordinates).
     * @param wheel  Wheel details; `deltaY > 0` means scroll up.
     * @note MESSAGE THREAD.
     */
    void mouseWheelMove (const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override;

    /**
     * @brief Begins a text selection or forwards a mouse-button SGR event.
     *
     * Triple-click selects the entire row.  Single click starts a new selection
     * anchored at the clicked cell.  In mouse-tracking mode the event is
     * forwarded as an SGR button-press sequence instead.
     *
     * @param event  The mouse event.
     * @note MESSAGE THREAD.
     */
    void mouseDown (const juce::MouseEvent& event) override;

    /**
     * @brief Selects the word under the cursor on double-click.
     *
     * Scans left and right from the clicked cell for non-space codepoints
     * (> 0x20) to determine word boundaries.  Only active on the primary screen
     * and when mouse tracking is disabled.
     *
     * @param event  The mouse event.
     * @note MESSAGE THREAD.
     */
    void mouseDoubleClick (const juce::MouseEvent& event) override;

    /**
     * @brief Extends the selection or forwards a mouse-motion SGR event.
     *
     * In motion-tracking mode writes an SGR button-32 (drag) sequence.
     * Otherwise extends `screenSelection->end` to the current cell.
     *
     * @param event  The mouse event.
     * @note MESSAGE THREAD.
     */
    void mouseDrag (const juce::MouseEvent& event) override;

    /**
     * @brief Finalises the selection or forwards a mouse-button-release SGR event.
     *
     * Clears a zero-area selection (anchor == end) on mouse-up.  In
     * mouse-tracking mode writes an SGR button-release sequence.
     *
     * @param event  The mouse event.
     * @note MESSAGE THREAD.
     */
    void mouseUp (const juce::MouseEvent& event) override;

    /**
     * @brief Returns the current number of visible grid rows.
     * @return Row count as reported by Screen.
     */
    int getGridRows() const noexcept;

    /**
     * @brief Returns the current number of visible grid columns.
     * @return Column count as reported by Screen.
     */
    int getGridCols() const noexcept;

    void glContextCreated() noexcept override;
    void glContextClosing() noexcept override;
    void renderGL() noexcept override;

    std::function<void()> onRepaintNeeded;

    void copySelection();
    void pasteClipboard();
    void reloadConfig();
    void increaseZoom();
    void decreaseZoom();
    void resetZoom();

private:
    //==============================================================================
    /**
     * @brief Reacts to cursor position and active-screen changes in the ValueTree.
     *
     * Listens for `Terminal::ID::value` property changes on `Terminal::ID::PARAM`
     * nodes.  Handles:
     * - `activeScreen` — rebinds the cursor to the new screen's state tree.
     * - `cursorRow` / `cursorCol` — repositions the CursorComponent.
     *
     * @param tree      The ValueTree node that changed.
     * @param property  The property identifier that changed.
     * @note MESSAGE THREAD — called by JUCE ValueTree notification.
     */
    void valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property) override;

    /**
     * @brief VBlank callback: renders the grid if dirty, updates cursor visibility.
     *
     * Called every display refresh by `juce::VBlankAttachment`.  Steps:
     * 1. Ensures this component is the focus container.
     * 2. If the OpenGL context was just recreated, marks all cells dirty.
     * 3. If the snapshot is dirty and the resize lock is available, renders.
     * 4. Updates cursor visibility based on scroll offset and cursor mode.
     *
     * @note MESSAGE THREAD — VBlankAttachment callbacks run on the message thread.
     */
    void onVBlank();

    /**
     * @brief Repositions CursorComponent to the current cursor cell bounds.
     *
     * Reads `cursorRow` and `cursorCol` from the cursor ValueTree, queries
     * `Screen::getCellBounds()`, and calls `cursor->setBounds()`.  Also adjusts
     * width for wide (double-width) cursor glyphs via `getCellWidth()`.
     *
     * @note MESSAGE THREAD.
     */
    void updateCursorBounds() noexcept;

    /**
     * @brief Applies the current config to the renderer after a reload.
     *
     * Updates ligatures, embolden, and theme on Screen, then marks all cells
     * dirty so the next VBlank re-renders with the new settings.
     *
     * @note MESSAGE THREAD — called from perform() on reload command.
     * @see Config::reload
     */
    void applyConfig() noexcept;

    void handleScrollNavigation (int keyCode);
    void clearSelectionAndScroll();

    /**
     * @brief Scales the font size by @p zoom and re-lays out the component.
     *
     * Sets `zoomInProgress = true` before calling `resized()` to suppress the
     * grid-size MessageOverlay during zoom, then clears the flag.
     *
     * @param zoom  The zoom multiplier to apply (already clamped by the caller).
     * @note MESSAGE THREAD — called from keyPressed() on Cmd+= / Cmd+- / Cmd+0.
     * @see Screen::setFontSize
     */
    void applyZoom (float zoom) noexcept;

    /**
     * @brief Returns whether any mouse-tracking mode is currently active.
     *
     * Checks `mouseTracking`, `mouseMotionTracking`, and `mouseAllTracking`
     * modes in the Session state tree.
     *
     * @return @c true if any mouse-tracking mode is enabled.
     */
    bool isMouseTracking() const noexcept;

    //==============================================================================
    /** @brief GPU-accelerated terminal renderer; attached to this component. */
    Screen screen;

    /** @brief pty session, VT parser, and grid state machine. */
    Session session;

    /** @brief Cursor overlay component; positioned by updateCursorBounds(). */
    std::unique_ptr<CursorComponent> cursor;

    /** @brief Transient overlay for grid-size and error/reload messages. */
    std::unique_ptr<MessageOverlay> messageOverlay;

    /** @brief Current text selection; null when nothing is selected. */
    std::unique_ptr<ScreenSelection> screenSelection;

    /**
     * @brief Snapshot of the Session state ValueTree for listener registration.
     *
     * Held as a member so that `addListener` / `removeListener` operate on the
     * same tree object across the component lifetime.
     */
    juce::ValueTree stateTree;

    /** @brief VBlank attachment that drives the render loop at display refresh rate. */
    juce::VBlankAttachment vblank;

    //==============================================================================
    /** @brief Vertical padding (pixels) between the component edge and the grid. */
    static constexpr int verticalInset { 10 };

    /** @brief Horizontal padding (pixels) between the component edge and the grid. */
    static constexpr int horizontalInset { 10 };

    /**
     * @brief Height of the native title bar in pixels; 0 when buttons are hidden.
     *
     * Read from Config at construction.  When `window.buttons = true` the
     * traffic-light buttons occupy 24 px at the top of the component.
     */
    const int titleBarHeight { Config::getContext()->getBool (Config::Key::windowButtons) ? 24 : 0 };

    /**
     * @brief Flag set during applyZoom() to suppress the grid-size MessageOverlay.
     *
     * `resized()` calls `messageOverlay->show()` after every layout pass.
     * Setting this flag before calling `resized()` prevents that display during
     * zoom operations where the size change is intentional and transient.
     */
    bool zoomInProgress { false };

    /** @brief Returns the config font size corrected for display DPI.
     *  On Windows at 150% scale, divides by 1.5 so 14pt looks the same as Mac. */
    static float dpiCorrectedFontSize() noexcept
    {
        const float raw { Config::getContext()->getFloat (Config::Key::fontSize) };
#if JUCE_WINDOWS
        const auto* display { juce::Desktop::getInstance().getDisplays().getPrimaryDisplay() };
        const float scale { display != nullptr ? static_cast<float> (display->scale) : 1.0f };
        return raw / scale;
#else
        return raw;
#endif
    }

    /** @brief Suppresses the grid-size overlay on the initial layout pass. */
    bool isInitLayout { false };

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Component)
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Terminal
