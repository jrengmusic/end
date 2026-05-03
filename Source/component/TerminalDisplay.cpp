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

/**
 * @brief Constructs Terminal::Display for the given Processor.
 *
 * Construction order:
 * 1. **processor** — stored by reference.
 * 2. **Screen** — initialised with font family and base size from Config.
 * 3. **VBlankAttachment** — starts the render loop immediately.
 * 4. `initialise()` — wires Processor callbacks, applies screen settings.
 * 5. State ValueTree listener — registered so `valueTreePropertyChanged` fires on flush.
 *
 * @note MESSAGE THREAD — called from Processor::createDisplay().
 */
Terminal::Display::Display (Terminal::Processor& p,
                            jam::Font& fontToUse,
                            jam::Glyph::Packer& packerToUse,
                            jam::gl::GlyphAtlas& glAtlasToUse,
                            jam::GraphicsAtlas& graphicsAtlasToUse)
    : processor (p)
    , font (fontToUse)
    , packerRef (packerToUse)
    , glAtlasRef (glAtlasToUse)
    , graphicsAtlasRef (graphicsAtlasToUse)
    , config (*lua::Engine::getContext())
    , screen (std::in_place_type<Screen<jam::Glyph::GraphicsContext>>, fontToUse, packerToUse, graphicsAtlasToUse)
    , vblank (this,
              [this]
              {
                  onVBlank();
              })
{
    processor.getState().get().addListener (this);
    initialise();
}

/**
 * @brief Unwires all Processor callbacks, unsubscribes from State ValueTree,
 *        and tears down the Screen renderer.
 *
 * @note MESSAGE THREAD.
 */
Terminal::Display::~Display()
{
    cancelPendingUpdate();

    processor.getState().get().removeListener (this);

    processor.writeInput = nullptr;
    processor.onResize = nullptr;
    processor.onShellExited = nullptr;
    processor.onClipboardChanged = nullptr;
    processor.onBell = nullptr;
    processor.onDesktopNotification = nullptr;
    processor.getParser().onPreviewFile = nullptr;
    processor.getParser().onImageDecoded = nullptr;

    visitScreen (
        [&] (auto& scr)
        {
            scr.setSelection (nullptr);
        });
    screenSelection.reset();
    removeKeyListener (this);
}

/**
 * @brief Called by the State ValueTree when any property changes.
 *
 * Marks the snapshot dirty and requests a repaint.
 *
 * @note MESSAGE THREAD.
 */
void Terminal::Display::valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&)
{
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
    // Routes PTY writes through Display::writeToPty → Terminal::Session::sendInput or Interprocess::Link::sendInput (mode-agnostic).
    linkManager.emplace (processor.getState(),
                         processor.getGrid(),
                         [this] (const char* data, int len)
                         {
                             writeToPty (data, len);
                         });

    // Construct Terminal::Input once — no Screen dependency, never reconstructed.
    inputHandler.emplace (processor, *linkManager);

    // Switch to the renderer stored in AppState (SSOT).
    switchRenderer (AppState::getContext()->getRendererType());

    inputHandler->buildKeyMap (config.getSelectionKeys());

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

    processor.getParser().onPreviewFile = [this] (const juce::String& filepath,
                                                   int gridRow, int gridCol,
                                                   int cols, int lines)
    {
        const juce::SpinLock::ScopedLockType lock { pendingPreviewLock };
        pendingPreviewPath  = filepath;
        pendingPreviewRow   = gridRow;
        pendingPreviewCol   = gridCol;
        pendingPreviewCols  = cols;
        pendingPreviewLines = lines;
        hasPendingPreview   = true;
    };

    processor.getParser().onImageDecoded = [this] (juce::HeapBlock<uint8_t>&& pixels,
                                                    juce::HeapBlock<int>&& delays,
                                                    int frameCount, int widthPx, int heightPx,
                                                    int gridRow, int gridCol, int cellCols, int cellRows,
                                                    bool /*isPreview*/)
    {
        auto pixelPtr { std::make_shared<juce::HeapBlock<uint8_t>> (std::move (pixels)) };
        auto delayPtr { std::make_shared<juce::HeapBlock<int>> (std::move (delays)) };

        juce::MessageManager::callAsync (
            [this, pixelPtr, delayPtr, frameCount, widthPx, heightPx, gridRow, gridCol, cellCols, cellRows]
            {
                handleDecodedImage (*pixelPtr, *delayPtr, frameCount, widthPx, heightPx,
                                    gridRow, gridCol, cellCols, cellRows);
            });
    };

    linkManager->onOpenMarkdown = [this] (const juce::File& file)
    {
        if (onOpenMarkdown != nullptr)
            onOpenMarkdown (file);
    };

    linkManager->onOpenImage = [this] (const juce::File& file, int triggerRow)
    {
        handleOpenImage (file, triggerRow, 0, 0, 0);
    };
}

/**
 * @brief Positions the overlay on top of Screen at cell-aligned coordinates.
 *
 * Handles both native (config cell count, right-aligned) and conform (protocol
 * cell bounds) modes.  Overlay paints on top of Screen — no viewport split.
 *
 * @param contentArea  The full content rectangle.
 * @note MESSAGE THREAD — called from resized().
 */
void Terminal::Display::setOverlayBounds (juce::Rectangle<int> contentArea) noexcept
{
    int cellW { 0 };
    int cellH { 0 };

    visitScreen ([&] (auto& s)
    {
        cellW = s.getCellWidth();
        cellH = s.getCellHeight();
    });

    if (overlayConform)
    {
        const int overlayX { contentArea.getX() + overlayTriggerCol * cellW };
        const int overlayY { contentArea.getY() + overlayTriggerRow * cellH };
        const int overlayW { overlayCellCols * cellW };
        const int overlayH { overlayCellRows * cellH };

        overlay->setBounds (overlayX, overlayY, overlayW, overlayH);
    }
    else
    {
        const int overlayW { config.nexus.image.cols * cellW };
        const int overlayH { config.nexus.image.rows * cellH };

        int overlayY { contentArea.getY() + overlayTriggerRow * cellH };
        const int maxY { contentArea.getBottom() - overlayH };
        overlayY = juce::jlimit (contentArea.getY(), juce::jmax (contentArea.getY(), maxY), overlayY);

        const int overlayX { contentArea.getRight() - overlayW };
        overlay->setBounds (overlayX, overlayY, overlayW, overlayH);
    }
}

/**
 * @brief Continuous resize: viewport, Grid reflow, overlay, then async PTY signal.
 *
 * Runs on every JUCE resize event.  Updates Screen viewport, reflows Grid
 * content, resets scroll offset, and repositions overlay — all renderer-side
 * work.  Calls triggerAsyncUpdate() to coalesce PTY signalling: the shell
 * receives exactly one SIGWINCH per message-loop drain, regardless of how
 * many resized() calls occurred in the burst.
 *
 * @note MESSAGE THREAD — called by JUCE on every resize event.
 */
void Terminal::Display::resized()
{
    auto contentArea { getContentBounds() };

    if (contentArea.getWidth() > 0 and contentArea.getHeight() > 0)
    {
        visitScreen (
            [&] (auto& s)
            {
                s.setViewport (contentArea);
            });

        const int cols { screenBase().getNumCols() };
        const int rows { screenBase().getNumRows() };

        if (cols > 0 and rows > 0)
        {
            processor.resized (cols, rows);
            processor.getState().setScrollOffset (0);
        }

        if (overlay != nullptr)
            setOverlayBounds (contentArea);
    }

    triggerAsyncUpdate();
}

/**
 * @brief Signals PTY with current terminal dimensions.
 *
 * AsyncUpdater coalesces multiple triggerAsyncUpdate() calls into one
 * callback on the next message-loop pass.  The shell receives exactly one
 * SIGWINCH after the resize burst settles.
 *
 * @note MESSAGE THREAD — AsyncUpdater callback.
 */
void Terminal::Display::handleAsyncUpdate()
{
    const int cols { screenBase().getNumCols() };
    const int rows { screenBase().getNumRows() };

    if (cols > 0 and rows > 0
        and (cols != lastSignaledCols or rows != lastSignaledRows))
    {
        lastSignaledCols = cols;
        lastSignaledRows = rows;

        const int pixelW { physCellWidthCache * cols };
        const int pixelH { physCellHeightCache * rows };

        if (processor.onResize != nullptr)
            processor.onResize (cols, rows, pixelW, pixelH);
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

            processor.writeInput (bytes.toRawUTF8(), static_cast<int> (bytes.getNumBytesAsUTF8()));
        }
    }
}

void Terminal::Display::writeToPty (const char* data, int len) { processor.writeInput (data, len); }

bool Terminal::Display::keyPressed (const juce::KeyPress& key, juce::Component*)
{
    const bool isCopy { inputHandler.has_value() and screenSelection != nullptr
                        and inputHandler->isSelectionCopyKey (key) };

    if (isCopy)
        copySelection();

    const bool dispatched { inputHandler.has_value() and not isCopy
                            and (onProcessExited != nullptr ? inputHandler->handleKeyDirect (key)
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
                                       processor.setScrollOffsetClamped (offset);
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
        const juce::String multifiles { config.nexus.terminal.dropMultifiles };
        const bool shouldQuote { config.nexus.terminal.dropQuoted };
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
                    const juce::String shellProg { config.nexus.shell.program };

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
                processor.writeInput (bytes.toRawUTF8(), static_cast<int> (bytes.getNumBytesAsUTF8()));
        }
    }
}

// GL THREAD
void Terminal::Display::glContextCreated() noexcept
{
    if (std::holds_alternative<Screen<jam::Glyph::GLContext>> (screen))
    {
        jam::BackgroundBlur::enableWindowTransparency();
        std::get<Screen<jam::Glyph::GLContext>> (screen).glContextCreated();
        processor.getGrid().markAllDirty();
        processor.getState().setSnapshotDirty();
    }
}

// GL THREAD
void Terminal::Display::glContextClosing() noexcept
{
    if (std::holds_alternative<Screen<jam::Glyph::GLContext>> (screen))
    {
        std::get<Screen<jam::Glyph::GLContext>> (screen).glContextClosing();
    }
}

juce::Point<int> Terminal::Display::getOriginInTopLevel() const noexcept
{
    const float scale { jam::Typeface::getDisplayScale() };
    const auto* topLevel { getTopLevelComponent() };
    const auto relative { topLevel != nullptr ? topLevel->getLocalPoint (this, juce::Point<int> (0, 0))
                                              : juce::Point<int> (0, 0) };

    return { static_cast<int> (static_cast<float> (relative.x) * scale),
             static_cast<int> (static_cast<float> (relative.y) * scale) };
}

juce::Rectangle<int> Terminal::Display::getContentBounds() const noexcept
{
    auto bounds { getLocalBounds() };
    bounds.removeFromTop (paddingTop);
    bounds.removeFromRight (paddingRight);
    bounds.removeFromBottom (paddingBottom);
    bounds.removeFromLeft (paddingLeft);
    return bounds;
}

// GL THREAD
void Terminal::Display::paintGL() noexcept
{
    if (isVisible() and std::holds_alternative<Screen<jam::Glyph::GLContext>> (screen))
    {
        const auto origin { getOriginInTopLevel() };
        auto& glScreen { std::get<Screen<jam::Glyph::GLContext>> (screen) };

        if (not glScreen.isGLContextReady())
            glScreen.glContextCreated();

        glScreen.renderOpenGL (origin.x, origin.y, getFullViewportHeight());
    }
}

// MESSAGE THREAD
void Terminal::Display::paint (juce::Graphics& g)
{
    if (isVisible() and std::holds_alternative<Screen<jam::Glyph::GraphicsContext>> (screen))
    {
        g.fillAll (findColour (juce::ResizableWindow::backgroundColourId));
        std::get<Screen<jam::Glyph::GraphicsContext>> (screen).renderPaint (g, 0, 0, getHeight());
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

// MESSAGE THREAD
void Terminal::Display::parentHierarchyChanged()
{
    if (isShowing())
    {
        auto safeThis { juce::Component::SafePointer<Display> (this) };

        juce::MessageManager::callAsync (
            [safeThis]
            {
                if (safeThis != nullptr)
                    safeThis->grabKeyboardFocus();
            });
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
        processor.writeInput (focusBytes.toRawUTF8(), static_cast<int> (focusBytes.getNumBytesAsUTF8()));

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
        processor.writeInput (focusBytes.toRawUTF8(), static_cast<int> (focusBytes.getNumBytesAsUTF8()));

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
 * After render, calls `repaint()` to self-invalidate so JUCE's `paint(Graphics&)` fires for
 * this Display — necessary in CPU mode where each pane must receive its own repaint signal.
 * Also fires `onRepaintNeeded()` for cross-cutting concerns: `Window::triggerRepaint()` in GPU
 * mode and status-bar-hint refresh.
 *
 * @note MESSAGE THREAD — VBlankAttachment callbacks run on the message thread.
 */
void Terminal::Display::onVBlank()
{
    if (getComponentID() == AppState::getContext()->getActivePaneID())
    {
        if (juce::Process::isForegroundProcess() and isShowing() and not hasKeyboardFocus (true)
            and juce::Component::getCurrentlyModalComponent() == nullptr)
        {
            toFront (true);
        }
    }

    if (overlay != nullptr and not processor.getState().isPreviewActive())
        dismissPreview();

    if (processor.getState().consumeSnapshotDirty())
        processDirtySnapshot();

    consumePendingPreview();
}

/**
 * @brief Processes a dirty snapshot under the grid resize lock.
 *
 * Refreshes State, rebuilds selection, updates link underlay, calls
 * Screen::render(), and requests repaint.  Falls back to re-dirtying
 * the snapshot if the resize lock is unavailable.
 *
 * @note MESSAGE THREAD — called from onVBlank().
 */
void Terminal::Display::processDirtySnapshot()
{
    const juce::ScopedTryLock lock (processor.getGrid().getResizeLock());

    if (lock.isLocked())
    {
        processor.getState().refresh();

        const int scrollback { processor.getGrid().getScrollbackUsed() };
        const int scrollOffset { processor.getState().getScrollOffset() };
        const int visibleStart { scrollback - scrollOffset };
        const bool isActivePane { getComponentID() == AppState::getContext()->getActivePaneID() };

        rebuildSelectionFromState (isActivePane, visibleStart);

        if (linkManager.has_value())
        {
            const auto& links { linkManager->getClickableLinks() };
            visitScreen (
                [&] (auto& s)
                {
                    s.setLinkUnderlay (links.data(), static_cast<int> (links.size()));
                });
        }

        visitScreen (
            [&] (auto& s)
            {
                s.render (processor.getState(), processor.getGrid());
            });

        repaint();

        if (onRepaintNeeded != nullptr)
            onRepaintNeeded();
    }
    else
    {
        processor.getState().setSnapshotDirty();
    }
}

/**
 * @brief Rebuilds screenSelection from State params for the current frame.
 *
 * Reads selection anchor/cursor from State, converts to visible-row coordinates,
 * and updates the Screen selection pointer.  Clears selection when inactive.
 *
 * @param isActivePane   True when this Display is the focused pane.
 * @param visibleStart   First absolute row visible in the viewport.
 * @note MESSAGE THREAD — called from processDirtySnapshot().
 */
void Terminal::Display::rebuildSelectionFromState (bool isActivePane, int visibleStart)
{
    {
        const bool active { isActivePane and processor.getState().getModalType() == ModalType::selection };
        visitScreen (
            [&] (auto& s)
            {
                s.setSelectionActive (active);
            });
    }

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

/**
 * @brief Applies current config settings to the active screen.
 *
 * @note MESSAGE THREAD.
 */
void Terminal::Display::applyScreenSettings() noexcept
{
    visitScreen (
        [&] (auto& s)
        {
            s.setLigatures (config.display.font.ligatures);
            s.setEmbolden (config.display.font.embolden);
            s.setLineHeight (config.display.font.lineHeight);
            s.setCellWidth (config.display.font.cellWidth);
            s.setTheme (config.buildTheme());
            s.setFontSize (config.dpiCorrectedFontSize() * AppState::getContext()->getWindowZoom());
        });
}

void Terminal::Display::applyConfig() noexcept
{
    applyScreenSettings();

    if (inputHandler.has_value())
        inputHandler->buildKeyMap (config.getSelectionKeys());

    processor.getGrid().markAllDirty();
    processor.getState().setSnapshotDirty();
    resized();
}

/**
 * @brief Reads the current zoom from AppState and recomputes the font size
 *        from Config (base) * AppState (zoom).  Resizes the top-level window
 *        proportionally to the cell metric change to preserve grid dimensions.
 *
 * @note MESSAGE THREAD.
 */
void Terminal::Display::applyZoom (float) noexcept
{
    // Old font size from Screen — the last value set before AppState was updated.
    // No shadow copy: Screen::getBaseFontSize() reads the live field.
    float oldFontSize { 0.0f };
    visitScreen (
        [&] (auto& s)
        {
            oldFontSize = s.getBaseFontSize();
        });

    // New font size from SSOT — lua::Engine (base) * AppState (zoom), computed fresh.
    const float newFontSize { config.dpiCorrectedFontSize()
                              * AppState::getContext()->getWindowZoom() };

    // Raw font metrics for both sizes — multiplier cancels in the ratio.
    const jam::Typeface::Metrics oldMetrics { font.getResolvedTypeface()->calcMetrics (oldFontSize) };
    const jam::Typeface::Metrics newMetrics { font.getResolvedTypeface()->calcMetrics (newFontSize) };

    visitScreen (
        [&] (auto& s)
        {
            s.setFontSize (newFontSize);
        });

    if (oldMetrics.physCellW > 0 and oldMetrics.physCellH > 0 and newMetrics.physCellW > 0 and newMetrics.physCellH > 0)
    {
        if (auto* topLevel { getTopLevelComponent() })
        {
            const int decorW { topLevel->getWidth() - getWidth() };
            const int decorH { topLevel->getHeight() - getHeight() };

            const auto contentBounds { getContentBounds() };
            const int oldContentW { contentBounds.getWidth() };
            const int oldContentH { contentBounds.getHeight() };

            const float ratioW { static_cast<float> (newMetrics.physCellW)
                                 / static_cast<float> (oldMetrics.physCellW) };
            const float ratioH { static_cast<float> (newMetrics.physCellH)
                                 / static_cast<float> (oldMetrics.physCellH) };

            const int newContentW { juce::roundToInt (static_cast<float> (oldContentW) * ratioW) };
            const int newContentH { juce::roundToInt (static_cast<float> (oldContentH) * ratioH) };

            topLevel->setSize (
                newContentW + paddingLeft + paddingRight + decorW, newContentH + paddingTop + paddingBottom + decorH);
        }
    }
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
        if (not std::holds_alternative<Screen<jam::Glyph::GraphicsContext>> (screen))
        {
            screen.emplace<Screen<jam::Glyph::GraphicsContext>> (font, packerRef, graphicsAtlasRef);
        }
    }
    else
    {
        if (not std::holds_alternative<Screen<jam::Glyph::GLContext>> (screen))
        {
            screen.emplace<Screen<jam::Glyph::GLContext>> (font, packerRef, glAtlasRef);
        }
    }

    if (linkManager.has_value())
    {
        mouseHandler.emplace (processor, screenBase(), *linkManager);
        visitScreen (
            [this] (auto& s)
            {
                s.setLinkManager (&*linkManager);
            });
    }

    applyScreenSettings();

    // Wire physCellDimensions callback: Screen::calc() notifies Parser
    // so the READER THREAD Sixel decoder has the correct cell grid size.
    // Also cache the physical cell dimensions for MESSAGE-THREAD image rendering.
    visitScreen (
        [this] (auto& s)
        {
            s.onPhysCellDimensionsChanged = [this] (int widthPx, int heightPx)
            {
                processor.getParser().setPhysCellDimensions (widthPx, heightPx);
                physCellWidthCache = widthPx;
                physCellHeightCache = heightPx;
            };
        });

    processor.getGrid().markAllDirty();
    processor.getState().setSnapshotDirty();

    if (getWidth() > 0 and getHeight() > 0)
    {
        resized();
    }
}
