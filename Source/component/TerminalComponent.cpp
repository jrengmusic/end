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
    buildSelectionKeyMap();

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
    const int page { screen.getNumRows() };
    const int current { session.getState().getScrollOffset() };

    if (code == juce::KeyPress::pageUpKey)
    {
        setScrollOffsetClamped (current + page);
    }
    else if (code == juce::KeyPress::pageDownKey)
    {
        setScrollOffsetClamped (current - page);
    }
    else if (code == juce::KeyPress::homeKey)
    {
        setScrollOffsetClamped (session.getGrid().getScrollbackUsed());
    }
    else if (code == juce::KeyPress::endKey)
    {
        setScrollOffsetClamped (0);
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

    // Modal intercept fires BEFORE the action system.
    // This ensures Escape, Ctrl+V, and any other mapped key cannot
    // leak through to paste or other actions while a modal is active.
    if (session.getState().isModal())
    {
        if (handleModalKey (key))
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

bool Terminal::Component::handleModalKey (const juce::KeyPress& key) noexcept
{
    const auto type { session.getState().getModalType() };

    if (type == ModalType::selection)
        return handleSelectionKey (key);

    if (type == ModalType::openFile)
        return handleOpenFileKey (key);

    return false;
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
    pendingG = false;
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
    pendingG = false;
    boxSelection = BoxSelection {};
    screenSelection.reset();
    screen.setSelection (nullptr);
    session.getState().setSnapshotDirty();
}

void Terminal::Component::enterOpenFileMode() noexcept
{
    const juce::ScopedTryLock stl { session.getGrid().getResizeLock() };

    const juce::String cwd { session.getState().get().getProperty (Terminal::ID::cwd).toString() };

    try
    {
        activeLinks = scanViewportForLinks (session.getGrid(), cwd, false);
    }
    catch (...)
    {
        activeLinks.clear();
    }

    screen.setHintOverlay (activeLinks.data(), static_cast<int> (activeLinks.size()));
    session.getState().setModalType (ModalType::openFile);
    session.getState().setSnapshotDirty();
}

bool Terminal::Component::handleOpenFileKey (const juce::KeyPress& key) noexcept
{
    if (key == juce::KeyPress::escapeKey)
    {
        screen.setHintOverlay (nullptr, 0);
        activeLinks.clear();
        session.getState().setModalType (ModalType::none);
        session.getState().setSnapshotDirty();
    }
    else
    {
        const juce::juce_wchar ch { key.getTextCharacter() };
        const char lower { static_cast<char> (ch >= 'A' and ch <= 'Z' ? ch + 32 : ch) };
        const bool isLetter { lower >= 'a' and lower <= 'z' };

        if (isLetter)
        {
            const LinkSpan* matched { nullptr };

            for (const auto& span : activeLinks)
            {
                if (span.hintLabel[0] == lower)
                {
                    matched = &span;
                }
            }

            if (matched != nullptr)
            {
                if (matched->type == LinkDetector::LinkType::url)
                {
                    juce::URL { matched->uri }.launchInDefaultBrowser();
                }
                else
                {
                    const juce::String path { matched->uri.fromFirstOccurrenceOf ("file://", false, false) };
                    const juce::String editor { Config::getContext()->getString (Config::Key::hyperlinksEditor) };
                    const juce::String command { editor + " " + path + "\r" };
                    session.writeToPty (command.toRawUTF8(), static_cast<int> (command.getNumBytesAsUTF8()));
                }

                screen.setHintOverlay (nullptr, 0);
                activeLinks.clear();
                session.getState().setModalType (ModalType::none);
                session.getState().setSnapshotDirty();
            }
        }
    }

    return true;// fully modal — consume all keys
}

std::vector<Terminal::LinkSpan> Terminal::Component::scanViewportForLinks (const Grid& grid,
                                                                           const juce::String& cwd,
                                                                           bool outputRowsOnly)
{
    session.getParser().clearOsc8Links();
    std::vector<LinkSpan> spans;

    const int visibleRows { grid.getVisibleRows() };
    const int cols { grid.getCols() };
    const int blockTop { outputRowsOnly ? session.getState().getOutputBlockTop() : 0 };
    const int blockBottom { outputRowsOnly ? session.getState().getOutputBlockBottom() : visibleRows - 1 };

    for (int row = 0; row < visibleRows; ++row)
    {
        if (outputRowsOnly and (blockTop < 0 or row < blockTop or row > blockBottom))
            continue;

        const Cell* rowCells { grid.activeVisibleRow (row) };

        if (rowCells == nullptr)
            continue;

        int col { 0 };

        while (col < cols)
        {
            // Skip whitespace / empty cells
            while (col < cols
                   and (rowCells[col].codepoint == 0
                        or rowCells[col].codepoint <= 0x20))
            {
                ++col;
            }

            if (col >= cols)
                break;

            // Accumulate non-whitespace token
            const int tokenStartCol { col };
            juce::String token;

            while (col < cols
                   and rowCells[col].codepoint > 0x20)
            {
                token += juce::String::charToString (
                    static_cast<juce::juce_wchar> (rowCells[col].codepoint));
                ++col;
            }

            const int tokenLength { col - tokenStartCol };
            const LinkDetector::LinkType linkType { LinkDetector::classify (token) };

            if (linkType != LinkDetector::LinkType::none)
            {
                LinkSpan span;
                span.row = row;
                span.col = tokenStartCol;
                span.length = tokenLength;
                span.type = linkType;

                if (linkType == LinkDetector::LinkType::url)
                {
                    span.uri = token;
                }
                else
                {
                    // Resolve relative paths against cwd
                    juce::File resolved;

                    if (juce::File::isAbsolutePath (token))
                    {
                        resolved = juce::File { token };
                    }
                    else
                    {
                        resolved = juce::File { cwd }.getChildFile (token);
                    }

                    span.uri = resolved.getFullPathName().isNotEmpty()
                                   ? "file://" + resolved.getFullPathName()
                                   : "file://" + token;
                }

                spans.push_back (std::move (span));
            }
        }
    }

    // Merge OSC 8 explicit hyperlink spans from the parser.
    // These are produced by programs that emit OSC 8 start/end sequences
    // (e.g. compilers printing clickable file:// links, man pages, etc.).
    for (const auto& osc : session.getParser().getOsc8Links())
    {
        if (outputRowsOnly and (blockTop < 0 or osc.row < blockTop or osc.row > blockBottom))
            continue;

        const juce::String uri { osc.uri };

        if (uri.isEmpty())
            continue;

        LinkSpan span;
        span.row = osc.row;
        span.col = osc.startCol;
        span.length = osc.endCol - osc.startCol;
        span.uri = uri;

        if (uri.startsWith ("http://") or uri.startsWith ("https://"))
        {
            span.type = LinkDetector::LinkType::url;
        }
        else
        {
            span.type = LinkDetector::LinkType::file;

            if (not uri.startsWith ("file://"))
                span.uri = "file://" + uri;
        }

        spans.push_back (std::move (span));
    }

    // Assign hint labels: each span gets a unique character from its own filename.
    // First char preferred. If taken, shift to next char until unique.
    std::unordered_set<char> usedLabels;

    for (auto& span : spans)
    {
        const int tokenEnd { span.col + span.length };
        bool assigned { false };

        for (int c { span.col }; c < tokenEnd and not assigned; ++c)
        {
            const Cell* rowCells { grid.activeVisibleRow (span.row) };

            if (rowCells != nullptr)
            {
                const uint32_t cp { rowCells[c].codepoint };
                const char lower { static_cast<char> (cp >= 'A' and cp <= 'Z' ? cp + 32 : cp) };

                if (lower >= 'a' and lower <= 'z' and usedLabels.find (lower) == usedLabels.end())
                {
                    span.hintLabel[0] = lower;
                    span.hintLabel[1] = 0;
                    span.labelCol = c;
                    usedLabels.insert (lower);
                    assigned = true;
                }
            }
        }

        if (not assigned)
        {
            span.hintLabel[0] = 0;
            span.labelCol = span.col;
        }
    }

    return spans;
}

bool Terminal::Component::handleSelectionKey (const juce::KeyPress& key) noexcept
{
    const int maxRow { session.getGrid().getVisibleRows() + session.getGrid().getScrollbackUsed() - 1 };
    const int maxCol { session.getGrid().getCols() - 1 };

    auto& st { session.getState() };

    bool consumed { false };

    if (key == selectionKeys.exit)
    {
        st.setSelectionType (static_cast<int> (SelectionType::none));
        st.setModalType (ModalType::none);
        pendingG = false;
        boxSelection = BoxSelection {};
        screenSelection.reset();
        screen.setSelection (nullptr);
        consumed = true;
    }
    else if (key == selectionKeys.visualBlock)
    {
        const auto current { static_cast<SelectionType> (st.getSelectionType()) };

        if (current == SelectionType::visualBlock)
        {
            st.setSelectionType (static_cast<int> (SelectionType::none));
            st.setModalType (ModalType::none);
        }
        else
        {
            st.setSelectionAnchor (st.getSelectionCursorRow(), st.getSelectionCursorCol());
            st.setSelectionType (static_cast<int> (SelectionType::visualBlock));
        }

        consumed = true;
    }
    else if (key == selectionKeys.left)
    {
        st.setSelectionCursor (st.getSelectionCursorRow(),
                               std::max (0, st.getSelectionCursorCol() - 1));
        consumed = true;
    }
    else if (key == selectionKeys.down)
    {
        st.setSelectionCursor (std::min (maxRow, st.getSelectionCursorRow() + 1),
                               st.getSelectionCursorCol());
        consumed = true;
    }
    else if (key == selectionKeys.up)
    {
        st.setSelectionCursor (std::max (0, st.getSelectionCursorRow() - 1),
                               st.getSelectionCursorCol());
        consumed = true;
    }
    else if (key == selectionKeys.right)
    {
        st.setSelectionCursor (st.getSelectionCursorRow(),
                               std::min (maxCol, st.getSelectionCursorCol() + 1));
        consumed = true;
    }
    else if (key == selectionKeys.visualLine)
    {
        const auto current { static_cast<SelectionType> (st.getSelectionType()) };

        if (current == SelectionType::visualLine)
        {
            st.setSelectionType (static_cast<int> (SelectionType::none));
            st.setModalType (ModalType::none);
        }
        else
        {
            st.setSelectionAnchor (st.getSelectionCursorRow(), st.getSelectionCursorCol());
            st.setSelectionType (static_cast<int> (SelectionType::visualLine));
        }

        consumed = true;
    }
    else if (key == selectionKeys.visual)
    {
        const auto current { static_cast<SelectionType> (st.getSelectionType()) };

        if (current == SelectionType::visual)
        {
            st.setSelectionType (static_cast<int> (SelectionType::none));
            st.setModalType (ModalType::none);
        }
        else
        {
            st.setSelectionAnchor (st.getSelectionCursorRow(), st.getSelectionCursorCol());
            st.setSelectionType (static_cast<int> (SelectionType::visual));
        }

        consumed = true;
    }
    else if (key == selectionKeys.copy or key == selectionKeys.globalCopy)
    {
        const auto smType { static_cast<SelectionType> (st.getSelectionType()) };

        if (smType != SelectionType::none)
        {
            const juce::ScopedTryLock tryLock (session.getGrid().getResizeLock());

            if (tryLock.isLocked())
            {
                const int scrollback { session.getGrid().getScrollbackUsed() };
                const int scrollOffset { st.getScrollOffset() };
                const int visibleStart { scrollback - scrollOffset };
                const int cols { session.getGrid().getCols() };

                const int anchorVisRow { st.getSelectionAnchorRow() - visibleStart };
                const int cursorVisRow { st.getSelectionCursorRow() - visibleStart };
                const int anchorCol { st.getSelectionAnchorCol() };
                const int cursorCol { st.getSelectionCursorCol() };

                juce::String text;

                if (smType == SelectionType::visual)
                {
                    const juce::Point<int> start { anchorCol, anchorVisRow };
                    const juce::Point<int> end { cursorCol, cursorVisRow };
                    text = session.getGrid().extractText (start, end);
                }
                else if (smType == SelectionType::visualLine)
                {
                    const juce::Point<int> start { 0, std::min (anchorVisRow, cursorVisRow) };
                    const juce::Point<int> end { cols - 1, std::max (anchorVisRow, cursorVisRow) };
                    text = session.getGrid().extractText (start, end);
                }
                else
                {
                    const juce::Point<int> topLeft {
                        std::min (anchorCol, cursorCol),
                        std::min (anchorVisRow, cursorVisRow)
                    };
                    const juce::Point<int> bottomRight {
                        std::max (anchorCol, cursorCol),
                        std::max (anchorVisRow, cursorVisRow)
                    };
                    text = session.getGrid().extractBoxText (topLeft, bottomRight);
                }

                juce::SystemClipboard::copyTextToClipboard (text);
            }
        }

        st.setSelectionType (static_cast<int> (SelectionType::none));
        st.setModalType (ModalType::none);
        pendingG = false;
        consumed = true;
    }
    else if (key == selectionKeys.bottom)
    {
        st.setSelectionCursor (maxRow, st.getSelectionCursorCol());
        consumed = true;
    }
    else if (key == selectionKeys.top)
    {
        if (pendingG)
        {
            st.setSelectionCursor (0, 0);
            pendingG = false;
        }
        else
        {
            pendingG = true;
        }

        consumed = true;
    }
    else if (key == selectionKeys.lineStart)
    {
        st.setSelectionCursor (st.getSelectionCursorRow(), 0);
        consumed = true;
    }
    else if (key == selectionKeys.lineEnd)
    {
        st.setSelectionCursor (st.getSelectionCursorRow(), maxCol);
        consumed = true;
    }

    if (consumed)
    {
        const int cursorRow { st.getSelectionCursorRow() };
        const int visibleRows { session.getGrid().getVisibleRows() };
        const int scrollback { session.getGrid().getScrollbackUsed() };
        const int visibleStart { scrollback - st.getScrollOffset() };
        const int visibleEnd { visibleStart + visibleRows - 1 };

        if (cursorRow < visibleStart)
        {
            setScrollOffsetClamped (scrollback - cursorRow);
        }
        else if (cursorRow > visibleEnd)
        {
            setScrollOffsetClamped (scrollback - (cursorRow - visibleRows + 1));
        }

        updateSelectionHighlight();
        st.setSnapshotDirty();
    }

    return true;
}

/**
 * @brief Syncs `screenSelection` and `screen.setSelection()` from State.
 *
 * Maps the State selectionType to `ScreenSelection::SelectionType`, sets
 * anchor/end from the selection cursor and anchor stored in State, and calls
 * `screen.setSelection()`.  Clears the selection when selectionType is `none`.
 *
 * @note MESSAGE THREAD.
 */
void Terminal::Component::updateSelectionHighlight() noexcept
{
    const auto smType { static_cast<SelectionType> (session.getState().getSelectionType()) };

    if (smType == SelectionType::none)
    {
        screenSelection.reset();
        screen.setSelection (nullptr);
    }
    else
    {
        if (screenSelection == nullptr)
        {
            screenSelection = std::make_unique<ScreenSelection>();
        }

        const int scrollback { session.getGrid().getScrollbackUsed() };
        const int scrollOffset { session.getState().getScrollOffset() };
        const int visibleStart { scrollback - scrollOffset };

        const int anchorVisRow { session.getState().getSelectionAnchorRow() - visibleStart };
        const int cursorVisRow { session.getState().getSelectionCursorRow() - visibleStart };

        screenSelection->anchor = { session.getState().getSelectionAnchorCol(), anchorVisRow };
        screenSelection->end = { session.getState().getSelectionCursorCol(), cursorVisRow };

        if (smType == SelectionType::visual)
        {
            screenSelection->type = ScreenSelection::SelectionType::linear;
        }
        else if (smType == SelectionType::visualLine)
        {
            screenSelection->type = ScreenSelection::SelectionType::line;
        }
        else
        {
            screenSelection->type = ScreenSelection::SelectionType::box;
        }

        screen.setSelection (screenSelection.get());
    }
}

/**
 * @brief Parses all selection-mode key strings from Config into the cached selectionKeys struct.
 *
 * Called from initialise() and applyConfig() so config reloads take effect without restart.
 *
 * @note MESSAGE THREAD.
 */
void Terminal::Component::buildSelectionKeyMap() noexcept
{
    auto* cfg { Config::getContext() };

    selectionKeys.up = Terminal::Action::parseShortcut (cfg->getString (Config::Key::keysSelectionUp));
    selectionKeys.down = Terminal::Action::parseShortcut (cfg->getString (Config::Key::keysSelectionDown));
    selectionKeys.left = Terminal::Action::parseShortcut (cfg->getString (Config::Key::keysSelectionLeft));
    selectionKeys.right = Terminal::Action::parseShortcut (cfg->getString (Config::Key::keysSelectionRight));
    selectionKeys.visual = Terminal::Action::parseShortcut (cfg->getString (Config::Key::keysSelectionVisual));
    selectionKeys.visualLine = Terminal::Action::parseShortcut (cfg->getString (Config::Key::keysSelectionVisualLine));
    // Visual block uses real Ctrl (not Cmd on macOS).  parseShortcut maps
    // "ctrl" → commandModifier on macOS, which conflicts with paste.  We
    // build the KeyPress directly with ctrlModifier so Ctrl+V is unambiguous
    // on all platforms.  The config value is still read so users can remap.
    {
        const juce::String raw { cfg->getString (Config::Key::keysSelectionVisualBlock) };
        const bool hasCtrl { raw.containsIgnoreCase ("ctrl") and not raw.containsIgnoreCase ("cmd") };

        if (hasCtrl)
        {
            const int vChar { raw.toLowerCase().contains ("v") ? 'v' : 'v' };
            selectionKeys.visualBlock = juce::KeyPress (vChar, juce::ModifierKeys::ctrlModifier, 0);
        }
        else
        {
            selectionKeys.visualBlock = Terminal::Action::parseShortcut (raw);
        }
    }
    selectionKeys.copy = Terminal::Action::parseShortcut (cfg->getString (Config::Key::keysSelectionCopy));
    selectionKeys.globalCopy = Terminal::Action::parseShortcut (cfg->getString (Config::Key::keysCopy));
    selectionKeys.top = Terminal::Action::parseShortcut (cfg->getString (Config::Key::keysSelectionTop));
    selectionKeys.bottom = Terminal::Action::parseShortcut (cfg->getString (Config::Key::keysSelectionBottom));
    selectionKeys.lineStart = Terminal::Action::parseShortcut (cfg->getString (Config::Key::keysSelectionLineStart));
    selectionKeys.lineEnd = Terminal::Action::parseShortcut (cfg->getString (Config::Key::keysSelectionLineEnd));
    selectionKeys.exit = Terminal::Action::parseShortcut (cfg->getString (Config::Key::keysSelectionExit));
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
            const int delta { scrollUp ? scrollLines : -scrollLines };
            setScrollOffsetClamped (session.getState().getScrollOffset() + delta);
        }
    }
    else
    {
        // --- Smooth (trackpad) path ---
        // Scale the tiny per-frame delta by scrollLines so the config value still
        // controls overall speed, then accumulate until whole lines are reached.
        scrollAccumulator += wheel.deltaY * static_cast<float> (scrollLines) * trackpadDeltaScale;

        const int lines { static_cast<int> (scrollAccumulator) };

        if (lines != 0)
        {
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
                setScrollOffsetClamped (session.getState().getScrollOffset() + lines);
            }
        }
    }
}

/**
 * @brief Updates the mouse cursor and refreshes clickable link spans on hover.
 *
 * Scans the viewport for links (lazily, only when `linkScanNeeded`) and
 * hit-tests the hovered cell.  Shows `PointingHandCursor` over a link span,
 * `NormalCursor` otherwise.  Inactive when a modal is active or when mouse
 * events are forwarded to the PTY.
 *
 * @param event  The mouse event.
 * @note MESSAGE THREAD.
 */
void Terminal::Component::mouseMove (const juce::MouseEvent& event)
{
    if (not shouldForwardMouseToPty() and not session.getState().isModal())
    {
        if (linkScanNeeded)
        {
            const juce::ScopedTryLock stl { session.getGrid().getResizeLock() };

            if (stl.isLocked())
            {
                const juce::String cwd { session.getState().get().getProperty (Terminal::ID::cwd).toString() };

                try
                {
                    clickableLinks = scanViewportForLinks (session.getGrid(), cwd, true);
                }
                catch (...)
                {
                    clickableLinks.clear();
                }

                screen.setLinkUnderlay (clickableLinks.data(), static_cast<int> (clickableLinks.size()));
                session.getState().setSnapshotDirty();
                linkScanNeeded = false;
            }
        }

        const auto cell { screen.cellAtPoint (event.x, event.y) };
        bool overLink { false };

        for (const auto& span : clickableLinks)
        {
            if (cell.y == span.row and cell.x >= span.col and cell.x < span.col + span.length)
            {
                overLink = true;
                break;
            }
        }

        if (overLink)
        {
            setMouseCursor (juce::MouseCursor::PointingHandCursor);
        }
        else
        {
            setMouseCursor (juce::MouseCursor::NormalCursor);
        }
    }
    else
    {
        setMouseCursor (juce::MouseCursor::NormalCursor);
    }
}

/**
 * @brief Begins a selection or forwards a mouse-button SGR event.
 *
 * - **Mouse tracking active**: writes SGR button-0 press at the clicked cell.
 * - **Triple-click (click count 3)**: selects the entire clicked row directly
 *   via `ScreenSelection` with type `line`.  No `ModalType` is set — mouse
 *   selection is independent of vim-style selection mode.
 * - **Single click**: records the anchor cell for a potential drag.  Clears
 *   any existing drag selection (`screenSelection` + `screen.setSelection()`).
 *   Does not touch modal state — vim-style selection mode is unaffected.
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
    else if (event.getNumberOfClicks() == 3)
    {
        // Triple-click: select the entire clicked row directly via ScreenSelection.
        // No ModalType — mouse selection is independent of vim-style selection mode.
        const auto cell { screen.cellAtPoint (event.x, event.y) };

        screenSelection = std::make_unique<ScreenSelection>();
        screenSelection->type = ScreenSelection::SelectionType::line;
        screenSelection->anchor = { 0, cell.y };
        screenSelection->end = { 0, cell.y };

        boxSelection.anchorCol = 0;
        boxSelection.anchorRow = cell.y;
        boxSelection.endCol = 0;
        boxSelection.endRow = cell.y;
        boxSelection.active = false;

        screen.setSelection (screenSelection.get());
        session.getState().setSnapshotDirty();
    }
    else
    {
        const auto cell { screen.cellAtPoint (event.x, event.y) };

        // Click-mode dispatch: hit-test against clickable link spans.
        // Only active when no modal is open and content is at the live view.
        if (not session.getState().isModal()
            and session.getState().getScrollOffset() == 0)
        {
            const LinkSpan* matched { nullptr };

            for (const auto& span : clickableLinks)
            {
                if (cell.y == span.row and cell.x >= span.col and cell.x < span.col + span.length)
                {
                    matched = &span;
                    break;
                }
            }

            if (matched != nullptr)
            {
                if (matched->type == LinkDetector::LinkType::url)
                {
                    juce::URL { matched->uri }.launchInDefaultBrowser();
                }
                else
                {
                    const juce::String path { matched->uri.fromFirstOccurrenceOf ("file://", false, false) };
                    const juce::String editor { Config::getContext()->getString (Config::Key::hyperlinksEditor) };
                    const juce::String command { editor + " " + path + "\r" };
                    session.writeToPty (command.toRawUTF8(), static_cast<int> (command.getNumBytesAsUTF8()));
                }
            }
            else
            {
                // Single click on non-link: record anchor for potential drag.
                // Clear any existing drag selection.
                boxSelection.anchorCol = cell.x;
                boxSelection.anchorRow = cell.y;
                boxSelection.endCol = cell.x;
                boxSelection.endRow = cell.y;
                boxSelection.active = false;

                screenSelection.reset();
                screen.setSelection (nullptr);
                session.getState().setSnapshotDirty();
            }
        }
        else
        {
            // Single click: record anchor for potential drag.
            // Clear any existing drag selection.
            boxSelection.anchorCol = cell.x;
            boxSelection.anchorRow = cell.y;
            boxSelection.endCol = cell.x;
            boxSelection.endRow = cell.y;
            boxSelection.active = false;

            screenSelection.reset();
            screen.setSelection (nullptr);
            session.getState().setSnapshotDirty();
        }
    }
}

/**
 * @brief Selects the word under the cursor or forwards a double-click SGR event.
 *
 * - **Mouse tracking active**: writes SGR button-0 press at the clicked cell.
 * - **No tracking**: scans left and right from the clicked column to find word
 *   boundaries (non-space codepoints, > 0x20) and sets `screenSelection` with
 *   type `linear` spanning word start to word end.  No `ModalType` is set —
 *   mouse selection is independent of vim-style selection mode.
 *
 * Word boundary scan uses `scrollbackRow()` so it works correctly when scrolled
 * back into the scrollback buffer.
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
    else
    {
        const auto cell { screen.cellAtPoint (event.x, event.y) };
        const int scrollOffset { session.getState().getScrollOffset() };
        const int cols { session.getGrid().getCols() };

        // Scan the visible row (with scroll offset applied) to find word boundaries.
        const Cell* row { session.getGrid().scrollbackRow (cell.y, scrollOffset) };

        int wordStart { cell.x };
        int wordEnd { cell.x };

        if (row != nullptr)
        {
            // Scan left: stop at space (codepoint <= 0x20) or column 0.
            int c { cell.x - 1 };
            while (c >= 0 and row[c].codepoint > 0x20)
            {
                wordStart = c;
                --c;
            }

            // Scan right: stop at space or end of line.
            c = cell.x + 1;
            while (c < cols and row[c].codepoint > 0x20)
            {
                wordEnd = c;
                ++c;
            }
        }

        // Write directly to screenSelection — no ModalType, no State selection params.
        screenSelection = std::make_unique<ScreenSelection>();
        screenSelection->type = ScreenSelection::SelectionType::linear;
        screenSelection->anchor = { wordStart, cell.y };
        screenSelection->end = { wordEnd, cell.y };

        // Sync boxSelection for copy compatibility.
        boxSelection.anchorCol = wordStart;
        boxSelection.anchorRow = cell.y;
        boxSelection.endCol = wordEnd;
        boxSelection.endRow = cell.y;
        boxSelection.active = false;

        screen.setSelection (screenSelection.get());
        session.getState().setSnapshotDirty();
    }
}

/**
 * @brief Extends the selection or forwards a mouse-motion SGR event.
 *
 * - **Motion tracking active**: writes SGR button-32 (drag) at the current cell.
 * - **No tracking, threshold not met**: drag distance (Manhattan) from the anchor
 *   is less than 2 cells — no selection is started yet.
 * - **No tracking, threshold met, first qualifying drag**: creates `screenSelection`
 *   with type `linear` anchored at the mouseDown position; sets `boxSelection.active`.
 *   No `ModalType` is set — mouse drag is independent of vim-style selection mode.
 * - **No tracking, selection active**: extends `screenSelection->end` to the
 *   current cell and calls `screen.setSelection()`.
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
        const int maxCol { session.getGrid().getCols() - 1 };
        const int maxRow { session.getGrid().getVisibleRows() - 1 };

        const int clampedCol { juce::jlimit (0, maxCol, cell.x) };
        const int clampedRow { juce::jlimit (0, maxRow, cell.y) };

        // 2-cell Manhattan distance threshold before starting a drag selection.
        const int manhattanDist { std::abs (clampedCol - boxSelection.anchorCol)
                                + std::abs (clampedRow - boxSelection.anchorRow) };

        if (manhattanDist >= 2)
        {
            if (not boxSelection.active)
            {
                // Threshold crossed — create the screenSelection anchored at mouseDown position.
                screenSelection = std::make_unique<ScreenSelection>();
                screenSelection->type = ScreenSelection::SelectionType::linear;
                screenSelection->anchor = { boxSelection.anchorCol, boxSelection.anchorRow };
                screenSelection->end = { clampedCol, clampedRow };
                boxSelection.active = true;
            }
            else
            {
                // Extend existing drag selection to current cell.
                screenSelection->end = { clampedCol, clampedRow };
            }

            // Sync boxSelection end for copy compatibility.
            boxSelection.endCol = clampedCol;
            boxSelection.endRow = clampedRow;

            screen.setSelection (screenSelection.get());
            session.getState().setSnapshotDirty();
        }
    }
}

/**
 * @brief Finalises the selection or forwards a mouse-button-release SGR event.
 *
 * - **Mouse tracking active**: writes an SGR button-0 release sequence.
 * - **Drag completed**: `screenSelection` remains visible so the user can Cmd+C
 *   to copy or click elsewhere to clear it.  `boxSelection.active` is reset to
 *   false so subsequent clicks begin a fresh selection rather than extending.
 * - **Plain click / double-click / triple-click**: no drag occurred; the selection
 *   set by `mouseDown()` or `mouseDoubleClick()` is left intact.
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
        // If a drag selection was active, keep the screenSelection highlighted.
        // The user can Cmd+C to copy or click elsewhere to clear it.
        // Reset active flag so subsequent clicks don't extend the selection.
        boxSelection.active = false;
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
        linkScanNeeded = true;
        clickableLinks.clear();
        screen.setLinkUnderlay (nullptr, 0);

        const juce::ScopedTryLock lock (session.getGrid().getResizeLock());

        if (lock.isLocked())
        {
            // Compute the visible-row coordinate for the selection cursor before
            // rendering.  State stores absolute (scrollback-aware) rows;
            // the renderer needs visible rows (0 = topmost visible row).
            // visibleRow = absoluteRow - (scrollbackUsed - scrollOffset)
            {
                const bool active { session.getState().getModalType() == ModalType::selection };
                const int scrollback { session.getGrid().getScrollbackUsed() };
                const int scrollOffset { session.getState().getScrollOffset() };
                const int visibleRow { session.getState().getSelectionCursorRow()
                                       - (scrollback - scrollOffset) };
                screen.setSelectionCursor (active, visibleRow, session.getState().getSelectionCursorCol());
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
    buildSelectionKeyMap();
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

void Terminal::Component::setScrollOffsetClamped (int newOffset) noexcept
{
    const int maxOffset { session.getGrid().getScrollbackUsed() };
    const int current { session.getState().getScrollOffset() };
    const int clamped { juce::jlimit (0, maxOffset, newOffset) };

    if (clamped != current)
    {
        session.getState().setScrollOffset (clamped);
        session.getState().setSnapshotDirty();
    }
}
