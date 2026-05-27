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
    // Cache NORMAL/ALTERNATE screen nodes from State (built by State::buildLayout).
    normalScreen = state.get().getChildWithName (terminal::id::NORMAL);
    alternateScreen = state.get().getChildWithName (terminal::id::ALTERNATE);

    // Parent Screen (owned by Session) for rendering via the Component hierarchy.
    addAndMakeVisible (session.getScreen());
    session.getScreen().addKeyListener (this);

    // Graft DISPLAY node via ComponentAttachment — registerNodeAtomics fires on appendChild
    // and creates Parameter<int> entries in the DISPLAY group for font metrics.
    attachment = std::make_unique<jam::ComponentAttachment> (
        state,
        terminal::id::DISPLAY,
        std::initializer_list<jam::ComponentAttachment::Property> {
            { terminal::id::cellWidth,  0 },
            { terminal::id::cellHeight, 0 },
            { terminal::id::baseline,   0 },
            { terminal::id::fontSize,   0 }
        });

    AppState::getContext()->get().addListener (this);

    // Listen to per-session terminal state for content updates (screenDirty).
    terminalState = state.get();
    terminalState.addListener (this);

    applyFromAppState();
}

terminal::State& terminal::Display::createAndAttachState (terminal::State& stateToSeed,
                                                      juce::ValueTree& normalScreenNode,
                                                      juce::ValueTree& alternateScreenNode) noexcept
{
    normalScreenNode = stateToSeed.get().getChildWithName (terminal::id::NORMAL);
    alternateScreenNode = stateToSeed.get().getChildWithName (terminal::id::ALTERNATE);
    return stateToSeed;
}

terminal::Display::~Display()
{
    terminalState.removeListener (this);
    AppState::getContext()->get().removeListener (this);
    session.getScreen().removeKeyListener (this);
}

// PaneComponent
juce::String terminal::Display::getPaneType() const noexcept { return Map::PaneType::getContext()->get (Map::PaneType::terminal); }
void terminal::Display::switchRenderer (app::RendererType) noexcept {}
juce::ValueTree terminal::Display::getValueTree() noexcept { return state.get(); }
void terminal::Display::applyFromAppState() noexcept
{
    const auto* appState { AppState::getContext() };

    const jam::Font font { appState->getFontFamily(),
                           appState->getFontSize(),
                           static_cast<float> (appState->getCellWidth()),
                           static_cast<float> (appState->getLineHeight()) };

    session.getScreen().setFont (font);
    session.getScreen().setCaretChar (jam::toChar (appState->getCursorCodepoint()));
    session.getScreen().setCaretShape (appState->getCursorStyle());
    session.getScreen().setCaretBlinkRate (appState->getCursorBlinkInterval());

    attachment->setValue (terminal::id::cellWidth,  font.bounds.width);
    attachment->setValue (terminal::id::cellHeight, font.bounds.height);
    attachment->setValue (terminal::id::baseline,   font.baseline);
    attachment->setValue (terminal::id::fontSize,   static_cast<int> (font.fontSize));

    mouse.setCellSize (font.bounds.width, font.bounds.height);
    input.buildKeyMap();
}
void terminal::Display::valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property)
{
    bool isContentUpdate { false };

    if (property == id::value and tree.getType() == jam::ValueTree::PARAM)
    {
        const juce::Identifier paramId { tree.getProperty (id::id).toString() };

        if (paramId == id::screenDirty)
        {
            isContentUpdate = true;
            const int activeScreen { state.getActiveScreen() };
            session.getScreen().setWrapEnabled (activeScreen == Map::Screen::normal);
            session.getScreen().setText (processor.getTextLineArray());
        }
    }

    if (not isContentUpdate)
        applyFromAppState();
}

void terminal::Display::applyZoom (float) noexcept {}
void terminal::Display::enterSelectionMode() noexcept
{
    state.setModalType (terminal::ModalType::selection);
}

void terminal::Display::copySelection() noexcept
{
    // Stub — text extraction pending Screen grid accessor.
    juce::SystemClipboard::copyTextToClipboard ({});
    state.setModalType (terminal::ModalType::none);
}

bool terminal::Display::hasSelection() const noexcept
{
    const auto node { state.get().getChildWithName (jam::TextEditor::properties.at (jam::TextEditor::textEditorId)) };
    bool result { false };

    if (node.isValid())
        result = static_cast<int> (node.getProperty (jam::TextEditor::properties.at (jam::TextEditor::selectionTypeId))) != static_cast<int> (terminal::SelectionType::none);

    return result;
}

// Deferred stubs
bool terminal::Display::isInSelectionMode() const noexcept { return false; }
void terminal::Display::exitSelectionMode() noexcept {}
void terminal::Display::enterOpenFileMode() noexcept {}
void terminal::Display::pasteClipboard() {}
void terminal::Display::writeToPty (const char* data, int len) noexcept
{
    processor.writeInput (data, len);
}
int terminal::Display::getHintPage() const noexcept { return 0; }
int terminal::Display::getHintTotalPages() const noexcept { return 0; }

// juce::Component
void terminal::Display::focusGained (FocusChangeType) { session.getScreen().grabKeyboardFocus(); }

void terminal::Display::resized()
{
    const auto* appState { AppState::getContext() };
    const auto contentBounds { getLocalBounds()
                                   .withTrimmedTop (appState->getPaddingTop())
                                   .withTrimmedRight (appState->getPaddingRight())
                                   .withTrimmedBottom (appState->getPaddingBottom())
                                   .withTrimmedLeft (appState->getPaddingLeft()) };

    // setBounds triggers Screen::resized() -> updateWinsize() -> winsize property -> Session::valueChanged.
    session.getScreen().setBounds (contentBounds);

    // Pixel dimensions — needed by SIGWINCH (tty->setWinsize).
    state.setValue (jam::ID::width, contentBounds.getWidth());
    state.setValue (jam::ID::height, contentBounds.getHeight());
}

// juce::KeyListener
bool terminal::Display::keyPressed (const juce::KeyPress& key, juce::Component*)
{
    const bool handled { input.handleKey (key) };

    if (handled)
        session.getScreen().scrollToBottom();

    return handled;
}

// juce::Component mouse events
void terminal::Display::mouseDown (const juce::MouseEvent& event) { mouse.handleDown (event); }
void terminal::Display::mouseDoubleClick (const juce::MouseEvent& event) { mouse.handleDoubleClick (event); }
void terminal::Display::mouseDrag (const juce::MouseEvent& event) { mouse.handleDrag (event); }
void terminal::Display::mouseUp (const juce::MouseEvent& event) { mouse.handleUp (event); }
void terminal::Display::mouseMove (const juce::MouseEvent& event) { mouse.handleMove (event, *this); }
