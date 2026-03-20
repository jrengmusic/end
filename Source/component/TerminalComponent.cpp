/**
 * @file TerminalComponent.cpp
 * @brief Implementation of the terminal UI host component.
 *
 * Wires the terminal backend (Session, Grid, Screen) to JUCE input events and
 * the VBlank render loop.  See TerminalComponent.h for the full architectural
 * overview.
 *
 * @see Terminal::Component
 * @see Terminal::Session
 * @see Terminal::Screen
 */

#include "TerminalComponent.h"
#include "../AppState.h"
#include "../config/Config.h"
#include "../terminal/action/Action.h"

/**
 * @brief Constructs Terminal::Component and wires all subsystems.
 *
 * Construction order:
 * 1. **Screen** — initialised with font family and base size from Config.
 * 2. **VBlankAttachment** — starts the render loop immediately.
 * 3. Screen settings (ligatures, embolden, theme) applied from Config.
 * 4. Saved zoom applied if > 1× (scales font size before first layout).
 * 5. **MessageOverlay** created and added as a hidden child.
 * 6. Session callbacks wired: `onShellExited`, `onClipboardChanged`, `onBell`.
 * 7. Startup config error (if any) shown asynchronously via MessageOverlay.
 *
 * @note MESSAGE THREAD — called from MainComponent constructor.
 */
Terminal::Component::Component()
    : screen()
    , vblank (this,
              [this]
              {
                  onVBlank();
              })
{
    initialise();
    session.getState().get().setProperty (jreng::ID::id, juce::Uuid().toString(), nullptr);
}

Terminal::Component* Terminal::Component::create (juce::Component& parent,
                                                  juce::Rectangle<int> bounds,
                                                  jreng::Owner<Component>& owner,
                                                  const juce::String& workingDirectory)
{
    auto terminal { workingDirectory.isNotEmpty() ? std::make_unique<Component> (workingDirectory)
                                                  : std::make_unique<Component>() };
    const auto uuid { terminal->getValueTree().getProperty (jreng::ID::id).toString() };
    terminal->setComponentID (uuid);
    terminal->setBounds (bounds);
    parent.addChildComponent (terminal.get());
    auto& ref { owner.add (std::move (terminal)) };
    return ref.get();
}

/**
 * @brief Constructs a terminal component starting in the given directory.
 *
 * @param workingDirectory  Absolute path for the shell's initial cwd.
 * @note MESSAGE THREAD.
 */
Terminal::Component::Component (const juce::String& workingDirectory)
    : screen()
    , vblank (this,
              [this]
              {
                  onVBlank();
              })
{
    session.setWorkingDirectory (workingDirectory);
    initialise();
    session.getState().get().setProperty (jreng::ID::id, juce::Uuid().toString(), nullptr);

}

/**
 * @brief Constructs a terminal component running a specific command.
 *
 * @param program           Shell command or executable path.
 * @param args              Arguments passed to the command.
 * @param workingDirectory  Initial cwd. Empty = inherit.
 * @note MESSAGE THREAD.
 */
Terminal::Component::Component (const juce::String& program,
                                const juce::String& args,
                                const juce::String& workingDirectory)
    : screen()
    , vblank (this,
              [this]
              {
                  onVBlank();
              })
{
    session.setShellProgram (program, args);
    session.setWorkingDirectory (workingDirectory);
    initialise();
    session.getState().get().setProperty (jreng::ID::id, juce::Uuid().toString(), nullptr);
}

/**
 * @brief Initialises the terminal component after construction.
 *
 * Contains the shared initialization logic for all constructors.
 * Sets up screen settings, session callbacks, and message overlay.
 *
 * @note MESSAGE THREAD.
 */
void Terminal::Component::initialise()
{
    auto* cfg { Config::getContext() };
    screen.setLigatures (cfg->getBool (Config::Key::fontLigatures));
    screen.setEmbolden (cfg->getBool (Config::Key::fontEmbolden));
    screen.setTheme (cfg->buildTheme());

    const float savedZoom { AppState::getContext()->getWindowZoom() };
    screen.setFontSize (dpiCorrectedFontSize() * savedZoom);

    setOpaque (false);
    setWantsKeyboardFocus (true);
    addKeyListener (this);

    session.onShellExited = [this]
    {
        juce::MessageManager::callAsync (
            [this]
            {
                if (onProcessExited != nullptr)
                    onProcessExited();
                else
                    juce::JUCEApplication::getInstance()->systemRequestedQuit();
            });
    };

    session.onClipboardChanged = [] (const juce::String& text)
    {
        juce::SystemClipboard::copyTextToClipboard (text);
    };

    session.onBell = []
    {
        std::fwrite ("\a", 1, 1, stderr);
    };

}

/**
 * @brief Tears down all listeners and detaches the Screen renderer.
 *
 * Destruction order is the reverse of construction:
 * 1. Clear the selection pointer on Screen before resetting the local unique_ptr.
 * 2. Detach Screen from the OpenGL context.
 * 3. Remove the key listener.
 *
 * @note MESSAGE THREAD.
 */
Terminal::Component::~Component()
{
    screen.setSelection (nullptr);
    screenSelection.reset();
    removeKeyListener (this);
}

/**
 * @brief Lays out the screen viewport, notifies Session of new grid size.
 *
 * Removes the title-bar height from the top, then applies the four configurable
 * padding values (`terminal.padding`) to produce the content area passed to
 * `Screen::setViewport()`.  After the viewport is set, the grid dimensions are
 * known and the Session is notified via `Session::resized()`.
 *
 * @note MESSAGE THREAD — called by JUCE on every resize event.
 */
void Terminal::Component::resized()
{
    auto contentArea { getLocalBounds() };
    contentArea.removeFromTop    (titleBarHeight);
    contentArea.removeFromTop    (paddingTop);
    contentArea.removeFromRight  (paddingRight);
    contentArea.removeFromBottom (paddingBottom);
    contentArea.removeFromLeft   (paddingLeft);

    if (contentArea.getWidth() > 0 and contentArea.getHeight() > 0)
    {
        screen.setViewport (contentArea);
        session.resized (screen.getNumCols(), screen.getNumRows());
        session.getState().setScrollOffset (0);
    }
}


bool Terminal::Component::hasSelection() const noexcept
{
    return boxSelection.active;
}

void Terminal::Component::copySelection()
{
    if (boxSelection.active)
    {
        const juce::ScopedLock lock (session.getGrid().getResizeLock());

        const juce::Point<int> topLeft {
            std::min (boxSelection.anchorCol, boxSelection.endCol),
            std::min (boxSelection.anchorRow, boxSelection.endRow) };
        const juce::Point<int> bottomRight {
            std::max (boxSelection.anchorCol, boxSelection.endCol),
            std::max (boxSelection.anchorRow, boxSelection.endRow) };

        const auto text { session.getGrid().extractBoxText (topLeft, bottomRight) };
        juce::SystemClipboard::copyTextToClipboard (text);

        boxSelection = BoxSelection {};
        screenSelection.reset();
        screen.setSelection (nullptr);
        session.getState().setSnapshotDirty();
    }
}

void Terminal::Component::pasteClipboard()
{
    session.paste (juce::SystemClipboard::getTextFromClipboard());
}

void Terminal::Component::writeToPty (const char* data, int len)
{
    session.writeToPty (data, len);
}

void Terminal::Component::increaseZoom()
{
    const float current { AppState::getContext()->getWindowZoom() };
    const float newZoom { juce::jlimit (Config::zoomMin, Config::zoomMax, current + 0.25f) };
    AppState::getContext()->setWindowZoom (newZoom);
    applyZoom (newZoom);
}

void Terminal::Component::decreaseZoom()
{
    const float current { AppState::getContext()->getWindowZoom() };
    const float newZoom { juce::jlimit (Config::zoomMin, Config::zoomMax, current - 0.25f) };
    AppState::getContext()->setWindowZoom (newZoom);
    applyZoom (newZoom);
}

void Terminal::Component::resetZoom()
{
    AppState::getContext()->setWindowZoom (Config::zoomMin);
    applyZoom (Config::zoomMin);
}

void Terminal::Component::handleScrollNavigation (int code)
{
    if (code == juce::KeyPress::pageUpKey)
    {
        const int maxOffset { session.getGrid().getScrollbackUsed() };
        const int current { session.getState().getScrollOffset() };
        const int page { screen.getNumRows() };
        const int clamped { juce::jlimit (0, maxOffset, current + page) };

        if (clamped != current)
        {
            session.getState().setScrollOffset (clamped);
            session.getState().setSnapshotDirty();
        }
    }
    else if (code == juce::KeyPress::pageDownKey)
    {
        const int current { session.getState().getScrollOffset() };
        const int page { screen.getNumRows() };
        const int clamped { juce::jmax (0, current - page) };

        if (clamped != current)
        {
            session.getState().setScrollOffset (clamped);
            session.getState().setSnapshotDirty();
        }
    }
    else if (code == juce::KeyPress::homeKey)
    {
        const int maxOffset { session.getGrid().getScrollbackUsed() };

        if (session.getState().getScrollOffset() != maxOffset)
        {
            session.getState().setScrollOffset (maxOffset);
            session.getState().setSnapshotDirty();
        }
    }
    else if (code == juce::KeyPress::endKey)
    {
        if (session.getState().getScrollOffset() != 0)
        {
            session.getState().setScrollOffset (0);
            session.getState().setSnapshotDirty();
        }
    }
}

void Terminal::Component::clearSelectionAndScroll()
{
    if (boxSelection.active or screenSelection != nullptr)
    {
        boxSelection = BoxSelection {};
        screenSelection.reset();
        screen.setSelection (nullptr);
        session.getState().setSnapshotDirty();
    }

    if (session.getState().getScrollOffset() > 0)
    {
        session.getState().setScrollOffset (0);
        session.getState().setSnapshotDirty();
    }
}

bool Terminal::Component::keyPressed (const juce::KeyPress& key, juce::Component*)
{
    // Popup terminals bypass the action system entirely.
    // All keys go directly to the PTY — identical to tmux's overlay model.
    if (onProcessExited != nullptr)
    {
        session.handleKeyPress (key);
        return true;
    }

    bool handled { Terminal::Action::getContext()->handleKeyPress (key) };

    if (not handled)
    {
        const int code { key.getKeyCode() };
        const auto mods { key.getModifiers() };

        const bool isScrollNav { mods.isShiftDown() and not mods.isCommandDown()
                                 and (code == juce::KeyPress::pageUpKey or code == juce::KeyPress::pageDownKey
                                      or code == juce::KeyPress::homeKey or code == juce::KeyPress::endKey) };

        if (isScrollNav)
        {
            handleScrollNavigation (code);
            handled = true;
        }
        else
        {
            clearSelectionAndScroll();
            session.handleKeyPress (key);
            handled = true;
        }
    }

    return handled;
}

/**
 * @brief Forwards mouse wheel scroll events to the PTY or scrolls the scrollback.
 *
 * Discrete mouse wheels scroll by a fixed `terminalScrollStep` lines per notch.
 * Smooth trackpad input accumulates fractional deltas in `scrollAccumulator`
 * and only scrolls when a whole-line threshold is crossed, preventing the
 * massive overscroll that occurs when every micro-event triggers a fixed jump.
 *
 * - **Mouse tracking active**: writes SGR scroll button (64 = up, 65 = down) at
 *   the clicked cell.
 * - **No tracking**: adjusts the scrollback offset.
 *
 * @param event  Mouse event; position used for SGR cell coordinates.
 * @param wheel  Wheel details; `deltaY > 0` = scroll up, `isSmooth` = trackpad.
 * @note MESSAGE THREAD.
 */
void Terminal::Component::mouseWheelMove (const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    const int scrollLines { Config::getContext()->getInt (Config::Key::terminalScrollStep) };
    const auto activeScreen { session.getState().getActiveScreen() };

    // Discrete mouse wheel: fixed step per notch (existing behaviour).
    // Smooth trackpad: accumulate fractional deltas, scroll whole lines only.
    if (not wheel.isSmooth)
    {
        const bool scrollUp { wheel.deltaY > 0.0f };

        if (activeScreen == Terminal::ActiveScreen::alternate)
        {
            if (shouldForwardMouseToPty())
            {
                const int button { scrollUp ? 64 : 65 };
                const auto cell { screen.cellAtPoint (event.x, event.y) };

                for (int i { 0 }; i < scrollLines; ++i)
                    session.writeMouseEvent (button, cell.x, cell.y, true);
            }
        }
        else
        {
            const int maxOffset { session.getGrid().getScrollbackUsed() };
            const int current { session.getState().getScrollOffset() };
            const int delta { scrollUp ? scrollLines : -scrollLines };
            const int clamped { juce::jlimit (0, maxOffset, current + delta) };

            if (clamped != current)
            {
                session.getState().setScrollOffset (clamped);
                session.getState().setSnapshotDirty();
            }
        }

        return;
    }

    // --- Smooth (trackpad) path ---
    // Scale the tiny per-frame delta by scrollLines so the config value still
    // controls overall speed, then accumulate until whole lines are reached.
    scrollAccumulator += wheel.deltaY * static_cast<float> (scrollLines) * trackpadDeltaScale;

    const int lines { static_cast<int> (scrollAccumulator) };

    if (lines == 0)
        return;

    scrollAccumulator -= static_cast<float> (lines);

    if (activeScreen == Terminal::ActiveScreen::alternate)
    {
        if (shouldForwardMouseToPty())
        {
            const int button { lines > 0 ? 64 : 65 };
            const auto cell { screen.cellAtPoint (event.x, event.y) };
            const int count { std::abs (lines) };

            for (int i { 0 }; i < count; ++i)
                session.writeMouseEvent (button, cell.x, cell.y, true);
        }
    }
    else
    {
        const int maxOffset { session.getGrid().getScrollbackUsed() };
        const int current { session.getState().getScrollOffset() };
        const int clamped { juce::jlimit (0, maxOffset, current + lines) };

        if (clamped != current)
        {
            session.getState().setScrollOffset (clamped);
            session.getState().setSnapshotDirty();
        }
    }
}

/**
 * @brief Begins a box selection or forwards a mouse-button SGR event.
 *
 * - **Mouse tracking active**: writes SGR button-0 press at the clicked cell.
 * - **No tracking**: records the anchor cell for a new box selection and clears
 *   any existing selection.  A plain click (no subsequent drag) will clear the
 *   selection on mouse-up.
 *
 * @param event  The mouse event.
 * @note MESSAGE THREAD.
 */
void Terminal::Component::mouseDown (const juce::MouseEvent& event)
{
    if (shouldForwardMouseToPty())
    {
        const auto cell { screen.cellAtPoint (event.x, event.y) };
        session.writeMouseEvent (0, cell.x, cell.y, true);
    }
    else
    {
        const auto cell { screen.cellAtPoint (event.x, event.y) };

        boxSelection.anchorCol = cell.x;
        boxSelection.anchorRow = cell.y;
        boxSelection.endCol    = cell.x;
        boxSelection.endRow    = cell.y;
        boxSelection.active    = false;

        screenSelection.reset();
        screen.setSelection (nullptr);
        session.getState().setSnapshotDirty();
    }
}

/**
 * @brief Forwards a double-click event to the PTY when mouse tracking is active.
 *
 * When mouse tracking is disabled, double-click is treated as a plain click
 * and the box selection anchor is reset to the clicked cell (no drag yet).
 *
 * @param event  The mouse event.
 * @note MESSAGE THREAD.
 */
void Terminal::Component::mouseDoubleClick (const juce::MouseEvent& event)
{
    if (shouldForwardMouseToPty())
    {
        const auto cell { screen.cellAtPoint (event.x, event.y) };
        session.writeMouseEvent (0, cell.x, cell.y, true);
    }
}

/**
 * @brief Extends the box selection or forwards a mouse-motion SGR event.
 *
 * - **Motion tracking active**: writes SGR button-32 (drag) at the current cell.
 * - **No tracking**: updates the box selection end cell to the current drag
 *   position, activates the selection, and marks the snapshot dirty so the
 *   selection overlay is redrawn on the next frame.
 *
 * @param event  The mouse event.
 * @note MESSAGE THREAD.
 */
void Terminal::Component::mouseDrag (const juce::MouseEvent& event)
{
    if (shouldForwardMouseToPty())
    {
        const auto cell { screen.cellAtPoint (event.x, event.y) };
        session.writeMouseEvent (32, cell.x, cell.y, true);
    }
    else
    {
        const auto cell { screen.cellAtPoint (event.x, event.y) };

        boxSelection.endCol = cell.x;
        boxSelection.endRow = cell.y;
        boxSelection.active = true;

        if (screenSelection == nullptr)
        {
            screenSelection = std::make_unique<ScreenSelection>();
        }

        screenSelection->anchor = { boxSelection.anchorCol, boxSelection.anchorRow };
        screenSelection->end    = { boxSelection.endCol,    boxSelection.endRow };

        screen.setSelection (screenSelection.get());
        session.getState().setSnapshotDirty();
    }
}

/**
 * @brief Finalises the box selection or forwards a mouse-button-release SGR event.
 *
 * - **Mouse tracking active**: writes an SGR button-0 release sequence.
 * - **No tracking**: if the selection has zero area (anchor == end, i.e. a plain
 *   click with no drag), clears the selection so a plain click leaves no
 *   degenerate selection visible.  A non-zero-area selection is kept visible.
 *
 * @param event  The mouse event.
 * @note MESSAGE THREAD.
 */
void Terminal::Component::mouseUp (const juce::MouseEvent& event)
{
    if (shouldForwardMouseToPty())
    {
        const auto cell { screen.cellAtPoint (event.x, event.y) };
        session.writeMouseEvent (0, cell.x, cell.y, false);
    }
    else
    {
        if (not boxSelection.active)
        {
            screenSelection.reset();
            screen.setSelection (nullptr);
            session.getState().setSnapshotDirty();
        }
    }
}

/**
 * @brief Returns true — the terminal accepts any file drag.
 *
 * @param files  Array of absolute file paths being dragged.
 * @return Always @c true.
 * @note MESSAGE THREAD.
 */
bool Terminal::Component::isInterestedInFileDrag (const juce::StringArray&)
{
    return true;
}

/**
 * @brief Pastes dropped file paths as text into the PTY.
 *
 * By default, paths are pasted raw (TUI-friendly).  When the configured
 * modifier key is held during the drop, paths are shell-quoted using the
 * quoting convention for the active shell program.
 *
 * @param files  Array of absolute file paths dropped onto the terminal.
 * @param x      Drop position x (unused).
 * @param y      Drop position y (unused).
 * @note MESSAGE THREAD.
 */
void Terminal::Component::filesDropped (const juce::StringArray& files, int, int)
{
    if (files.isEmpty())
        return;

    const auto* cfg { Config::getContext() };
    const juce::String multifiles { cfg->getString (Config::Key::terminalDropMultifiles) };
    const bool shouldQuote { cfg->getBool (Config::Key::terminalDropQuoted) };
    const juce::String separator { multifiles == "newline" ? "\n" : " " };

    juce::StringArray paths;

    for (const auto& file : files)
    {
        if (shouldQuote and (file.containsChar (' ') or file.containsChar ('\'')
            or file.containsChar ('"') or file.containsChar ('\\')
            or file.containsChar ('(') or file.containsChar (')')))
        {
            const juce::String shell { cfg->getString (Config::Key::shellProgram) };

            if (shell.contains ("cmd"))
                paths.add ("\"" + file + "\"");
            else
                paths.add ("'" + file.replace ("'", "'\\''") + "'");
        }
        else
        {
            paths.add (file);
        }
    }

    session.paste (paths.joinIntoString (separator));
}

// GL THREAD
void Terminal::Component::glContextCreated() noexcept
{
    screen.glContextCreated();
    session.getGrid().markAllDirty();
    session.getState().setSnapshotDirty();
}

// GL THREAD
void Terminal::Component::glContextClosing() noexcept { screen.glContextClosing(); }

juce::Point<int> Terminal::Component::getOriginInTopLevel() const noexcept
{
    const float scale { Fonts::getDisplayScale() };
    const auto* topLevel { getTopLevelComponent() };
    const auto relative { topLevel != nullptr ? topLevel->getLocalPoint (this, juce::Point<int> (0, 0))
                                              : juce::Point<int> (0, 0) };

    return { static_cast<int> (static_cast<float> (relative.x) * scale),
             static_cast<int> (static_cast<float> (relative.y) * scale) };
}

// GL THREAD
void Terminal::Component::renderGL() noexcept
{
    if (isVisible())
    {
        if (not screen.isGLContextReady())
            screen.glContextCreated();

        const auto origin { getOriginInTopLevel() };
        screen.renderOpenGL (origin.x, origin.y, getFullViewportHeight());
    }
}

// MESSAGE THREAD
void Terminal::Component::visibilityChanged()
{
    if (isVisible())
    {
        session.getGrid().markAllDirty();
        session.getState().setSnapshotDirty();
    }
}

/**
 * @brief Notifies the shell of focus gain and triggers a repaint.
 *
 * Sends a focus-in event to the pty so that applications using focus tracking
 * (e.g. Neovim) can respond.  The repaint updates the cursor appearance.
 *
 * @param cause  Unused.
 * @note MESSAGE THREAD.
 */
void Terminal::Component::focusGained (FocusChangeType)
{
    session.getState().setCursorFocused (true);
    session.writeFocusEvent (true);
    repaint();
}

/**
 * @brief Notifies the shell of focus loss and triggers a repaint.
 *
 * Sends a focus-out event to the pty.  The repaint updates the cursor
 * appearance (typically rendered as hollow when unfocused).
 *
 * @param cause  Unused.
 * @note MESSAGE THREAD.
 */
void Terminal::Component::focusLost (FocusChangeType)
{
    session.getState().setCursorFocused (false);
    session.writeFocusEvent (false);
    repaint();
}

/** @return Current number of visible grid rows as reported by Screen. */
int Terminal::Component::getGridRows() const noexcept { return screen.getNumRows(); }

/** @return Current number of visible grid columns as reported by Screen. */
int Terminal::Component::getGridCols() const noexcept { return screen.getNumCols(); }

juce::ValueTree Terminal::Component::getValueTree() noexcept { return session.getState().get(); }

/**
 * @brief VBlank callback: renders the grid if dirty, repositions cursor, updates visibility.
 *
 * Called every display refresh by `juce::VBlankAttachment`.
 *
 * Steps:
 * 1. Ensures this component is the focus container (brings to front if not).
 * 2. If `consumeSnapshotDirty()` returns true and the resize lock is available,
 *    calls `Screen::render()` with the current state and grid.
 * 3. Invokes `onRepaintNeeded` to trigger a GL repaint on the shared context.
 *
 * @note MESSAGE THREAD — VBlankAttachment callbacks run on the message thread.
 */
void Terminal::Component::onVBlank()
{
    if (getComponentID() == AppState::getContext()->getActiveTerminalUuid())
    {
        if (isShowing() and not hasKeyboardFocus (true))
        {
            toFront (true);
        }
    }

    if (session.getState().consumeSnapshotDirty())
    {
        const juce::ScopedTryLock lock (session.getGrid().getResizeLock());

        if (lock.isLocked())
        {
            screen.render (session.getState(), session.getGrid());

            if (onRepaintNeeded != nullptr)
                onRepaintNeeded();
        }
    }
}

/**
 * @brief Applies the current config to the renderer after a reload.
 *
 * Updates ligatures, embolden, and theme on Screen, then marks all cells dirty
 * and sets the snapshot dirty so the next VBlank re-renders with the new
 * settings.  Does not change font size (zoom is preserved across reloads).
 *
 * @note MESSAGE THREAD — called from reloadConfig().
 * @see Config::reload
 */
void Terminal::Component::applyConfig() noexcept
{
    auto* cfg { Config::getContext() };
    screen.setLigatures (cfg->getBool (Config::Key::fontLigatures));
    screen.setEmbolden (cfg->getBool (Config::Key::fontEmbolden));
    screen.setTheme (cfg->buildTheme());
    screen.setFontSize (dpiCorrectedFontSize() * AppState::getContext()->getWindowZoom());
    session.getGrid().markAllDirty();
    session.getState().setSnapshotDirty();
    resized();
}

/**
 * @brief Scales the font size by @p zoom and re-lays out the component.
 *
 * Computes the new font size as `baseSize * zoom` and calls
 * `Screen::setFontSize()`.
 *
 * @param zoom  The zoom multiplier to apply (already clamped by the caller).
 * @note MESSAGE THREAD — called from increaseZoom(), decreaseZoom(), resetZoom().
 * @see Screen::setFontSize
 * @see AppState::setWindowZoom
 */
void Terminal::Component::applyZoom (float zoom) noexcept
{
    screen.setFontSize (dpiCorrectedFontSize() * zoom);
    resized();
}

/**
 * @brief Returns whether any mouse-tracking mode is currently active.
 *
 * Checks the three mouse-tracking modes in the Session state tree:
 * - `mouseTracking` — basic button tracking (X10 / normal mode).
 * - `mouseMotionTracking` — button + motion tracking (button-event mode).
 * - `mouseAllTracking` — all motion tracking (any-event mode).
 *
 * @return @c true if any of the three modes is enabled.
 * @see Terminal::ID::mouseTracking
 * @see Terminal::ID::mouseMotionTracking
 * @see Terminal::ID::mouseAllTracking
 */
bool Terminal::Component::isMouseTracking() const noexcept
{
    const auto& st { session.getState() };
    return st.getTreeMode (Terminal::ID::mouseTracking) or st.getTreeMode (Terminal::ID::mouseMotionTracking)
           or st.getTreeMode (Terminal::ID::mouseAllTracking);
}

/**
 * @brief Returns whether mouse events should be forwarded to the PTY.
 *
 * Returns true when the PTY should receive mouse events instead of the
 * component performing text selection — i.e. when any mouse-tracking mode
 * is active.
 *
 * On Windows, `isMouseTracking()` works correctly because the sideloaded
 * ConPTY (`conpty.dll` + `OpenConsole.exe`) forwards DECSET mouse mode
 * sequences through the pipe to END's parser, unlike the inbox conhost
 * which intercepts them.
 *
 * @return @c true if mouse events should be forwarded to the PTY.
 * @note MESSAGE THREAD.
 */
bool Terminal::Component::shouldForwardMouseToPty() const noexcept
{
    return isMouseTracking();
}
