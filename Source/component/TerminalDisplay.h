/**
 * @file TerminalDisplay.h
 * @brief Ephemeral UI component that renders a Terminal::Processor.
 *
 * Terminal::Display is the editor analog in the Processor/Editor pattern:
 *
 * - **Session** (Processor) — owns PTY state, always alive.
 * - **Display** (Editor) — ephemeral UI; created on demand, destroyed on pane close.
 *
 * Display holds a `sessionId` string and reaches its Session via
 * `Nexus::Session::getContext()->get(sessionId)`.  It never owns the Session.
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
 * @see Terminal::Processor
 * @see Nexus::Session
 * @see MessageOverlay
 * @see Config
 */

#pragma once
#include <variant>
#include <JuceHeader.h>
#include "../terminal/logic/Processor.h"
#include "../terminal/rendering/Screen.h"
#include "../terminal/rendering/ScreenSelection.h"
#include "../SelectionType.h"
#include "../terminal/selection/LinkSpan.h"
#include "../terminal/selection/LinkManager.h"
#include "../nexus/Session.h"
#include "InputHandler.h"
#include "MouseHandler.h"
#include "PaneComponent.h"
#include "config/Config.h"

namespace Terminal
{ /*____________________________________________________________________________*/
/**
 * @class Terminal::Display
 * @brief Ephemeral JUCE component that renders a Terminal::Processor.
 *
 * Inherits `jreng::GLComponent` for GL-backed rendering and layout, `juce::KeyListener` for
 * keyboard input.  Cursor rendering is handled by `Screen::drawCursor()` in
 * the GL pipeline — no JUCE component overlay.
 *
 * Created via `Processor::createDisplay()`.
 * Destroyed when the pane or popup is closed; destructor unwires all Session callbacks.
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
 * @see Terminal::Processor
 * @see Nexus::Session
 */
class Display
    : public PaneComponent
    , public juce::KeyListener
    , public juce::FileDragAndDropTarget
    , public juce::ChangeListener
{
public:
    /** @brief The Processor this Display renders. Lifetime owned by the caller. */
    Terminal::Processor& processor;

    /**
     * @brief Constructs a Display for the given Processor.
     *
     * Stores a reference to @p proc, subscribes as a ChangeListener, initialises
     * the screen renderer and VBlank loop, wires all Processor callbacks, and
     * applies the current Config.
     *
     * @param proc  The Processor this Display renders.
     * @param font  Font instance providing metrics, shaping, and rasterisation.
     * @note MESSAGE THREAD.
     */
    Display (Terminal::Processor& proc, jreng::Typeface& font);

    /**
     * @brief Unwires all Processor callbacks, unsubscribes as ChangeListener,
     *        and tears down the Screen renderer.
     *
     * @note MESSAGE THREAD.
     */
    ~Display() override;

    /**
     * @brief Called by Processor via ChangeBroadcaster when state has been updated.
     *
     * Marks the snapshot dirty and requests a repaint so `onVBlank()` picks up
     * the new state on the next frame.
     *
     * @param source  The ChangeBroadcaster that fired (ignored).
     * @note MESSAGE THREAD.
     */
    void changeListenerCallback (juce::ChangeBroadcaster* source) override;

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
     * @brief Updates the mouse cursor on hover.
     *
     * Hit-tests the mouse position against clickable links via
     * `LinkManager::hitTest()`: shows `PointingHandCursor` over a link,
     * `NormalCursor` otherwise.  Inactive when any modal is active or
     * when mouse tracking is forwarded to the PTY.
     *
     * @param event  The mouse event.
     * @note MESSAGE THREAD.
     */
    void mouseMove (const juce::MouseEvent& event) override;

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
     * @brief Returns true — the terminal accepts any file drag.
     *
     * @param files  Array of absolute file paths being dragged.
     * @return Always @c true.
     * @note MESSAGE THREAD.
     */
    bool isInterestedInFileDrag (const juce::StringArray& files) override;

    /**
     * @brief Pastes dropped file paths as text into the PTY.
     *
     * When `terminal.drop_quoted` is true, paths containing shell-special
     * characters are quoted using the convention for the active shell.
     * Multiple files are joined by the configured separator.
     *
     * @param files  Array of absolute file paths dropped onto the terminal.
     * @param x      Drop position x (unused).
     * @param y      Drop position y (unused).
     * @note MESSAGE THREAD.
     */
    void filesDropped (const juce::StringArray& files, int x, int y) override;

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
    juce::ValueTree getValueTree() noexcept override;

    /**
     * @brief Called by GLRenderer when the shared OpenGL context is first created.
     *
     * Forwards to Screen::glContextCreated() to compile shaders and create GPU
     * buffers.  Also calls jreng::BackgroundBlur::enableWindowTransparency() for
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
    void paintGL() noexcept override;

    /**
     * @brief Renders the terminal via juce::Graphics (CPU path).
     *
     * Active during Plan 3.4 Graphics-only validation.  Delegates to
     * Screen::renderPaint() using the component's local coordinate space
     * (originX/Y = 0, fullHeight = getHeight()).
     *
     * @param g  The graphics context from JUCE's paint cycle.
     * @note MESSAGE THREAD.
     */
    void paint (juce::Graphics& g) override;

    /**
     * @brief Callback invoked when the shell process exits.
     *
     * If set, this replaces the default behaviour (quit the application).
     * Used by popup terminals to dismiss the popup window on process exit.
     * Also gates the keyPress bypass: when set, all keys go to the PTY
     * directly (popup model). Do NOT set this on regular pane terminals.
     *
     * @note MESSAGE THREAD (via callAsync).
     */
    std::function<void()> onProcessExited;

    /**
     * @brief Callback invoked whenever the shell process exits.
     *
     * Fired unconditionally on shell exit, before the onProcessExited /
     * systemRequestedQuit decision. Use this to close a pane without
     * affecting the keyPress routing that onProcessExited controls.
     *
     * @note MESSAGE THREAD (via callAsync).
     */
    std::function<void()> onShellExited;

    /**
     * @brief Callback invoked when a .md file link is activated.
     *
     * Propagated from LinkManager via the callback chain to Panes.
     *
     * @note MESSAGE THREAD.
     */
    std::function<void (const juce::File&)> onOpenMarkdown;

    /**
     * @brief Returns `true` if a non-degenerate selection is currently active.
     *
     * Used by the copy action to decide whether to consume the key or let it
     * fall through to the PTY as `\x03`.
     *
     * @return `true` if `screenSelection` is non-null.
     * @note MESSAGE THREAD.
     */
    bool hasSelection() const noexcept override;

    /**
     * @brief Copies the current text selection to the system clipboard.
     *
     * Clears the selection after copying.
     *
     * @note MESSAGE THREAD.
     */
    void copySelection() noexcept override;

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
    juce::String getPaneType() const noexcept override { return App::ID::paneTypeTerminal; }
    void applyConfig() noexcept override;

    /**
     * @brief Switches the active rendering backend at runtime.
     *
     * Replaces the active Screen variant with the appropriate renderer for
     * @p type, reconstructs InputHandler and MouseHandler with the new
     * ScreenBase reference, reapplies config, and forces a full repaint.
     *
     * @param type  The desired rendering backend.
     * @note MESSAGE THREAD.
     */
    void switchRenderer (App::RendererType type) override;

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
     * @brief Enters vim-style selection mode, placing the cursor at the current
     *        terminal cursor position.
     *
     * Computes the cursor row in scrollback-aware coordinates (scrollback rows
     * above the visible screen + the in-screen cursor row), then writes the
     * cursor and anchor to State and sets modal type to `selection` so the
     * next frame draws the selection overlay.
     *
     * Called by the action registration (step 3) and by mouse handlers (step 7).
     *
     * @note MESSAGE THREAD.
     */
    void enterSelectionMode() noexcept override;

    /**
     * @brief Enters open-file hint-label mode.
     *
     * Sets the modal type to `ModalType::openFile` and marks the snapshot
     * dirty so the next frame renders the hint-label overlay.  Span indexing
     * and label assignment will be wired here in a later step.
     *
     * Called by the action registration (step 3).
     *
     * @note MESSAGE THREAD.
     */
    void enterOpenFileMode() noexcept;

    /**
     * @brief Returns `true` when the terminal is currently in vim-style selection mode.
     *
     * Used by MainComponent to guard tab/pane switch exit logic.
     *
     * @return `true` if the modal type is `ModalType::selection`.
     * @note MESSAGE THREAD.
     */
    bool isInSelectionMode() const noexcept;

    /**
     * @brief Exits vim-style selection mode, clearing the selection highlight.
     *
     * Resets selection state in State (modal type, selection type), clears
     * `screenSelection` and the screen selection pointer, and marks the
     * snapshot dirty.
     *
     * Called by MainComponent before tab or pane switches so that selection mode
     * does not persist on a terminal that is no longer focused.
     *
     * @note MESSAGE THREAD.
     */
    void exitSelectionMode() noexcept;

    /**
     * @brief Returns the current selection type as its integer representation.
     *
     * Lock-free atomic read from State.  Used by MainComponent to poll the
     * active terminal's selection state every VBlank frame and update the
     * StatusBarOverlay without a callback chain.
     *
     * @return Integer cast of the current SelectionType.
     * @note ANY THREAD — lock-free, noexcept.
     */
    int getSelectionType() const noexcept;

    /**
     * @brief Returns the currently active modal type.
     *
     * Lock-free atomic read from State.  Used by MainComponent to poll the
     * active terminal's modal state every VBlank frame and update the
     * StatusBarOverlay without a callback chain.
     *
     * @return The active ModalType, or `ModalType::none` if no modal is active.
     * @note ANY THREAD — lock-free, noexcept.
     */
    ModalType getModalType() const noexcept;

    /**
     * @brief Returns the current hint page index (0-based).
     *
     * Lock-free atomic read from State.  Used by MainComponent to poll the
     * active terminal's hint page state every VBlank frame and update the
     * StatusBarOverlay without a callback chain.
     *
     * @return Zero-based page index.
     * @note ANY THREAD — lock-free, noexcept.
     */
    int getHintPage() const noexcept;

    /**
     * @brief Returns the total number of hint pages.
     *
     * Lock-free atomic read from State.  Used by MainComponent to poll the
     * active terminal's hint total-page count every VBlank frame and update
     * the StatusBarOverlay without a callback chain.
     *
     * @return Total page count (0 when no hints are active).
     * @note ANY THREAD — lock-free, noexcept.
     */
    int getHintTotalPages() const noexcept;

    /**
     * @brief Initialises the terminal display after construction.
     *
     * Contains the shared initialization logic for the constructor.
     * Sets up screen settings, session callbacks, and message overlay.
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
     * @brief VBlank callback: renders the grid if dirty.
     *
     * Called every display refresh by `juce::VBlankAttachment`.  Steps:
     * 1. Ensures this component is the focus container.
     * 2. If the snapshot is dirty and the resize lock is available:
     *    a. Rebuilds `screenSelection` from State params (SSOT for selection rendering).
     *    b. Calls `Screen::render()`.
     *
     * @note MESSAGE THREAD — VBlankAttachment callbacks run on the message thread.
     */
    void onVBlank();

    /**
     * @brief Applies current config settings (ligatures, embolden, theme, font size) to the active screen.
     *
     * Extracted shared body called by both `applyConfig()` and `switchRenderer()`.
     *
     * @note MESSAGE THREAD.
     */
    void applyScreenSettings() noexcept;

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
     * Used by paintGL() to set the correct GL viewport origin when the component
     * is offset by the tab bar.
     *
     * @return The pixel offset from the top-left corner of the top-level window.
     * @note MESSAGE THREAD.
     * @see paintGL
     */
    juce::Point<int> getOriginInTopLevel() const noexcept;

    /**
     * @brief Clamps @p newOffset to [0, scrollbackUsed] and applies it if changed.
     *
     * Single SSOT for all scrollback offset mutations.  Reads the current
     * offset and the scrollback high-water mark, clamps @p newOffset into the
     * valid range, and — only if the clamped value differs from the current
     * offset — writes it back and marks the snapshot dirty.
     *
     * @param newOffset  Desired scroll offset in lines (positive = scrolled back).
     * @note MESSAGE THREAD.
     */
    void setScrollOffsetClamped (int newOffset) noexcept;

    /** @brief Returns the ScreenBase interface for the active Screen variant. */
    ScreenBase& screenBase() noexcept
    {
        return std::visit ([] (auto& s) -> ScreenBase& { return s; }, screen);
    }

    /**
     * @brief Visits the active Screen variant, forwarding the return value.
     *
     * @tparam Func  Callable accepting any Screen<T> specialisation by reference.
     */
    template <typename Func>
    decltype(auto) visitScreen (Func&& func)
    {
        return std::visit (std::forward<Func> (func), screen);
    }


    //==============================================================================
    /** @brief Font reference; lifetime owned by MainComponent. */
    jreng::Typeface& font;

    /**
     * @brief Terminal renderer — variant over CPU and GPU Screen specialisations.
     *
     * GraphicsContext is the first alternative and the default (CPU path).
     * switchRenderer() emplaces the appropriate alternative at runtime.
     */
    using ScreenVariant = std::variant<
        Screen<jreng::Glyph::GraphicsContext>,
        Screen<jreng::Glyph::GLContext>>;

    ScreenVariant screen;


    /** @brief Current text selection; null when nothing is selected. */
    std::unique_ptr<ScreenSelection> screenSelection;

    /**
     * @brief Keyboard input handler: modal dispatch, vim selection, open-file keys.
     *
     * Held as optional so it can be emplaced after screen is initialised and
     * re-emplaced when switchRenderer() swaps the Screen variant.
     */
    std::optional<InputHandler> inputHandler;

    /**
     * @brief Mouse input handler: PTY forwarding, drag selection, link dispatch.
     *
     * Held as optional for the same reason as inputHandler.
     */
    std::optional<MouseHandler> mouseHandler;

    /**
     * @brief Link manager: owns viewport scanning, hit-testing, and dispatch.
     *
     * Handles both hover-underline (`clickableLinks`) and open-file hint-label
     * (`hintLinks`) modes.  Rescanned reactively via ValueTree listener when the
     * prompt row, active screen, or HYPERLINKS node changes.
     *
     * Held as optional because it requires Session references which are resolved
     * — emplaced in initialise() after Processor reference is stored.
     */
    std::optional<LinkManager> linkManager;

    /** @brief VBlank attachment that drives the render loop at display refresh rate. */
    juce::VBlankAttachment vblank;

    //==============================================================================
    /**
     * @brief Height of the native title bar in pixels; 0 when buttons are hidden.
     *
     * Read from Config at construction.  When `window.buttons = true` the
     * traffic-light buttons occupy 24 px at the top of the component.
     */
    const int titleBarHeight { Config::getContext()->getBool (Config::Key::windowButtons) ? App::titleBarHeight : 0 };

    /** @brief Grid padding — top edge inset in logical pixels (from `terminal.padding`). */
    const int paddingTop    { Config::getContext()->getInt (Config::Key::terminalPaddingTop) };

    /** @brief Grid padding — right edge inset in logical pixels (from `terminal.padding`). */
    const int paddingRight  { Config::getContext()->getInt (Config::Key::terminalPaddingRight) };

    /** @brief Grid padding — bottom edge inset in logical pixels (from `terminal.padding`). */
    const int paddingBottom { Config::getContext()->getInt (Config::Key::terminalPaddingBottom) };

    /** @brief Grid padding — left edge inset in logical pixels (from `terminal.padding`). */
    const int paddingLeft   { Config::getContext()->getInt (Config::Key::terminalPaddingLeft) };

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Display)
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Terminal
