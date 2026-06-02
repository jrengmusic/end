#include "Display.h"

terminal::Display::Display (terminal::Session& sessionToUse)
    : session (sessionToUse)
    , processor (sessionToUse.getProcessor())
    , state (sessionToUse.getProcessor().getState())
    , linkManager (sessionToUse.getProcessor().getState(),
                   [&sessionToUse] (const char* data, int len)
                   {
                       sessionToUse.getProcessor().writeInput (data, len);
                   })
    , input (sessionToUse.getProcessor(), linkManager)
    , mouse (sessionToUse.getProcessor(), 0, 0, linkManager)
{
    // Parent CodeView (owned by Session) for rendering via the Component hierarchy.
    addAndMakeVisible (session.getTextEditor());
    session.getTextEditor().addKeyListener (this);

    // Graft DISPLAY node via ComponentAttachment — registerNodeAtomics fires on appendChild
    // and creates Parameter<int> entries in the DISPLAY group for font metrics.
    attachment = std::make_unique<jam::ComponentAttachment> (state,
                                                             terminal::id::DISPLAY,
                                                             std::initializer_list<jam::ComponentAttachment::Property> {
                                                                 { terminal::id::cellWidth,  0 },
                                                                 { terminal::id::cellHeight, 0 },
                                                                 { terminal::id::baseline,   0 },
                                                                 { terminal::id::fontSize,   0 }
    });

    configListener.start();

    // Listen to per-session terminal state for content updates (screenDirty).
    state.addListener (this);

    applyFromAppModel();
}

terminal::Display::~Display()
{
    state.removeListener (this);
    configListener.stop();
    session.getTextEditor().removeKeyListener (this);
}

// PaneComponent
juce::String terminal::Display::getPaneType() const noexcept
{
    return Map::PaneType::getContext()->get (Map::PaneType::terminal);
}
juce::ValueTree terminal::Display::getValueTree() noexcept { return state.getRootTree(); }
void terminal::Display::applyFromAppModel() noexcept
{
    const auto* appState { AppModel::getContext() };

    const jam::Font font { appState->getFontFamily(),
                           appState->getFontSize(),
                           static_cast<float> (appState->getCellWidth()),
                           static_cast<float> (appState->getLineHeight()) };

    session.getTextEditor().setFont (font);
    session.getTextEditor().setCaretChar (jam::toChar (appState->getCursorCodepoint()));
    session.getTextEditor().setCaretShape (appState->getCursorStyle());
    session.getTextEditor().setCaretBlinkRate (appState->getCursorBlinkInterval());

    attachment->setValue (terminal::id::cellWidth, font.cellWidth);
    attachment->setValue (terminal::id::cellHeight, font.cellHeight);
    attachment->setValue (terminal::id::baseline, font.baseline);
    attachment->setValue (terminal::id::fontSize, static_cast<int> (font.fontSize));

    mouse.setCellSize (font.cellWidth, font.cellHeight);
    input.buildKeyMap();
}

void terminal::Display::ConfigListener::start() noexcept
{
    appState = AppModel::getContext()->getRootTree();
    appState.addListener (this);
}

void terminal::Display::ConfigListener::stop() noexcept
{
    appState.removeListener (this);
}

void terminal::Display::ConfigListener::valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier&)
{
    if (tree.getParent() == appState)
        display.applyFromAppModel();
}

void terminal::Display::valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property)
{
    if (property == id::value and tree.getType() == jam::Model::PARAM)
    {
        const juce::Identifier paramId { tree.getProperty (id::id).toString() };
        const int activeScreen { state.getActiveScreen() };

        if (paramId == id::screenDirty or paramId == id::activeScreen)
        {
            jam::debug::Log::write ("DRAIN paramId=" + paramId.toString()
                                    + " activeScreen=" + juce::String (activeScreen)
                                    + " parent=" + tree.getParent().getType().toString());

            session.getTextEditor().setWrapEnabled (activeScreen == Map::Screen::normal);

            jam::CodeModel& doc { session.getCodeModel() };

            // setActive first — all getNumLines / remove / append operate on THIS screen's deque.
            doc.setActive (activeScreen);

            // D13a: clear alternate screen on entry — VT spec (DECSET ?1049h clears alt).
            if (paramId == id::activeScreen and activeScreen == Map::Screen::alternate)
            {
                doc.clear();
                liveTailExtent.at (static_cast<size_t> (activeScreen)) = 0;
                session.getTextEditor().setViewportLineCount (0);
            }
            else
            {
                // (a) Remove the previous live tail of THIS screen — document is pure history after this.
                // liveTailExtent[activeScreen] is the count laid down for this screen last tick;
                // using the per-screen slot ensures a screen switch reads the correct deque's extent.
                const int extent { liveTailExtent.at (static_cast<size_t> (activeScreen)) };

                if (extent > 0)
                {
                    const int numLines { doc.getNumLines() };
                    jassert (numLines >= extent);
                    doc.remove (juce::Range<int> { numLines - extent, numLines });
                    liveTailExtent.at (static_cast<size_t> (activeScreen)) = 0;
                }
            }

            // (b) Drain departed history lines.
            // D13a: on alt-screen entry, drain and discard — residual normal-screen departures
            // between the last flush and the switch must not leak into the fresh alt screen.
            {
                jam::CodeLine line {};

                if (paramId == id::activeScreen and activeScreen == Map::Screen::alternate)
                {
                    while (processor.drainHistory (line)) {}
                }
                else
                {
                    while (processor.drainHistory (line))
                        doc.append (std::move (line));
                }
            }

            // (c) Drain the fresh active viewport → append at document end, counting rows.
            {
                jam::CodeLine line {};
                int newExtent { 0 };

                while (processor.drainActive (line))
                {
                    doc.append (std::move (line));
                    ++newExtent;
                }

                liveTailExtent.at (static_cast<size_t> (activeScreen)) = newExtent;
                session.getTextEditor().setViewportLineCount (newExtent);

                jam::debug::Log::write ("DRAIN done historyLines=" + juce::String (doc.getNumLines() - newExtent)
                                        + " liveExtent=" + juce::String (newExtent)
                                        + " totalLines=" + juce::String (doc.getNumLines()));
            }

            session.getTextEditor().calc();
        }
        else if (paramId == id::cursor)
        {
            const juce::Identifier screenId { Map::Screen::getContext()->get (activeScreen) };
            const auto screenNode { state.getChildWithName (screenId) };
            const CursorState cursorState { CursorState::unpack (
                static_cast<int> (jam::Model::getValueFromChildWithID (screenNode, id::cursor).getValue())) };

            session.getTextEditor().setCaretPosition (jam::Cell::Point { cell (cursorState.col), cell (cursorState.row) });
        }
    }
}

void terminal::Display::applyZoom (float) noexcept {}
void terminal::Display::enterSelectionMode() noexcept { state.setModalType (terminal::ModalType::selection); }

void terminal::Display::copySelection() noexcept
{
    // Stub — text extraction pending Screen grid accessor.
    juce::SystemClipboard::copyTextToClipboard ({});
    state.setModalType (terminal::ModalType::none);
}

bool terminal::Display::hasSelection() const noexcept
{
    const auto node { state.getChildWithName (jam::CodeView::properties.at (jam::CodeView::codeViewId)) };
    bool result { false };

    if (node.isValid())
        result = static_cast<int> (node.getProperty (jam::CodeView::properties.at (jam::CodeView::selectionTypeId)))
                 != static_cast<int> (terminal::SelectionType::none);

    return result;
}

// Deferred stubs
bool terminal::Display::isInSelectionMode() const noexcept { return false; }
void terminal::Display::exitSelectionMode() noexcept {}
void terminal::Display::enterOpenFileMode() noexcept {}
void terminal::Display::pasteClipboard() {}
void terminal::Display::writeToPty (const char* data, int len) noexcept { processor.writeInput (data, len); }
int terminal::Display::getHintPage() const noexcept { return 0; }
int terminal::Display::getHintTotalPages() const noexcept { return 0; }

// juce::Component
void terminal::Display::focusGained (FocusChangeType) { session.getTextEditor().grabKeyboardFocus(); }

void terminal::Display::resized()
{
    const auto* appState { AppModel::getContext() };
    const auto contentBounds { getLocalBounds()
                                   .withTrimmedTop (appState->getPaddingTop())
                                   .withTrimmedRight (appState->getPaddingRight())
                                   .withTrimmedBottom (appState->getPaddingBottom())
                                   .withTrimmedLeft (appState->getPaddingLeft()) };

    // setBounds triggers CodeView::resized() -> updateWinsize() -> winsize property -> Session::valueChanged.
    session.getTextEditor().setBounds (contentBounds);

    // Pixel dimensions — needed by SIGWINCH (tty->setWinsize).
    state.setValue (jam::ID::width, contentBounds.getWidth());
    state.setValue (jam::ID::height, contentBounds.getHeight());

    // Display is the sole author of viewport cell dimensions.
    // Uses contentBounds (scrollbar-unaware) — scrollbar is a TextEditor rendering concern.
    const auto& displayNode { attachment->getNode() };
    const int cellW { static_cast<int> (
        jam::Model::getValueFromChildWithID (displayNode, terminal::id::cellWidth).getValue()) };
    const int cellH { static_cast<int> (
        jam::Model::getValueFromChildWithID (displayNode, terminal::id::cellHeight).getValue()) };

    if (cellW > 0 and cellH > 0)
    {
        const auto cellDims { jam::Cell::Rectangle::fromPixel (contentBounds, cellW, cellH) };

        if (cellDims.isValid())
        {
            auto teNode { state.getChildWithName (
                jam::CodeView::properties.at (jam::CodeView::codeViewId)) };

            if (teNode.isValid())
                teNode.setProperty (
                    jam::CodeView::properties.at (jam::CodeView::viewportId), cellDims.pack(), nullptr);
        }
    }
}

// juce::KeyListener
bool terminal::Display::keyPressed (const juce::KeyPress& key, juce::Component*)
{
    const bool handled { input.handleKey (key) };

    if (handled)
        session.getTextEditor().scrollToBottom();

    return handled;
}

// juce::Component mouse events
void terminal::Display::mouseDown (const juce::MouseEvent& event) { mouse.handleDown (event); }
void terminal::Display::mouseDoubleClick (const juce::MouseEvent& event) { mouse.handleDoubleClick (event); }
void terminal::Display::mouseDrag (const juce::MouseEvent& event) { mouse.handleDrag (event); }
void terminal::Display::mouseUp (const juce::MouseEvent& event) { mouse.handleUp (event); }
void terminal::Display::mouseMove (const juce::MouseEvent& event) { mouse.handleMove (event, *this); }
