/**
 * @file TerminalComponent.h
 * @brief UI host component that wires the terminal backend to JUCE input/render.
 *
 * Terminal::Component is the central integration point between the terminal
 * emulation backend (Session + Grid) and the JUCE component tree.  It owns:
 *
 * - **Screen** — GPU-accelerated renderer; draws via the shared
 *   `jreng::GLRenderer` context owned by MainComponent.
 * - **Session** — pty session, VT parser, and grid state machine.
 * - **CursorComponent** — cursor overlay driven by ValueTree state.
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
#include "config/Config.h"

namespace Terminal
{ /*____________________________________________________________________________*/
/**
 * @class Terminal::Component
 * @brief JUCE component that hosts the terminal renderer and handles all input.
 *
 * Inherits `jreng::GLComponent` for GL-backed rendering and layout, `juce::KeyListener` for
 * keyboard input, and `juce::ValueTree::Listener` (private) to react to cursor
 * position and active-screen changes published by the Session state machine.
 *
 * @par Zoom
 * `applyZoom()` scales the base font size by the zoom multiplier and calls
 * `Screen::setFontSize()`.
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
 */
class Component
    : public jreng::GLComponent
    , public juce::KeyListener
    , private juce::ValueTree::Listener
{
public:
    /** @brief Constructs the component, wires Session callbacks, starts VBlank. */
    Component();

    /**
     * @brief Constructs a terminal component starting in the given directory.
     *
     * @param workingDirectory  Absolute path for the shell's initial cwd.
     * @note MESSAGE THREAD.
     */
    explicit Component (const juce::String& workingDirectory);

    /**
     * @brief Constructs a terminal component running a specific command.
     *
     * Overrides the default shell program from Config.  Used by popup
     * terminals to run TUI apps, scripts, etc.
     *
     * @param program           Shell command or executable path.
     * @param args              Arguments passed to the command.
     * @param workingDirectory  Initial cwd. Empty = inherit.
     * @note MESSAGE THREAD.
     */
    Component (const juce::String& program,
               const juce::String& args,
               const juce::String& workingDirectory);

    /**
     * @brief Creates a terminal, adds it to the parent and owner.
     *
     * @param parent            The component to add the terminal to (addAndMakeVisible).
     * @param bounds            The initial bounds for the terminal.
     * @param owner             Ownership container for the terminal's lifetime.
     * @param workingDirectory  Initial cwd for the shell. Empty = inherit parent cwd.
     * @return Raw pointer to the created terminal.
     * @note MESSAGE THREAD.
     */
    static Component* create (juce::Component& parent,
                              juce::Rectangle<int> bounds,
                              jreng::Owner<Component>& owner,
                              const juce::String& workingDirectory = {});

    /** @brief Tears down listeners, detaches Screen, resets all children. */
    ~Component() override;

    /**
     * @brief Lays out the screen viewport, notifies Session of new grid size.
     * @note MESSAGE THREAD — called by JUCE on every resize event.
     */
    void resized() override;

    /**
     * @brief Marks the grid dirty on visibility gain so the next frame redraws.
     *
     * When the component becomes visible (e.g. tab switch), marks all grid
     * rows and the snapshot dirty so the shared GL context redraws this
     * terminal on the next frame.
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

    /**
     * @brief Returns the terminal session's root ValueTree.
     *
     * Convenience accessor for grafting the SESSION subtree into the
     * application ValueTree.
     *
     * @return The SESSION ValueTree owned by this terminal's State.
     * @note MESSAGE THREAD.
     */
    juce::ValueTree getValueTree() noexcept;

    /**
     * @brief Called by GLRenderer when the shared OpenGL context is first created.
     *
     * Forwards to Screen::glContextCreated() to compile shaders and create GPU
     * buffers.  Also calls jreng::BackgroundBlur::enableGLTransparency() for
     * native window transparency.
     *
     * @note GL THREAD.
     * @see Screen::glContextCreated
     */
    void glContextCreated() noexcept override;

    /**
     * @brief Called by GLRenderer when the shared OpenGL context is about to close.
     *
     * Forwards to Screen::glContextClosing() to release GPU resources.
     *
     * @note GL THREAD.
     * @see Screen::glContextClosing
     */
    void glContextClosing() noexcept override;

    /**
     * @brief Called by GLRenderer each frame to render the terminal.
     *
     * Computes the viewport origin offset via getOriginInTopLevel() and forwards
     * to Screen::renderOpenGL().  Performs lazy GL initialisation if isGLContextReady()
     * returns false (for tabs created after the context was already established).
     *
     * @note GL THREAD.
     * @see Screen::renderOpenGL
     */
    void renderGL() noexcept override;

    /**
     * @brief Callback invoked after rendering to trigger a repaint.
     *
     * Set by Terminal::Tabs (which receives it from MainComponent).  Invoked from
     * onVBlank() after rendering to trigger GLRenderer::triggerRepaint() on the
     * shared GL context.
     *
     * @note MESSAGE THREAD.
     * @see GLRenderer::triggerRepaint
     */
    std::function<void()> onRepaintNeeded;

    /**
     * @brief Callback invoked when the shell process exits.
     *
     * If set, this replaces the default behaviour (quit the application).
     * Used by popup terminals to dismiss the popup window on process exit.
     *
     * @note MESSAGE THREAD (via callAsync).
     */
    std::function<void()> onProcessExited;

    /**
     * @brief Returns `true` if a non-degenerate box selection is currently active.
     *
     * Used by the copy action to decide whether to consume the key or let it
     * fall through to the PTY as `\x03`.
     *
     * @return `true` if `boxSelection.active` is set.
     * @note MESSAGE THREAD.
     */
    bool hasSelection() const noexcept;

    /**
     * @brief Copies the current text selection to the system clipboard.
     *
     * Clears the selection after copying.
     *
     * @note MESSAGE THREAD.
     */
    void copySelection();

    /**
     * @brief Reads the system clipboard and writes its text to the pty.
     *
     * @note MESSAGE THREAD.
     */
    void pasteClipboard();

    /** @brief Writes raw bytes to the PTY. @note MESSAGE THREAD. */
    void writeToPty (const char* data, int len);

    /**
     * @brief Applies the current config to the renderer.
     *
     * Updates ligatures, embolden, and theme on Screen, then marks all cells
     * dirty so the next VBlank re-renders with the new settings.
     *
     * @note MESSAGE THREAD.
     */
    void applyConfig() noexcept;

    /**
     * @brief Increases the zoom multiplier by one step.
     *
     * @note MESSAGE THREAD.
     * @see applyZoom
     */
    void increaseZoom();

    /**
     * @brief Decreases the zoom multiplier by one step.
     *
     * @note MESSAGE THREAD.
     * @see applyZoom
     */
    void decreaseZoom();

    /**
     * @brief Resets the zoom multiplier to 1.0.
     *
     * @note MESSAGE THREAD.
     * @see applyZoom
     */
    void resetZoom();

    /**
     * @brief Initialises the terminal component after construction.
     *
     * Contains the shared initialization logic for both constructors.
     * Sets up screen, cursor, session callbacks, and ValueTree listeners.
     *
     * @note MESSAGE THREAD.
     */
    void initialise();

    /** @brief Returns the config font size, currently unmodified on all platforms. */
    static float dpiCorrectedFontSize() noexcept
    {
        return Config::getContext()->getFloat (Config::Key::fontSize);
    }

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
     * @brief Handles scrollback navigation via Shift+PgUp/PgDn/Home/End.
     *
     * PgUp/PgDn scroll by Config::Key::scrollbackStep lines.  Home/End jump to
     * the top or bottom of the scrollback buffer.
     *
     * @param keyCode  The key code of the pressed key.
     * @note MESSAGE THREAD.
     * @see Config::Key::scrollbackStep
     */
    void handleScrollNavigation (int keyCode);

    /**
     * @brief Clears the active text selection and resets scroll offset.
     *
     * Resets the scroll offset to zero (bottom of scrollback).  Called when a
     * non-navigation key is pressed.
     *
     * @note MESSAGE THREAD.
     */
    void clearSelectionAndScroll();

    /**
     * @brief Scales the font size by @p zoom and re-lays out the component.
     *
     * @param zoom  The zoom multiplier to apply (already clamped by the caller).
     * @note MESSAGE THREAD — called from increaseZoom(), decreaseZoom(), resetZoom().
     * @see Screen::setFontSize
     */
    void applyZoom (float zoom) noexcept;
    /**
     * @brief Computes this component's pixel offset relative to the top-level window.
     *
     * Used by renderGL() to set the correct GL viewport origin when the component
     * is offset by the tab bar.
     *
     * @return The pixel offset from the top-left corner of the top-level window.
     * @note MESSAGE THREAD.
     * @see renderGL
     */
    juce::Point<int> getOriginInTopLevel() const noexcept;

    /**
     * @brief Returns whether any mouse-tracking mode is currently active.
     *
     * Checks `mouseTracking`, `mouseMotionTracking`, and `mouseAllTracking`
     * modes in the Session state tree.
     *
     * @return @c true if any mouse-tracking mode is enabled.
     */
    bool isMouseTracking() const noexcept;

    /**
     * @brief Returns whether mouse events should be forwarded to the PTY.
     *
     * Returns true when the PTY should receive mouse events instead of the
     * component performing text selection:
     * - If any mouse-tracking mode is active (works on macOS/Linux).
     * - On Windows: if the alternate screen is active (ConPTY workaround —
     *   ConPTY intercepts DECSET mouse mode sequences so `isMouseTracking()`
     *   always returns false on Windows even when a TUI app requests tracking).
     *
     * @return @c true if mouse events should be forwarded to the PTY.
     */
    bool shouldForwardMouseToPty() const noexcept;

    //==============================================================================
    /** @brief GPU-accelerated terminal renderer; attached to this component. */
    Screen screen;

    /** @brief pty session, VT parser, and grid state machine. */
    Session session;

    /** @brief Cursor overlay component; positioned by updateCursorBounds(). */
    std::unique_ptr<CursorComponent> cursor;

    /**
     * @brief Rectangle (box) selection state driven by mouse drag.
     *
     * Stores the anchor (mouse-down) and end (current drag) grid coordinates
     * for a box selection.  The actual selected rectangle is
     * `[min(anchorCol,endCol), max(anchorCol,endCol)]` ×
     * `[min(anchorRow,endRow), max(anchorRow,endRow)]` — the same column
     * range applies to every row, making this a strict rectangle rather than
     * a row-wrapped region.
     *
     * `active` is set to `true` once the user has dragged at least one cell
     * away from the anchor.  A plain click (no drag) leaves `active` false
     * and clears any previous selection.
     */
    struct BoxSelection
    {
        int  anchorCol { 0 };  ///< Column of the mouse-down anchor cell.
        int  anchorRow { 0 };  ///< Row of the mouse-down anchor cell.
        int  endCol    { 0 };  ///< Column of the current drag end cell.
        int  endRow    { 0 };  ///< Row of the current drag end cell.
        bool active    { false }; ///< True when a non-degenerate selection exists.
    };

    /** @brief Current box selection state; `active` is false when nothing is selected. */
    BoxSelection boxSelection;

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

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Component)
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Terminal
