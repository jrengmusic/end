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
#include "../terminal/notifications/Notifications.h"

/**
 * @brief Constructs Terminal::Component and wires all subsystems.
 *
 * Construction order:
 * 1. **Screen** — initialised with font family and base size from Config.
 * 2. **VBlankAttachment** — starts the render loop immediately.
 * 3. Screen settings (ligatures, embolden, theme) applied from Config.
 * 4. Saved zoom applied if > 1× (scales font size before first layout).
 * 5. **MessageOverlay** created and added as a hidden child.
 * 6. Session callbacks wired: `onShellExited`, `onClipboardChanged`, `onBell`, `onDesktopNotification`.
 * 7. Startup config error (if any) shown asynchronously via MessageOverlay.
 *
 * @note MESSAGE THREAD — called from MainComponent constructor.
 */
Terminal::Component::Component (jreng::Typeface& font_)
    : font (font_)
    , screen (std::in_place_type<Screen<jreng::Glyph::GraphicsContext>>, font_)
    , vblank (this,
              [this]
              {
                  onVBlank();
              })
{
    initialise();
    session.getState().get().setProperty (jreng::ID::id, juce::Uuid().toString(), nullptr);
}

std::unique_ptr<Terminal::Component> Terminal::Component::create (jreng::Typeface& font,
                                                                   const juce::String& workingDirectory)
{
    auto terminal { workingDirectory.isNotEmpty() ? std::make_unique<Component> (font, workingDirectory)
                                                  : std::make_unique<Component> (font) };
    const auto uuid { terminal->getValueTree().getProperty (jreng::ID::id).toString() };
    terminal->setComponentID (uuid);
    return terminal;
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
    , screen (std::in_place_type<Screen<jreng::Glyph::GraphicsContext>>, font_)
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
    , screen (std::in_place_type<Screen<jreng::Glyph::GraphicsContext>>, font_)
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
    setOpaque (false);
    setWantsKeyboardFocus (true);
    addKeyListener (this);

    // Construct InputHandler once — no Screen dependency, never reconstructed.
    inputHandler.emplace (session, linkManager);

    // Switch to the renderer stored in AppState (SSOT).
    // Emplaces correct Screen variant, handlers, opacity, and applies config.
    switchRenderer (getRendererType());
    inputHandler->buildKeyMap();

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

    session.onDesktopNotification = [] (const juce::String& title, const juce::String& body)
    {
        Terminal::Notifications::show (title, body);
    };

    linkManager.onOpenMarkdown = [this] (const juce::File& file)
    {
        if (onOpenMarkdown != nullptr)
            onOpenMarkdown (file);
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
    visitScreen ([&] (auto& s) { s.setSelection (nullptr); });
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
        visitScreen (
            [&] (auto& s)
            {
                s.setViewport (contentArea);
            });

        const int cols { screenBase().getNumCols() };
        const int rows { screenBase().getNumRows() };

        session.getState().setDimensions (cols, rows);
        session.resized (cols, rows);
        session.getState().setScrollOffset (0);
    }
}

bool Terminal::Component::hasSelection() const noexcept { return screenSelection != nullptr; }

void Terminal::Component::copySelection() noexcept
{
    if (screenSelection != nullptr)
    {
        const juce::ScopedLock lock (session.getGrid().getResizeLock());
        const int cols { session.getGrid().getCols() };
        const int scrollOffset { session.getState().getScrollOffset() };

        juce::String text;

        if (screenSelection->type == ScreenSelection::SelectionType::linear)
        {
            text = session.getGrid().extractText (screenSelection->anchor, screenSelection->end, scrollOffset);
        }
        else if (screenSelection->type == ScreenSelection::SelectionType::line)
        {
            const juce::Point<int> start { 0, std::min (screenSelection->anchor.y, screenSelection->end.y) };
            const juce::Point<int> end { cols - 1, std::max (screenSelection->anchor.y, screenSelection->end.y) };
            text = session.getGrid().extractText (start, end, scrollOffset);
        }
        else
        {
            const juce::Point<int> topLeft { std::min (screenSelection->anchor.x, screenSelection->end.x),
                                             std::min (screenSelection->anchor.y, screenSelection->end.y) };
            const juce::Point<int> bottomRight { std::max (screenSelection->anchor.x, screenSelection->end.x),
                                                 std::max (screenSelection->anchor.y, screenSelection->end.y) };
            text = session.getGrid().extractBoxText (topLeft, bottomRight, scrollOffset);
        }

        juce::SystemClipboard::copyTextToClipboard (text);

        session.getState().setDragActive (false);
        session.getState().setSelectionType (static_cast<int> (SelectionType::none));
        session.getState().setModalType (ModalType::none);
        screenSelection.reset();
        visitScreen (
            [&] (auto& s)
            {
                s.setSelection (nullptr);
            });
    }
}

void Terminal::Component::pasteClipboard() { session.paste (juce::SystemClipboard::getTextFromClipboard()); }

void Terminal::Component::writeToPty (const char* data, int len) { session.writeToPty (data, len); }

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
    if (screenSelection != nullptr and inputHandler->isSelectionCopyKey (key))
    {
        copySelection();
        return true;
    }

    return inputHandler->handleKey (key, onProcessExited != nullptr);
}

void Terminal::Component::enterSelectionMode() noexcept
{
    const juce::ScopedLock lock (session.getGrid().getResizeLock());
    const int screenCursorRow { session.getState().getCursorRow() };
    const int screenCursorCol { session.getState().getCursorCol() };
    const int scrollbackRows { session.getGrid().getScrollbackUsed() };
    const int absRow { scrollbackRows + screenCursorRow };

    session.getState().setSelectionCursor (absRow, screenCursorCol);
    session.getState().setSelectionAnchor (absRow, screenCursorCol);
    session.getState().setSelectionType (static_cast<int> (SelectionType::none));
    session.getState().setModalType (ModalType::selection);
    inputHandler->reset();
}

bool Terminal::Component::isInSelectionMode() const noexcept
{
    return session.getState().getModalType() == ModalType::selection;
}

int Terminal::Component::getSelectionType() const noexcept { return session.getState().getSelectionType(); }

Terminal::ModalType Terminal::Component::getModalType() const noexcept { return session.getState().getModalType(); }

int Terminal::Component::getHintPage() const noexcept { return session.getState().getHintPage(); }

int Terminal::Component::getHintTotalPages() const noexcept { return session.getState().getHintTotalPages(); }

void Terminal::Component::exitSelectionMode() noexcept
{
    session.getState().setSelectionType (static_cast<int> (SelectionType::none));
    session.getState().setModalType (ModalType::none);
    inputHandler->reset();
    session.getState().setDragActive (false);
    screenSelection.reset();
    visitScreen (
        [&] (auto& s)
        {
            s.setSelection (nullptr);
        });
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

            session.getState().setHintOverlay (linkManager.getActiveHintsData(), linkManager.getActiveHintsCount());
            session.getState().setModalType (ModalType::openFile);
        }
    }
}

void Terminal::Component::mouseWheelMove (const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    mouseHandler->handleWheel (event,
                               wheel,
                               [this] (int offset)
                               {
                                   setScrollOffsetClamped (offset);
                               });
}

void Terminal::Component::mouseMove (const juce::MouseEvent& event) { mouseHandler->handleMove (event, *this); }

void Terminal::Component::mouseDown (const juce::MouseEvent& event) { mouseHandler->handleDown (event); }

void Terminal::Component::mouseDoubleClick (const juce::MouseEvent& event) { mouseHandler->handleDoubleClick (event); }

void Terminal::Component::mouseDrag (const juce::MouseEvent& event) { mouseHandler->handleDrag (event); }

void Terminal::Component::mouseUp (const juce::MouseEvent& event) { mouseHandler->handleUp (event); }

/**
 * @brief Returns true — the terminal accepts any file drag.
 *
 * @param files  Array of absolute file paths being dragged.
 * @return Always @c true.
 * @note MESSAGE THREAD.
 */
bool Terminal::Component::isInterestedInFileDrag (const juce::StringArray&) { return true; }

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

        for (const auto& filePath : files)
        {
            const juce::File file { filePath };

            if (file.getFileExtension().toLowerCase() == ".md" and onOpenMarkdown != nullptr)
            {
                onOpenMarkdown (file);
            }
            else
            {
                if (shouldQuote
                    and (filePath.containsChar (' ') or filePath.containsChar ('\'') or filePath.containsChar ('"')
                         or filePath.containsChar ('\\') or filePath.containsChar ('(') or filePath.containsChar (')')))
                {
                    const juce::String shell { cfg->getString (Config::Key::shellProgram) };

                    if (shell.contains ("cmd"))
                        paths.add ("\"" + filePath + "\"");
                    else
                        paths.add ("'" + filePath.replace ("'", "'\\''") + "'");
                }
                else
                {
                    paths.add (filePath);
                }
            }
        }

        if (not paths.isEmpty())
            session.paste (paths.joinIntoString (separator));
    }
}

// GL THREAD
void Terminal::Component::glContextCreated() noexcept
{
    if (std::holds_alternative<Screen<jreng::Glyph::GLContext>> (screen))
    {
        // GL surface opacity only — AppKit blur is applied on the message thread
        // by MainComponent::valueTreePropertyChanged.
        jreng::BackgroundBlur::enableWindowTransparency();
        std::get<Screen<jreng::Glyph::GLContext>> (screen).glContextCreated();
        session.getGrid().markAllDirty();
        session.getState().setSnapshotDirty();
    }
}

// GL THREAD
void Terminal::Component::glContextClosing() noexcept
{
    if (std::holds_alternative<Screen<jreng::Glyph::GLContext>> (screen))
    {
        std::get<Screen<jreng::Glyph::GLContext>> (screen).glContextClosing();
    }
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
void Terminal::Component::paintGL() noexcept
{
    if (isVisible() and std::holds_alternative<Screen<jreng::Glyph::GLContext>> (screen))
    {
        auto& glScreen { std::get<Screen<jreng::Glyph::GLContext>> (screen) };

        if (not glScreen.isGLContextReady())
            glScreen.glContextCreated();

        const auto origin { getOriginInTopLevel() };
        glScreen.renderOpenGL (origin.x, origin.y, getFullViewportHeight());
    }
}

// MESSAGE THREAD
void Terminal::Component::paint (juce::Graphics& g)
{
    if (isVisible() and std::holds_alternative<Screen<jreng::Glyph::GraphicsContext>> (screen))
    {
        g.fillAll (findColour (juce::ResizableWindow::backgroundColourId));
        std::get<Screen<jreng::Glyph::GraphicsContext>> (screen).renderPaint (g, 0, 0, getHeight());
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
void Terminal::Component::focusGained (FocusChangeType cause)
{
    PaneComponent::focusGained (cause);
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
int Terminal::Component::getGridRows() const noexcept
{
    return std::visit (
        [] (const auto& s)
        {
            return s.getNumRows();
        },
        screen);
}

/** @return Current number of visible grid columns as reported by Screen. */
int Terminal::Component::getGridCols() const noexcept
{
    return std::visit (
        [] (const auto& s)
        {
            return s.getNumCols();
        },
        screen);
}

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
    if (getComponentID() == AppState::getContext()->getActivePaneID())
    {
        if (juce::Process::isForegroundProcess()
            and isShowing() and not hasKeyboardFocus (true)
            and juce::Component::getCurrentlyModalComponent() == nullptr)
        {
            toFront (true);
        }
    }

    if (session.getState().consumeSnapshotDirty())
    {
        const juce::ScopedTryLock lock (session.getGrid().getResizeLock());

        if (lock.isLocked())
        {
            session.getState().refresh();

            const int scrollback { session.getGrid().getScrollbackUsed() };
            const int scrollOffset { session.getState().getScrollOffset() };
            const int visibleStart { scrollback - scrollOffset };

            const bool isActivePane { AppState::getContext()->getActivePaneType() == "terminal" };

            // Compute visible-row coordinate for the selection cursor.
            // State stores absolute (scrollback-aware) rows;
            // the renderer needs visible rows (0 = topmost visible row).
            {
                const bool active { isActivePane and session.getState().getModalType() == ModalType::selection };
                const int visibleRow { session.getState().getSelectionCursorRow() - visibleStart };
                visitScreen (
                    [&] (auto& s)
                    {
                        s.setSelectionCursor (active, visibleRow, session.getState().getSelectionCursorCol());
                    });
            }

            // Build ScreenSelection from State — single SSOT for selection rendering.
            // Handles both keyboard (vim-style) and mouse selection paths.
            {
                const auto smType { static_cast<SelectionType> (session.getState().getSelectionType()) };

                if (isActivePane and smType != SelectionType::none)
                {
                    if (screenSelection == nullptr)
                        screenSelection = std::make_unique<ScreenSelection>();

                    const int anchorVisRow { session.getState().getSelectionAnchorRow() - visibleStart };
                    const int cursorVisRow { session.getState().getSelectionCursorRow() - visibleStart };

                    screenSelection->anchor = { session.getState().getSelectionAnchorCol(), anchorVisRow };
                    screenSelection->end = { session.getState().getSelectionCursorCol(), cursorVisRow };

                    if (smType == SelectionType::visual)
                        screenSelection->type = ScreenSelection::SelectionType::linear;
                    else if (smType == SelectionType::visualLine)
                        screenSelection->type = ScreenSelection::SelectionType::line;
                    else
                        screenSelection->type = ScreenSelection::SelectionType::box;

                    visitScreen (
                        [&] (auto& s)
                        {
                            s.setSelection (screenSelection.get());
                        });
                }
                else if (not session.getState().isDragActive())
                {
                    screenSelection.reset();
                    visitScreen (
                        [&] (auto& s)
                        {
                            s.setSelection (nullptr);
                        });
                }
            }

            // Link underlay: driven by LinkManager's ValueTree listener.
            {
                const auto& links { linkManager.getClickableLinks() };
                visitScreen (
                    [&] (auto& s)
                    {
                        s.setLinkUnderlay (links.data(), static_cast<int> (links.size()));
                    });
            }

            visitScreen (
                [&] (auto& s)
                {
                    s.render (session.getState(), session.getGrid());
                });

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
void Terminal::Component::applyScreenSettings() noexcept
{
    auto* cfg { Config::getContext() };
    visitScreen (
        [&] (auto& s)
        {
            s.setLigatures (cfg->getBool (Config::Key::fontLigatures));
            s.setEmbolden (cfg->getBool (Config::Key::fontEmbolden));
            s.setTheme (cfg->buildTheme());
            s.setFontSize (dpiCorrectedFontSize() * AppState::getContext()->getWindowZoom());
        });
}

void Terminal::Component::applyConfig() noexcept
{
    applyScreenSettings();
    inputHandler->buildKeyMap();
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
    visitScreen (
        [&] (auto& s)
        {
            s.setFontSize (dpiCorrectedFontSize() * zoom);
        });
    resized();
}

void Terminal::Component::switchRenderer (PaneComponent::RendererType type)
{
    // Release current renderer's GL resources before switching.
    // The contextInitialised guard in each renderer prevents
    // double-close or close-before-create.
    visitScreen (
        [&] (auto& s)
        {
            s.glContextClosing();
        });

    // Atlas resize is handled once by MainComponent's ValueTree listener.
    if (type == PaneComponent::RendererType::cpu)
    {
        if (not std::holds_alternative<Screen<jreng::Glyph::GraphicsContext>> (screen))
        {
            screen.emplace<Screen<jreng::Glyph::GraphicsContext>> (font);
        }
    }
    else
    {
        if (not std::holds_alternative<Screen<jreng::Glyph::GLContext>> (screen))
        {
            screen.emplace<Screen<jreng::Glyph::GLContext>> (font);
        }
    }

    mouseHandler.emplace (session, screenBase(), linkManager);

    applyScreenSettings();

    session.getGrid().markAllDirty();
    session.getState().setSnapshotDirty();

    if (getWidth() > 0 and getHeight() > 0)
    {
        resized();
    }
}

void Terminal::Component::setScrollOffsetClamped (int newOffset) noexcept
{
    const juce::ScopedLock lock (session.getGrid().getResizeLock());
    const int maxOffset { session.getGrid().getScrollbackUsed() };
    const int current { session.getState().getScrollOffset() };
    const int clamped { juce::jlimit (0, maxOffset, newOffset) };

    if (clamped != current)
    {
        session.getState().setScrollOffset (clamped);
    }
}
