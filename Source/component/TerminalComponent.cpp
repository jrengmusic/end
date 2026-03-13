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
#include "../config/KeyBinding.h"
#include "../config/ModalKeyBinding.h"





/**
 * @brief Constructs Terminal::Component and wires all subsystems.
 *
 * Construction order:
 * 1. **Screen** — initialised with font family and base size from Config.
 * 2. **stateTree** — snapshot of the Session state ValueTree for listener
 *    registration (must be taken before the VBlankAttachment starts).
 * 3. **VBlankAttachment** — starts the render loop immediately.
 * 4. Screen settings (ligatures, embolden, theme) applied from Config.
 * 5. Saved zoom applied if > 1× (scales font size before first layout).
 * 6. **CursorComponent** created with the cursor state subtree and Fonts ref.
 * 7. **MessageOverlay** created and added as a hidden child.
 * 8. Session callbacks wired: `onShellExited`, `onClipboardChanged`, `onBell`.
 * 9. ValueTree listener registered on `stateTree`.
 * 10. Startup config error (if any) shown asynchronously via MessageOverlay.
 *
 * @note MESSAGE THREAD — called from MainComponent constructor.
 */
Terminal::Component::Component()
    : screen()
    , stateTree (session.getState().get())
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
    auto terminal { workingDirectory.isNotEmpty()
                        ? std::make_unique<Component> (workingDirectory)
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
    , stateTree (session.getState().get())
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
 * @brief Initialises the terminal component after construction.
 *
 * Contains the shared initialization logic for both constructors.
 * Sets up screen, cursor, session callbacks, and ValueTree listeners.
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

    cursor = std::make_unique<CursorComponent> (session.getCursorState());

    setOpaque (false);
    setWantsKeyboardFocus (true);
    addKeyListener (this);

    addAndMakeVisible (cursor.get());

    session.onShellExited = []
    {
        juce::MessageManager::callAsync (
            []
            {
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

    stateTree.addListener (this);
}

/**
 * @brief Tears down all listeners and detaches the Screen renderer.
 *
 * Destruction order is the reverse of construction:
 * 1. Remove ValueTree listener to prevent callbacks into a partially-destroyed object.
 * 2. Clear the selection pointer on Screen before resetting the local unique_ptr.
 * 3. Detach Screen from the OpenGL context.
 * 5. Remove the key listener.
 *
 * @note MESSAGE THREAD.
 */
Terminal::Component::~Component()
{
    stateTree.removeListener (this);
    screen.setSelection (nullptr);
    screenSelection.reset();
    removeKeyListener (this);
}

/**
 * @brief Lays out the screen viewport, notifies Session of new grid size.
 *
 * Removes the title-bar height from the top, then applies horizontal and
 * vertical insets to produce the content area passed to `Screen::setViewport()`.
 * After the viewport is set, the grid dimensions are known and the Session is
 * notified via `Session::resized()`.  The cursor is repositioned.
 *
 * @note MESSAGE THREAD — called by JUCE on every resize event.
 */
void Terminal::Component::resized()
{
    auto contentArea { getLocalBounds() };
    contentArea.removeFromTop (titleBarHeight);
    contentArea = contentArea.reduced (horizontalInset, verticalInset);

    if (contentArea.getWidth() > 0 and contentArea.getHeight() > 0)
    {
        screen.setViewport (contentArea);
        session.resized (screen.getNumCols(), screen.getNumRows());
        session.getState().setScrollOffset (0);
        updateCursorBounds();
    }
}

void Terminal::Component::copySelection()
{
    if (screenSelection != nullptr)
    {
        const juce::ScopedLock lock (session.getGrid().getResizeLock());
        const auto text { session.getGrid().extractText (screenSelection->anchor, screenSelection->end) };
        juce::SystemClipboard::copyTextToClipboard (text);
        screenSelection.reset();
        screen.setSelection (nullptr);
        session.getState().setSnapshotDirty();
    }
}

void Terminal::Component::pasteClipboard()
{
    session.paste (juce::SystemClipboard::getTextFromClipboard());
    cursor->resetBlink();
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
    if (screenSelection != nullptr)
    {
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
    if (ModalKeyBinding::getContext()->handleKeyPress (key))
        return true;

    const int code { key.getKeyCode() };
    const auto mods { key.getModifiers() };

    const bool isScrollNav { mods.isShiftDown() and not mods.isCommandDown()
                             and (code == juce::KeyPress::pageUpKey or code == juce::KeyPress::pageDownKey
                                  or code == juce::KeyPress::homeKey or code == juce::KeyPress::endKey) };

    bool handled { false };

    if (isScrollNav)
    {
        handleScrollNavigation (code);
        handled = true;
    }
    else
    {
        const bool isAppCommand {
#if JUCE_MAC
            mods.isCommandDown()
#elif JUCE_WINDOWS
            KeyBinding::isCommandKeyPress (key)
            or (mods.isAltDown() and code == juce::KeyPress::F4Key)
#else
            KeyBinding::isCommandKeyPress (key)
#endif
        };

        const bool isAltBlocked {
#if JUCE_MAC
            mods.isAltDown()
#else
            false
#endif
        };

        if (not isAppCommand and not isAltBlocked)
        {
            clearSelectionAndScroll();
            session.handleKeyPress (key);
            cursor->resetBlink();
            handled = true;
        }
    }

    return handled;
}

/**
 * @brief Scrolls the scrollback buffer or forwards a mouse-wheel SGR event.
 *
 * When any mouse-tracking mode is active, encodes the scroll direction as
 * button 64 (wheel up) or 65 (wheel down) and writes an SGR 1006 sequence.
 * Otherwise adjusts the scroll offset by `Config::Key::scrollbackStep` lines,
 * clamped to `[0, scrollbackUsed]`.
 *
 * @param event  Mouse event; position used for SGR cell coordinates.
 * @param wheel  Wheel details; `deltaY > 0` = scroll up.
 * @note MESSAGE THREAD.
 */
void Terminal::Component::mouseWheelMove (const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    auto& st { session.getState() };

    if (isMouseTracking())
    {
        const auto cell { screen.cellAtPoint (event.x, event.y) };
        const int scrollButton { wheel.deltaY > 0.0f ? 64 : 65 };
        session.writeMouseEvent (scrollButton, cell.x, cell.y, true);
    }
    else
    {
        const int maxOffset { session.getGrid().getScrollbackUsed() };
        const int current { st.getScrollOffset() };
        const int step { Config::getContext()->getInt (Config::Key::scrollbackStep) };
        const int delta { wheel.deltaY > 0.0f ? step : -step };
        const int clamped { juce::jlimit (0, maxOffset, current + delta) };

        if (clamped != current)
        {
            st.setScrollOffset (clamped);
            st.setSnapshotDirty();
        }
    }
}

/**
 * @brief Begins a text selection or forwards a mouse-button SGR event.
 *
 * - **Mouse tracking active**: writes SGR button-0 press at the clicked cell.
 * - **Triple-click** (primary screen only): selects the entire row.
 * - **Single click** (primary screen only): starts a new selection anchored at
 *   the clicked cell.
 *
 * @param event  The mouse event.
 * @note MESSAGE THREAD.
 */
void Terminal::Component::mouseDown (const juce::MouseEvent& event)
{
    const auto& st { session.getState() };

    if (isMouseTracking())
    {
        const auto cell { screen.cellAtPoint (event.x, event.y) };
        session.writeMouseEvent (0, cell.x, cell.y, true);
    }
    else if (st.getScreen() != Terminal::ActiveScreen::alternate)
    {
        const auto cell { screen.cellAtPoint (event.x, event.y) };

        if (event.getNumberOfClicks() >= 3)
        {
            screenSelection = std::make_unique<Terminal::ScreenSelection>();
            screenSelection->anchor = { 0, cell.y };
            screenSelection->end = { session.getGrid().getCols() - 1, cell.y };
            screen.setSelection (screenSelection.get());
            session.getState().setSnapshotDirty();
        }
        else
        {
            screenSelection = std::make_unique<Terminal::ScreenSelection>();
            screenSelection->anchor = cell;
            screenSelection->end = cell;
            screen.setSelection (screenSelection.get());
            session.getState().setSnapshotDirty();
        }
    }
}

/**
 * @brief Selects the word under the cursor on double-click.
 *
 * Scans left from the clicked cell while `codepoint > 0x20`, then right while
 * `codepoint > 0x20`, to find word boundaries.  Only active on the primary
 * screen and when mouse tracking is disabled.  Cells with codepoint ≤ 0x20
 * (space, NUL) are treated as word separators.
 *
 * @param event  The mouse event.
 * @note MESSAGE THREAD.
 */
void Terminal::Component::mouseDoubleClick (const juce::MouseEvent& event)
{
    const auto& st { session.getState() };

    if (not isMouseTracking() and st.getScreen() != Terminal::ActiveScreen::alternate)
    {
        const auto cell { screen.cellAtPoint (event.x, event.y) };
        const juce::ScopedLock lock (session.getGrid().getResizeLock());
        const Terminal::Cell* cells { session.getGrid().activeVisibleRow (cell.y) };

        if (cells != nullptr)
        {
            const int numCols { session.getGrid().getCols() };
            int wordStart { cell.x };
            int wordEnd { cell.x };

            const uint32_t cp { (*(cells + cell.x)).codepoint };

            if (cp > 0x20)
            {
                while (wordStart > 0 and (*(cells + wordStart - 1)).codepoint > 0x20)
                    --wordStart;

                while (wordEnd < numCols - 1 and (*(cells + wordEnd + 1)).codepoint > 0x20)
                    ++wordEnd;

                screenSelection = std::make_unique<Terminal::ScreenSelection>();
                screenSelection->anchor = { wordStart, cell.y };
                screenSelection->end = { wordEnd, cell.y };
                screen.setSelection (screenSelection.get());
                session.getState().setSnapshotDirty();
            }
        }
    }
}

/**
 * @brief Extends the selection or forwards a mouse-motion SGR event.
 *
 * In motion-tracking or all-tracking mode, writes an SGR button-32 (drag)
 * sequence at the current cell.  Otherwise extends `screenSelection->end` to
 * the dragged cell and marks the snapshot dirty for re-render.
 *
 * @param event  The mouse event.
 * @note MESSAGE THREAD.
 */
void Terminal::Component::mouseDrag (const juce::MouseEvent& event)
{
    const auto& st { session.getState() };
    const bool motionTracking { st.getTreeMode (Terminal::ID::mouseMotionTracking)
                                or st.getTreeMode (Terminal::ID::mouseAllTracking) };

    if (motionTracking)
    {
        const auto cell { screen.cellAtPoint (event.x, event.y) };
        session.writeMouseEvent (32, cell.x, cell.y, true);
    }
    else if (st.getScreen() != Terminal::ActiveScreen::alternate)
    {
        if (screenSelection != nullptr)
        {
            const auto cell { screen.cellAtPoint (event.x, event.y) };
            screenSelection->end = cell;
            session.getState().setSnapshotDirty();
        }
    }
}

/**
 * @brief Finalises the selection or forwards a mouse-button-release SGR event.
 *
 * In mouse-tracking mode writes an SGR button-0 release sequence.  Otherwise,
 * if the selection has zero area (anchor == end), clears it so a plain click
 * does not leave a degenerate selection.
 *
 * @param event  The mouse event.
 * @note MESSAGE THREAD.
 */
void Terminal::Component::mouseUp (const juce::MouseEvent& event)
{
    const auto& st { session.getState() };

    if (isMouseTracking())
    {
        const auto cell { screen.cellAtPoint (event.x, event.y) };
        session.writeMouseEvent (0, cell.x, cell.y, false);
    }
    else if (st.getScreen() != Terminal::ActiveScreen::alternate)
    {
        if (screenSelection != nullptr)
        {
            if (screenSelection->anchor == screenSelection->end)
            {
                screenSelection.reset();
                screen.setSelection (nullptr);
                session.getState().setSnapshotDirty();
            }
        }
    }
}

// GL THREAD
void Terminal::Component::glContextCreated() noexcept
{
    screen.glContextCreated();
    session.getGrid().markAllDirty();
    session.getState().setSnapshotDirty();
}

// GL THREAD
void Terminal::Component::glContextClosing() noexcept
{
    screen.glContextClosing();
}

juce::Point<int> Terminal::Component::getOriginInTopLevel() const noexcept
{
    const float scale { Fonts::getDisplayScale() };
    const auto* topLevel { getTopLevelComponent() };
    const auto relative { topLevel != nullptr
        ? topLevel->getLocalPoint (this, juce::Point<int> (0, 0))
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
    session.writeFocusEvent (false);
    repaint();
}

/** @return Current number of visible grid rows as reported by Screen. */
int Terminal::Component::getGridRows() const noexcept { return screen.getNumRows(); }

/** @return Current number of visible grid columns as reported by Screen. */
int Terminal::Component::getGridCols() const noexcept { return screen.getNumCols(); }

juce::ValueTree Terminal::Component::getValueTree() noexcept
{
    return session.getState().get();
}

/**
 * @brief VBlank callback: renders the grid if dirty, updates cursor visibility.
 *
 * Called every display refresh by `juce::VBlankAttachment`.
 *
 * Steps:
 * 1. Ensures this component is the focus container (brings to front if not).
 * 2. If `consumeSnapshotDirty()` returns true and the resize lock is available,
 *    calls `Screen::render()` with the current state and grid.
 * 3. Invokes `onRepaintNeeded` to trigger a GL repaint on the shared context.
 * 4. Updates cursor visibility: hidden when scrolled back or when the terminal
 *    application has hidden the cursor via DEC private mode.
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

    const bool scrolledBack { session.getState().getScrollOffset() > 0 };
    const auto activeScreen { session.getState().getScreen() };
    const bool cursorMode { session.getState().isCursorVisible (activeScreen) };
    const bool focused { hasKeyboardFocus (true) };
    cursor->setVisible (not scrolledBack and cursorMode and focused);
}

/**
 * @brief Reacts to cursor position and active-screen changes in the ValueTree.
 *
 * Listens for `Terminal::ID::value` property changes on `Terminal::ID::PARAM`
 * nodes.  Dispatches on the `id` property of the changed node:
 * - `activeScreen` — rebinds the cursor to the new screen's state tree so that
 *   the cursor tracks the correct screen when switching between primary and
 *   alternate.
 * - `cursorRow` / `cursorCol` — repositions the CursorComponent.
 *
 * @param tree      The ValueTree node that changed.
 * @param property  The property identifier that changed.
 * @note MESSAGE THREAD — called by JUCE ValueTree notification.
 */
void Terminal::Component::valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property)
{
    if (property == Terminal::ID::value and tree.getType() == Terminal::ID::PARAM)
    {
        const auto paramId { juce::Identifier (tree.getProperty (Terminal::ID::id)) };

        if (paramId == Terminal::ID::activeScreen)
        {
            cursor->rebindToScreen (session.getCursorState());
            updateCursorBounds();
        }

        if (paramId == Terminal::ID::cursorRow or paramId == Terminal::ID::cursorCol)
        {
            updateCursorBounds();
        }
    }
}

/**
 * @brief Repositions CursorComponent to the current cursor cell bounds.
 *
 * Reads `cursorRow` and `cursorCol` from the cursor ValueTree subtree, queries
 * `Screen::getCellBounds()` for the pixel rectangle of that cell, then adjusts
 * the width for wide (double-width) cursor glyphs via `CursorComponent::getCellWidth()`.
 *
 * @note MESSAGE THREAD — called from valueTreePropertyChanged() and resized().
 */
void Terminal::Component::updateCursorBounds() noexcept
{
    const juce::ValueTree cursorState { session.getCursorState() };
    const auto rowParam { cursorState.getChildWithProperty (Terminal::ID::id, Terminal::ID::cursorRow.toString()) };
    const auto colParam { cursorState.getChildWithProperty (Terminal::ID::id, Terminal::ID::cursorCol.toString()) };

    const int row { rowParam.isValid() ? static_cast<int> (rowParam.getProperty (Terminal::ID::value)) : 0 };
    const int col { colParam.isValid() ? static_cast<int> (colParam.getProperty (Terminal::ID::value)) : 0 };

    auto bounds { screen.getCellBounds (col, row) };
    bounds.setWidth (bounds.getWidth() * cursor->getCellWidth());
    cursor->setBounds (bounds);
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
