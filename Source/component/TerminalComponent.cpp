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
Terminal::Component::Component (jreng::Typeface& font_)
    : font (font_)
    , screen (font_)
    , vblank (this,
              [this]
              {
                  onVBlank();
              })
{
    initialise();
    session.getState().get().setProperty (jreng::ID::id, juce::Uuid().toString(), nullptr);
}

Terminal::Component* Terminal::Component::create (jreng::Typeface& font,
                                                  juce::Component& parent,
                                                  juce::Rectangle<int> bounds,
                                                  jreng::Owner<Component>& owner,
                                                  const juce::String& workingDirectory)
{
    auto terminal { workingDirectory.isNotEmpty() ? std::make_unique<Component> (font, workingDirectory)
                                                  : std::make_unique<Component> (font) };
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
 * @param font_             Font instance providing metrics, shaping, and rasterisation.
 * @param workingDirectory  Absolute path for the shell's initial cwd.
 * @note MESSAGE THREAD.
 */
Terminal::Component::Component (jreng::Typeface& font_, const juce::String& workingDirectory)
    : font (font_)
    , screen (font_)
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
 * @param font_             Font instance providing metrics, shaping, and rasterisation.
 * @param program           Shell command or executable path.
 * @param args              Arguments passed to the command.
 * @param workingDirectory  Initial cwd. Empty = inherit.
 * @note MESSAGE THREAD.
 */
Terminal::Component::Component (jreng::Typeface& font_,
                                const juce::String& program,
                                const juce::String& args,
                                const juce::String& workingDirectory)
    : font (font_)
    , screen (font_)
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
    inputHandler.buildKeyMap();

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
                if (onShellExited != nullptr)
                    onShellExited();

                if (onProcessExited != nullptr)
                    onProcessExited();
                else if (onShellExited == nullptr)
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
    contentArea.removeFromTop (titleBarHeight);
    contentArea.removeFromTop (paddingTop);
    contentArea.removeFromRight (paddingRight);
    contentArea.removeFromBottom (paddingBottom);
    contentArea.removeFromLeft (paddingLeft);

    if (contentArea.getWidth() > 0 and contentArea.getHeight() > 0)
    {
        screen.setViewport (contentArea);
        session.resized (screen.getNumCols(), screen.getNumRows());
        session.getState().setScrollOffset (0);
    }
}

bool Terminal::Component::hasSelection() const noexcept
{
    return screenSelection != nullptr;
}

void Terminal::Component::copySelection()
{
    if (screenSelection != nullptr)
    {
        const juce::ScopedLock lock (session.getGrid().getResizeLock());
        const int cols { session.getGrid().getCols() };

        juce::String text;

        if (screenSelection->type == ScreenSelection::SelectionType::linear)
        {
            text = session.getGrid().extractText (screenSelection->anchor, screenSelection->end);
        }
        else if (screenSelection->type == ScreenSelection::SelectionType::line)
        {
            const juce::Point<int> start { 0, std::min (screenSelection->anchor.y, screenSelection->end.y) };
            const juce::Point<int> end { cols - 1, std::max (screenSelection->anchor.y, screenSelection->end.y) };
            text = session.getGrid().extractText (start, end);
        }
        else
        {
            const juce::Point<int> topLeft {
                std::min (screenSelection->anchor.x, screenSelection->end.x),
                std::min (screenSelection->anchor.y, screenSelection->end.y)
            };
            const juce::Point<int> bottomRight {
                std::max (screenSelection->anchor.x, screenSelection->end.x),
                std::max (screenSelection->anchor.y, screenSelection->end.y)
            };
            text = session.getGrid().extractBoxText (topLeft, bottomRight);
        }

        juce::SystemClipboard::copyTextToClipboard (text);

        session.getState().setDragActive (false);
        screenSelection.reset();
        screen.setSelection (nullptr);
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

bool Terminal::Component::keyPressed (const juce::KeyPress& key, juce::Component*)
{
    return inputHandler.handleKey (key, onProcessExited != nullptr);
}

void Terminal::Component::enterSelectionMode() noexcept
{
    const auto activeScreen { session.getState().getActiveScreen() };
    const int screenCursorRow { session.getState().getCursorRow (activeScreen) };
    const int screenCursorCol { session.getState().getCursorCol (activeScreen) };
    const int scrollbackRows { session.getGrid().getScrollbackUsed() };
    const int absRow { scrollbackRows + screenCursorRow };

    session.getState().setSelectionCursor (absRow, screenCursorCol);
    session.getState().setSelectionAnchor (absRow, screenCursorCol);
    session.getState().setSelectionType (static_cast<int> (SelectionType::none));
    session.getState().setModalType (ModalType::selection);
    inputHandler.reset();
}

bool Terminal::Component::isInSelectionMode() const noexcept
{
    return session.getState().getModalType() == ModalType::selection;
}

int Terminal::Component::getSelectionType() const noexcept
{
    return session.getState().getSelectionType();
}

Terminal::ModalType Terminal::Component::getModalType() const noexcept
{
    return session.getState().getModalType();
}

void Terminal::Component::exitSelectionMode() noexcept
{
    session.getState().setSelectionType (static_cast<int> (SelectionType::none));
    session.getState().setModalType (ModalType::none);
    inputHandler.reset();
    session.getState().setDragActive (false);
    screenSelection.reset();
    screen.setSelection (nullptr);
}

void Terminal::Component::enterOpenFileMode() noexcept
{
    if (session.getState().hasOutputBlock())
    {
        const juce::ScopedTryLock stl { session.getGrid().getResizeLock() };

        if (stl.isLocked())
        {
            const juce::String cwd { session.getState().get().getProperty (Terminal::ID::cwd).toString() };
            linkManager.scanForHints (cwd);

            const auto& hints { linkManager.getHintLinks() };
            screen.setHintOverlay (hints.data(), static_cast<int> (hints.size()));
            session.getState().setModalType (ModalType::openFile);
        }
    }
}


void Terminal::Component::mouseWheelMove (const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    mouseHandler.handleWheel (event, wheel, [this] (int offset)
    {
        setScrollOffsetClamped (offset);
    });
}

void Terminal::Component::mouseMove (const juce::MouseEvent& event)
{
    mouseHandler.handleMove (event, *this);
}

void Terminal::Component::mouseDown (const juce::MouseEvent& event)
{
    mouseHandler.handleDown (event);
}

void Terminal::Component::mouseDoubleClick (const juce::MouseEvent& event)
{
    mouseHandler.handleDoubleClick (event);
}

void Terminal::Component::mouseDrag (const juce::MouseEvent& event)
{
    mouseHandler.handleDrag (event);
}

void Terminal::Component::mouseUp (const juce::MouseEvent& event)
{
    mouseHandler.handleUp (event);
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
    if (not files.isEmpty())
    {
        const auto* cfg { Config::getContext() };
        const juce::String multifiles { cfg->getString (Config::Key::terminalDropMultifiles) };
        const bool shouldQuote { cfg->getBool (Config::Key::terminalDropQuoted) };
        const juce::String separator { multifiles == "newline" ? "\n" : " " };

        juce::StringArray paths;

        for (const auto& file : files)
        {
            if (shouldQuote and (file.containsChar (' ') or file.containsChar ('\'') or file.containsChar ('"') or file.containsChar ('\\') or file.containsChar ('(') or file.containsChar (')')))
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
}

// GL THREAD
void Terminal::Component::glContextCreated() noexcept
{
    // [Plan 3.4 — Graphics-only test: GL disabled]
    // jreng::BackgroundBlur::enableGLTransparency();
    // screen.glContextCreated();
    // session.getGrid().markAllDirty();
    // session.getState().setSnapshotDirty();
}

// GL THREAD
void Terminal::Component::glContextClosing() noexcept
{
    // [Plan 3.4 — Graphics-only test: GL disabled]
    // screen.glContextClosing();
}

juce::Point<int> Terminal::Component::getOriginInTopLevel() const noexcept
{
    const float scale { jreng::Typeface::getDisplayScale() };
    const auto* topLevel { getTopLevelComponent() };
    const auto relative { topLevel != nullptr ? topLevel->getLocalPoint (this, juce::Point<int> (0, 0))
                                              : juce::Point<int> (0, 0) };

    return { static_cast<int> (static_cast<float> (relative.x) * scale),
             static_cast<int> (static_cast<float> (relative.y) * scale) };
}

// GL THREAD
void Terminal::Component::renderGL() noexcept
{
    // [Plan 3.4 — Graphics-only test: GL disabled]
    // if (isVisible())
    // {
    //     if (not screen.isGLContextReady())
    //         screen.glContextCreated();
    //     const auto origin { getOriginInTopLevel() };
    //     screen.renderOpenGL (origin.x, origin.y, getFullViewportHeight());
    // }
}

// MESSAGE THREAD
void Terminal::Component::paint (juce::Graphics& g)
{
    if (isVisible())
    {
        screen.renderPaint (g, 0, 0, getHeight());
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
            const int scrollback { session.getGrid().getScrollbackUsed() };
            const int scrollOffset { session.getState().getScrollOffset() };
            const int visibleStart { scrollback - scrollOffset };

            // Compute visible-row coordinate for the selection cursor.
            // State stores absolute (scrollback-aware) rows;
            // the renderer needs visible rows (0 = topmost visible row).
            {
                const bool active { session.getState().getModalType() == ModalType::selection };
                const int visibleRow { session.getState().getSelectionCursorRow() - visibleStart };
                screen.setSelectionCursor (active, visibleRow, session.getState().getSelectionCursorCol());
            }

            // Build ScreenSelection from State — single SSOT for selection rendering.
            // Handles both keyboard (vim-style) and mouse selection paths.
            {
                const auto smType { static_cast<SelectionType> (session.getState().getSelectionType()) };

                if (smType != SelectionType::none)
                {
                    if (screenSelection == nullptr)
                        screenSelection = std::make_unique<ScreenSelection>();

                    const int anchorVisRow { session.getState().getSelectionAnchorRow() - visibleStart };
                    const int cursorVisRow { session.getState().getSelectionCursorRow() - visibleStart };

                    screenSelection->anchor = { session.getState().getSelectionAnchorCol(), anchorVisRow };
                    screenSelection->end    = { session.getState().getSelectionCursorCol(), cursorVisRow };

                    if (smType == SelectionType::visual)
                        screenSelection->type = ScreenSelection::SelectionType::linear;
                    else if (smType == SelectionType::visualLine)
                        screenSelection->type = ScreenSelection::SelectionType::line;
                    else
                        screenSelection->type = ScreenSelection::SelectionType::box;

                    screen.setSelection (screenSelection.get());
                }
                else if (not session.getState().isDragActive())
                {
                    screenSelection.reset();
                    screen.setSelection (nullptr);
                }
            }

            // Link underlay: driven by LinkManager's ValueTree listener.
            {
                const auto& links { linkManager.getClickableLinks() };
                screen.setLinkUnderlay (links.data(), static_cast<int> (links.size()));
            }

            screen.render (session.getState(), session.getGrid());

            if (onRepaintNeeded != nullptr)
                onRepaintNeeded();
        }
        else
        {
            session.getState().setSnapshotDirty();
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
    inputHandler.buildKeyMap();
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


void Terminal::Component::setScrollOffsetClamped (int newOffset) noexcept
{
    const int maxOffset { session.getGrid().getScrollbackUsed() };
    const int current { session.getState().getScrollOffset() };
    const int clamped { juce::jlimit (0, maxOffset, newOffset) };

    if (clamped != current)
    {
        session.getState().setScrollOffset (clamped);
    }
}
