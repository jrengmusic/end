/**
 * @file TerminalDisplay.cpp
 * @brief Implementation of the Terminal::Display ephemeral UI component.
 *
 * Wires the terminal backend (Processor, Grid, Screen) to JUCE input events and
 * the VBlank render loop.  See TerminalDisplay.h for the full architectural
 * overview.
 *
 * @see Terminal::Display
 * @see Terminal::Processor
 * @see Terminal::Screen
 */

#include "TerminalDisplay.h"
#include "../AppState.h"
#include "../config/Config.h"
#include "../terminal/notifications/Notifications.h"
#include "../nexus/Client.h"
#include "../nexus/Session.h"
#include "../nexus/Log.h"

/**
 * @brief Constructs Terminal::Display for the given Processor.
 *
 * Construction order:
 * 1. **processor** — stored by reference.
 * 2. **Screen** — initialised with font family and base size from Config.
 * 3. **VBlankAttachment** — starts the render loop immediately.
 * 4. `initialise()` — wires Processor callbacks, applies screen settings.
 *
 * @note MESSAGE THREAD — called from Processor::createDisplay().
 */
Terminal::Display::Display (Terminal::Processor& proc, jreng::Typeface& font_)
    : processor (proc)
    , font (font_)
    , screen (std::in_place_type<Screen<jreng::Glyph::GraphicsContext>>, font_)
    , vblank (this,
              [this]
              {
                  onVBlank();
              })
{
    processor.addChangeListener (this);
    initialise();
}

/**
 * @brief Unwires all Processor callbacks, unsubscribes from ChangeBroadcaster,
 *        and tears down the Screen renderer.
 *
 * @note MESSAGE THREAD.
 */
Terminal::Display::~Display()
{
    processor.removeChangeListener (this);

    processor.onShellExited = nullptr;
    processor.onClipboardChanged = nullptr;
    processor.onBell = nullptr;
    processor.onDesktopNotification = nullptr;

    visitScreen ([&] (auto& scr) { scr.setSelection (nullptr); });
    screenSelection.reset();
    removeKeyListener (this);
}

/**
 * @brief Called by Processor (ChangeBroadcaster) when state has been updated.
 *
 * Marks the snapshot dirty and requests a repaint.
 *
 * @note MESSAGE THREAD.
 */
void Terminal::Display::changeListenerCallback (juce::ChangeBroadcaster* /*source*/)
{
    const bool callbackSet { onRepaintNeeded != nullptr };
    Nexus::logLine ("Display::changeListenerCallback: fired for uuid=" + processor.getUuid()
                    + " onRepaintNeeded set=" + juce::String (callbackSet ? "yes" : "no"));
    processor.getState().setSnapshotDirty();

    if (onRepaintNeeded != nullptr)
        onRepaintNeeded();
}

// =============================================================================

/**
 * @brief Initialises the terminal display after construction.
 *
 * Contains the shared initialization logic for the constructor.
 * Sets up screen settings, processor callbacks, and message overlay.
 *
 * @note MESSAGE THREAD.
 */
void Terminal::Display::initialise()
{
    setOpaque (false);
    setWantsKeyboardFocus (true);
    addKeyListener (this);

    // Construct LinkManager — requires Processor state and grid references.
    // Routes PTY writes through Display::writeToPty → Nexus::Session::sendInput (mode-agnostic).
    linkManager.emplace (processor.getState(),
                         processor.getGrid(),
                         [this] (const char* data, int len) { writeToPty (data, len); });

    // Construct Terminal::Input once — no Screen dependency, never reconstructed.
    inputHandler.emplace (processor, *linkManager);

    // Switch to the renderer stored in AppState (SSOT).
    switchRenderer (AppState::getContext()->getRendererType());
    inputHandler->buildKeyMap();

    processor.onShellExited = [this]
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

    processor.onClipboardChanged = [] (const juce::String& text)
    {
        juce::SystemClipboard::copyTextToClipboard (text);
    };

    processor.onBell = []
    {
        std::fwrite ("\a", 1, 1, stderr);
    };

    processor.onDesktopNotification = [] (const juce::String& title, const juce::String& body)
    {
        Terminal::Notifications::show (title, body);
    };

    linkManager->onOpenMarkdown = [this] (const juce::File& file)
    {
        if (onOpenMarkdown != nullptr)
            onOpenMarkdown (file);
    };
}

/**
 * @brief Lays out the screen viewport, notifies Processor of new grid size.
 *
 * @note MESSAGE THREAD — called by JUCE on every resize event.
 */
void Terminal::Display::resized()
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

        processor.getState().setDimensions (cols, rows);
        processor.resized (cols, rows);
        Nexus::Session::getContext()->sendResize (processor.getUuid(), cols, rows);
        processor.getState().setScrollOffset (0);
    }
}

bool Terminal::Display::hasSelection() const noexcept { return screenSelection != nullptr; }

void Terminal::Display::copySelection() noexcept
{
    if (screenSelection != nullptr)
    {
        const juce::ScopedLock lock (processor.getGrid().getResizeLock());
        const int cols { processor.getGrid().getCols() };
        const int scrollOffset { processor.getState().getScrollOffset() };

        juce::String text;

        if (screenSelection->type == ScreenSelection::SelectionType::linear)
        {
            text = processor.getGrid().extractText (screenSelection->anchor, screenSelection->end, scrollOffset);
        }
        else if (screenSelection->type == ScreenSelection::SelectionType::line)
        {
            const juce::Point<int> start { 0, std::min (screenSelection->anchor.y, screenSelection->end.y) };
            const juce::Point<int> end { cols - 1, std::max (screenSelection->anchor.y, screenSelection->end.y) };
            text = processor.getGrid().extractText (start, end, scrollOffset);
        }
        else
        {
            const juce::Point<int> topLeft { std::min (screenSelection->anchor.x, screenSelection->end.x),
                                             std::min (screenSelection->anchor.y, screenSelection->end.y) };
            const juce::Point<int> bottomRight { std::max (screenSelection->anchor.x, screenSelection->end.x),
                                                 std::max (screenSelection->anchor.y, screenSelection->end.y) };
            text = processor.getGrid().extractBoxText (topLeft, bottomRight, scrollOffset);
        }

        juce::SystemClipboard::copyTextToClipboard (text);

        processor.getState().setDragActive (false);
        processor.getState().setSelectionType (static_cast<int> (SelectionType::none));
        processor.getState().setModalType (ModalType::none);
        screenSelection.reset();
        visitScreen (
            [&] (auto& s)
            {
                s.setSelection (nullptr);
            });
    }
}

void Terminal::Display::pasteClipboard()
{
    const juce::String text { juce::SystemClipboard::getTextFromClipboard() };

    if (text.isNotEmpty())
    {
        const auto bytes { processor.encodePaste (text) };

        if (bytes.isNotEmpty())
        {
            const bool bracketed { processor.getState().getMode (Terminal::ID::bracketedPaste) };

            if (not bracketed)
                processor.getState().setPasteEchoGate (static_cast<int> (bytes.getNumBytesAsUTF8()));

            Nexus::Session::getContext()->sendInput (processor.getUuid(), bytes.toRawUTF8(), static_cast<int> (bytes.getNumBytesAsUTF8()));
        }
    }
}

void Terminal::Display::writeToPty (const char* data, int len)
{
    Nexus::Session::getContext()->sendInput (processor.getUuid(), data, len);
}

void Terminal::Display::increaseZoom()
{
    const float current { AppState::getContext()->getWindowZoom() };
    const float newZoom { juce::jlimit (Config::zoomMin, Config::zoomMax, current + 0.25f) };
    AppState::getContext()->setWindowZoom (newZoom);
    applyZoom (newZoom);
}

void Terminal::Display::decreaseZoom()
{
    const float current { AppState::getContext()->getWindowZoom() };
    const float newZoom { juce::jlimit (Config::zoomMin, Config::zoomMax, current - 0.25f) };
    AppState::getContext()->setWindowZoom (newZoom);
    applyZoom (newZoom);
}

void Terminal::Display::resetZoom()
{
    AppState::getContext()->setWindowZoom (Config::zoomMin);
    applyZoom (Config::zoomMin);
}

bool Terminal::Display::keyPressed (const juce::KeyPress& key, juce::Component*)
{
    const bool isCopy { inputHandler.has_value()
                        and screenSelection != nullptr
                        and inputHandler->isSelectionCopyKey (key) };

    if (isCopy)
        copySelection();

    const bool dispatched { inputHandler.has_value() and not isCopy
                                and (onProcessExited != nullptr
                                         ? inputHandler->handleKeyDirect (key)
                                         : inputHandler->handleKey (key)) };

    return isCopy or dispatched;
}

void Terminal::Display::enterSelectionMode() noexcept
{
    const juce::ScopedLock lock (processor.getGrid().getResizeLock());
    const int screenCursorRow { processor.getState().getCursorRow() };
    const int screenCursorCol { processor.getState().getCursorCol() };
    const int scrollbackRows { processor.getGrid().getScrollbackUsed() };
    const int absRow { scrollbackRows + screenCursorRow };

    processor.getState().setSelectionCursor (absRow, screenCursorCol);
    processor.getState().setSelectionAnchor (absRow, screenCursorCol);
    processor.getState().setSelectionType (static_cast<int> (SelectionType::none));
    processor.getState().setModalType (ModalType::selection);

    if (inputHandler.has_value())
        inputHandler->reset();
}

bool Terminal::Display::isInSelectionMode() const noexcept
{
    return processor.getState().getModalType() == ModalType::selection;
}

int Terminal::Display::getSelectionType() const noexcept { return processor.getState().getSelectionType(); }

Terminal::ModalType Terminal::Display::getModalType() const noexcept { return processor.getState().getModalType(); }

int Terminal::Display::getHintPage() const noexcept { return processor.getState().getHintPage(); }

int Terminal::Display::getHintTotalPages() const noexcept { return processor.getState().getHintTotalPages(); }

void Terminal::Display::exitSelectionMode() noexcept
{
    processor.getState().setSelectionType (static_cast<int> (SelectionType::none));
    processor.getState().setModalType (ModalType::none);

    if (inputHandler.has_value())
        inputHandler->reset();

    processor.getState().setDragActive (false);
    screenSelection.reset();
    visitScreen (
        [&] (auto& s)
        {
            s.setSelection (nullptr);
        });
}

void Terminal::Display::enterOpenFileMode() noexcept
{
    if (processor.getState().hasOutputBlock())
    {
        const juce::ScopedTryLock stl { processor.getGrid().getResizeLock() };

        if (stl.isLocked() and linkManager.has_value())
        {
            const juce::String cwd { processor.getState().get().getProperty (Terminal::ID::cwd).toString() };
            linkManager->scanForHints (cwd);

            processor.getState().setHintOverlay (linkManager->getActiveHintsData(), linkManager->getActiveHintsCount());
            processor.getState().setModalType (ModalType::openFile);
        }
    }
}

void Terminal::Display::mouseWheelMove (const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    if (mouseHandler.has_value())
    {
        mouseHandler->handleWheel (event,
                                   wheel,
                                   [this] (int offset)
                                   {
                                       setScrollOffsetClamped (offset);
                                   });
    }
}

void Terminal::Display::mouseMove (const juce::MouseEvent& event)
{
    if (mouseHandler.has_value())
        mouseHandler->handleMove (event, *this);
}

void Terminal::Display::mouseDown (const juce::MouseEvent& event)
{
    if (mouseHandler.has_value())
        mouseHandler->handleDown (event);
}

void Terminal::Display::mouseDoubleClick (const juce::MouseEvent& event)
{
    if (mouseHandler.has_value())
        mouseHandler->handleDoubleClick (event);
}

void Terminal::Display::mouseDrag (const juce::MouseEvent& event)
{
    if (mouseHandler.has_value())
        mouseHandler->handleDrag (event);
}

void Terminal::Display::mouseUp (const juce::MouseEvent& event)
{
    if (mouseHandler.has_value())
        mouseHandler->handleUp (event);
}

/**
 * @brief Returns true — the terminal accepts any file drag.
 */
bool Terminal::Display::isInterestedInFileDrag (const juce::StringArray&) { return true; }

/**
 * @brief Pastes dropped file paths as text into the PTY.
 */
void Terminal::Display::filesDropped (const juce::StringArray& files, int, int)
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
                    const juce::String shellProg { cfg->getString (Config::Key::shellProgram) };

                    if (shellProg.contains ("cmd"))
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
        {
            const juce::String joined { paths.joinIntoString (separator) };
            const auto bytes { processor.encodePaste (joined) };

            if (bytes.isNotEmpty())
                Nexus::Session::getContext()->sendInput (processor.getUuid(), bytes.toRawUTF8(), static_cast<int> (bytes.getNumBytesAsUTF8()));
        }
    }
}

// GL THREAD
void Terminal::Display::glContextCreated() noexcept
{
    if (std::holds_alternative<Screen<jreng::Glyph::GLContext>> (screen))
    {
        jreng::BackgroundBlur::enableWindowTransparency();
        std::get<Screen<jreng::Glyph::GLContext>> (screen).glContextCreated();
        processor.getGrid().markAllDirty();
        processor.getState().setSnapshotDirty();
    }
}

// GL THREAD
void Terminal::Display::glContextClosing() noexcept
{
    if (std::holds_alternative<Screen<jreng::Glyph::GLContext>> (screen))
    {
        std::get<Screen<jreng::Glyph::GLContext>> (screen).glContextClosing();
    }
}

juce::Point<int> Terminal::Display::getOriginInTopLevel() const noexcept
{
    const float scale { jreng::Typeface::getDisplayScale() };
    const auto* topLevel { getTopLevelComponent() };
    const auto relative { topLevel != nullptr ? topLevel->getLocalPoint (this, juce::Point<int> (0, 0))
                                              : juce::Point<int> (0, 0) };

    return { static_cast<int> (static_cast<float> (relative.x) * scale),
             static_cast<int> (static_cast<float> (relative.y) * scale) };
}

// GL THREAD
void Terminal::Display::paintGL() noexcept
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
void Terminal::Display::paint (juce::Graphics& g)
{
    if (isVisible() and std::holds_alternative<Screen<jreng::Glyph::GraphicsContext>> (screen))
    {
        g.fillAll (findColour (juce::ResizableWindow::backgroundColourId));
        std::get<Screen<jreng::Glyph::GraphicsContext>> (screen).renderPaint (g, 0, 0, getHeight());
    }
}

// MESSAGE THREAD
void Terminal::Display::visibilityChanged()
{
    if (isVisible())
    {
        processor.getGrid().markAllDirty();
        processor.getState().setSnapshotDirty();
    }
}

/**
 * @brief Notifies the shell of focus gain and triggers a repaint.
 *
 * @note MESSAGE THREAD.
 */
void Terminal::Display::focusGained (FocusChangeType cause)
{
    PaneComponent::focusGained (cause);
    processor.getState().setCursorFocused (true);
    const auto focusBytes { processor.encodeFocusEvent (true) };

    if (focusBytes.isNotEmpty())
        Nexus::Session::getContext()->sendInput (processor.getUuid(), focusBytes.toRawUTF8(), static_cast<int> (focusBytes.getNumBytesAsUTF8()));

    repaint();
}

/**
 * @brief Notifies the shell of focus loss and triggers a repaint.
 *
 * @note MESSAGE THREAD.
 */
void Terminal::Display::focusLost (FocusChangeType)
{
    processor.getState().setCursorFocused (false);
    const auto focusBytes { processor.encodeFocusEvent (false) };

    if (focusBytes.isNotEmpty())
        Nexus::Session::getContext()->sendInput (processor.getUuid(), focusBytes.toRawUTF8(), static_cast<int> (focusBytes.getNumBytesAsUTF8()));

    repaint();
}

/** @return Current number of visible grid rows as reported by Screen. */
int Terminal::Display::getGridRows() const noexcept
{
    return std::visit (
        [] (const auto& s)
        {
            return s.getNumRows();
        },
        screen);
}

/** @return Current number of visible grid columns as reported by Screen. */
int Terminal::Display::getGridCols() const noexcept
{
    return std::visit (
        [] (const auto& s)
        {
            return s.getNumCols();
        },
        screen);
}

juce::ValueTree Terminal::Display::getValueTree() noexcept { return processor.getState().get(); }

/**
 * @brief VBlank callback: renders the grid if dirty, repositions cursor, updates visibility.
 *
 * @note MESSAGE THREAD — VBlankAttachment callbacks run on the message thread.
 */
void Terminal::Display::onVBlank()
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

    if (processor.getState().consumeSnapshotDirty())
    {
        const juce::ScopedTryLock lock (processor.getGrid().getResizeLock());

        if (lock.isLocked())
        {
            processor.getState().refresh();

            const int scrollback { processor.getGrid().getScrollbackUsed() };
            const int scrollOffset { processor.getState().getScrollOffset() };
            const int visibleStart { scrollback - scrollOffset };

            const bool isActivePane { AppState::getContext()->getActivePaneType() == App::ID::paneTypeTerminal };

            {
                const bool active { isActivePane and processor.getState().getModalType() == ModalType::selection };
                const int visibleRow { processor.getState().getSelectionCursorRow() - visibleStart };
                visitScreen (
                    [&] (auto& s)
                    {
                        s.setSelectionCursor (active, visibleRow, processor.getState().getSelectionCursorCol());
                    });
            }

            {
                const auto smType { static_cast<SelectionType> (processor.getState().getSelectionType()) };

                if (isActivePane and smType != SelectionType::none)
                {
                    if (screenSelection == nullptr)
                        screenSelection = std::make_unique<ScreenSelection>();

                    const int anchorVisRow { processor.getState().getSelectionAnchorRow() - visibleStart };
                    const int cursorVisRow { processor.getState().getSelectionCursorRow() - visibleStart };

                    screenSelection->anchor = { processor.getState().getSelectionAnchorCol(), anchorVisRow };
                    screenSelection->end = { processor.getState().getSelectionCursorCol(), cursorVisRow };

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
                else if (not processor.getState().isDragActive())
                {
                    screenSelection.reset();
                    visitScreen (
                        [&] (auto& s)
                        {
                            s.setSelection (nullptr);
                        });
                }
            }

            {
                if (linkManager.has_value())
                {
                    const auto& links { linkManager->getClickableLinks() };
                    visitScreen (
                        [&] (auto& s)
                        {
                            s.setLinkUnderlay (links.data(), static_cast<int> (links.size()));
                        });
                }
            }

            visitScreen (
                [&] (auto& s)
                {
                    s.render (processor.getState(), processor.getGrid());
                });

            if (onRepaintNeeded != nullptr)
                onRepaintNeeded();
        }
        else
        {
            processor.getState().setSnapshotDirty();
        }
    }
}

/**
 * @brief Applies current config settings to the active screen.
 *
 * @note MESSAGE THREAD.
 */
void Terminal::Display::applyScreenSettings() noexcept
{
    auto* cfg { Config::getContext() };
    visitScreen (
        [&] (auto& s)
        {
            s.setLigatures (cfg->getBool (Config::Key::fontLigatures));
            s.setEmbolden (cfg->getBool (Config::Key::fontEmbolden));
            s.setLineHeight (cfg->getFloat (Config::Key::fontLineHeight));
            s.setCellWidth (cfg->getFloat (Config::Key::fontCellWidth));
            s.setTheme (cfg->buildTheme());
            s.setFontSize (dpiCorrectedFontSize() * AppState::getContext()->getWindowZoom());
        });
}

void Terminal::Display::applyConfig() noexcept
{
    applyScreenSettings();

    if (inputHandler.has_value())
        inputHandler->buildKeyMap();

    processor.getGrid().markAllDirty();
    processor.getState().setSnapshotDirty();
    resized();
}

/**
 * @brief Scales the font size by @p zoom and re-lays out the component.
 *
 * @note MESSAGE THREAD.
 */
void Terminal::Display::applyZoom (float zoom) noexcept
{
    visitScreen (
        [&] (auto& s)
        {
            s.setFontSize (dpiCorrectedFontSize() * zoom);
        });
    resized();
}

void Terminal::Display::switchRenderer (App::RendererType type)
{
    visitScreen (
        [&] (auto& s)
        {
            s.glContextClosing();
        });

    if (type == App::RendererType::cpu)
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

    if (linkManager.has_value())
        mouseHandler.emplace (processor, screenBase(), *linkManager);

    applyScreenSettings();

    processor.getGrid().markAllDirty();
    processor.getState().setSnapshotDirty();

    if (getWidth() > 0 and getHeight() > 0)
    {
        resized();
    }
}

void Terminal::Display::setScrollOffsetClamped (int newOffset) noexcept
{
    const juce::ScopedLock lock (processor.getGrid().getResizeLock());
    const int maxOffset { processor.getGrid().getScrollbackUsed() };
    const int current { processor.getState().getScrollOffset() };
    const int clamped { juce::jlimit (0, maxOffset, newOffset) };

    if (clamped != current)
    {
        processor.getState().setScrollOffset (clamped);
    }
}
