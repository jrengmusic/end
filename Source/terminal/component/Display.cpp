#include "Display.h"

terminal::Display::Display (terminal::Processor& processorToUse)
    : processor (processorToUse)
    , state (processorToUse.getState())
    , screen (Display::createAndAttachState (processorToUse.getState(), normalScreen, alternateScreen),
              processorToUse.getBuffer())
    , linkManager (processorToUse.getState(),
                   [&processorToUse] (const char* data, int len)
                   {
                       processorToUse.writeInput (data, len);
                   })
    , input (processorToUse, linkManager)
    , mouse (processorToUse, 0, 0, linkManager)
{
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

    addAndMakeVisible (screen);
    screen.addKeyListener (this);

    AppState::getContext()->get().addListener (this);
    applyFromAppState();

    screen.transitioner.addTrigger<cell, cell> (
        terminal::id::resizeStart,
        [this] (cell targetCols, cell targetRows)
        {
            screen.setCaretVisible (false);

            // Read old values from State VT BEFORE overwriting with target.
            const cell oldVisibleRows { static_cast<int> (
                jam::ValueTree::getValueFromChildWithID (state.get(), terminal::id::visibleRows).getValue()) };

            // Write target dimensions to State — DST drives the moving target.
            state.setValue (terminal::id::cols, targetCols.value);
            state.setValue (terminal::id::visibleRows, targetRows.value);

            jam::debug::Log::write ("[DST::trigger] targetCols=" + juce::String (targetCols.value)                  // DIAG
                                    + " targetRows=" + juce::String (targetRows.value)                                // DIAG
                                    + " oldVisRows=" + juce::String (oldVisibleRows.value)                            // DIAG
                                    + " prevNumRows=" + juce::String (screen.transitioner.previous.getNumRows()));   // DIAG

            // Reflow from DST snapshot — skipped on cold start (snapshot empty).
            if (screen.transitioner.previous.getNumRows() > 0)
            {
                const int scrollbackLines { static_cast<int> (
                    jam::ValueTree::getValueFromChildWithID (AppState::getContext()->get(), app::id::scrollbackLines).getValue()) };

                const juce::Identifier normalId { Map::Screen::getContext()->get (Map::Screen::normal) };
                const juce::Identifier alternateId { Map::Screen::getContext()->get (Map::Screen::alternate) };
                const int numHistoryNormal { static_cast<int> (
                    jam::ValueTree::getValueFromChildWithID (state.get().getChildWithName (normalId), terminal::id::numRows).getValue()) };
                const int numHistoryAlternate { static_cast<int> (
                    jam::ValueTree::getValueFromChildWithID (state.get().getChildWithName (alternateId), terminal::id::numRows).getValue()) };
                const int cursorRow { static_cast<int> (
                    jam::ValueTree::getValueFromChildWithID (state.get().getChildWithName (normalId), terminal::id::cursorRow).getValue()) };

                // Allocate reflowed storage at new dimensions.
                const int ringSize { juce::nextPowerOfTwo (scrollbackLines + targetRows.value) };
                screen.reflowedContent.setSize (2, ringSize, targetCols.value);

                // Reflow DST snapshot into reflowedContent; capture history count for onStop.
                screen.reflowedHistoryNormal = terminal::Screen::reflow (screen.reflowedContent,
                                                                         screen.transitioner.previous,
                                                                         scrollbackLines,
                                                                         oldVisibleRows.value,
                                                                         targetRows.value,
                                                                         numHistoryNormal,
                                                                         numHistoryAlternate,
                                                                         cursorRow);
            }
        });

    screen.transitioner.onStop = [this]
    {
        auto& buf { processor.getBuffer() };
        const int ringSize { screen.reflowedContent.getNumRows() };
        const int numCols { screen.reflowedContent.getNumCols() };

        // NOW safe to resize live buffer — reflowed content is in screen.reflowedContent.
        buf.setSize (2, ringSize, numCols);

        // Copy reflowed content to live buffer.
        for (int ch { 0 }; ch < 2; ++ch)
        {
            for (int r { 0 }; r < ringSize; ++r)
                buf.copyFrom (ch, r, screen.reflowedContent, ch, r);
        }

        // Write new history count to State.
        const juce::Identifier normalId { Map::Screen::getContext()->get (Map::Screen::normal) };
        state.setValue (normalId, terminal::id::numRows, screen.reflowedHistoryNormal);

        processor.finishResize();
        screen.setCaretVisible (true);
    };
}

terminal::State& terminal::Display::createAndAttachState (terminal::State& stateToSeed,
                                                      juce::ValueTree& normalScreenNode,
                                                      juce::ValueTree& alternateScreenNode) noexcept
{
    const auto seedNode = [] (juce::ValueTree& node)
    {
        node.setProperty (terminal::id::cursorRow,     0, nullptr);
        node.setProperty (terminal::id::cursorCol,     0, nullptr);
        node.setProperty (terminal::id::cursorVisible, 1, nullptr);
        node.setProperty (terminal::id::cursorShape,   0, nullptr);
        node.setProperty (terminal::id::cursorColor,  -1, nullptr);
        node.setProperty (terminal::id::keyboardFlags, 0, nullptr);
        node.setProperty (terminal::id::numRows,       0, nullptr);
        node.setProperty (terminal::id::scrollOffset,  0, nullptr);
        node.setProperty (terminal::id::screenDirty,   0, nullptr);
    };

    normalScreenNode = juce::ValueTree (terminal::id::NORMAL);
    seedNode (normalScreenNode);

    alternateScreenNode = juce::ValueTree (terminal::id::ALTERNATE);
    seedNode (alternateScreenNode);

    stateToSeed.get().appendChild (normalScreenNode, nullptr);
    stateToSeed.get().appendChild (alternateScreenNode, nullptr);

    return stateToSeed;
}

terminal::Display::~Display()
{
    AppState::getContext()->get().removeListener (this);
    screen.removeKeyListener (this);
    state.get().removeChild (normalScreen, nullptr);
    state.get().removeChild (alternateScreen, nullptr);
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

    screen.setFont (font);
    screen.setCaretChar (jam::toChar (appState->getCursorCodepoint()));
    screen.setCaretShape (appState->getCursorStyle());
    screen.setCaretBlinkRate (appState->getCursorBlinkInterval());


    attachment->setValue (terminal::id::cellWidth,  font.bounds.width);
    attachment->setValue (terminal::id::cellHeight, font.bounds.height);
    attachment->setValue (terminal::id::baseline,   font.baseline);
    attachment->setValue (terminal::id::fontSize,   static_cast<int> (font.fontSize));

    mouse.setCellSize (font.bounds.width, font.bounds.height);
    input.buildKeyMap();
}
void terminal::Display::valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&)
{
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
void terminal::Display::focusGained (FocusChangeType) { screen.grabKeyboardFocus(); }

void terminal::Display::resized()
{
    const auto* appState { AppState::getContext() };
    const auto contentBounds { getLocalBounds()
                                   .withTrimmedTop (appState->getPaddingTop())
                                   .withTrimmedRight (appState->getPaddingRight())
                                   .withTrimmedBottom (appState->getPaddingBottom())
                                   .withTrimmedLeft (appState->getPaddingLeft()) };

    screen.setBounds (contentBounds);

    // Compute cell dimensions from pixel bounds — Display is sole dimension author.
    // Cell pixel dims from DISPLAY attachment node (written by applyFromAppState).
    const int cellWidth { static_cast<int> (jam::ValueTree::getValueFromChildWithID (attachment->getNode(), terminal::id::cellWidth).getValue()) };
    const int cellHeight { static_cast<int> (jam::ValueTree::getValueFromChildWithID (attachment->getNode(), terminal::id::cellHeight).getValue()) };

    if (cellWidth > 0 and cellHeight > 0 and contentBounds.getWidth() > 0 and contentBounds.getHeight() > 0)
    {
        // Predict scrollbar visibility: content exceeds viewport when history exists.
        const juce::Identifier normalId { Map::Screen::getContext()->get (Map::Screen::normal) };
        const int numRows { static_cast<int> (
            jam::ValueTree::getValueFromChildWithID (state.get().getChildWithName (normalId), terminal::id::numRows).getValue()) };
        const int scrollbarWidth { numRows > 0 ? screen.getLookAndFeel().getDefaultScrollbarWidth() : 0 };

        const int availableWidth { contentBounds.getWidth() - scrollbarWidth };
        const auto gridRect { jam::Cell::Rectangle (jam::Bounds { cellWidth, cellHeight },
                                                     juce::Rectangle<int> { 0, 0, availableWidth, contentBounds.getHeight() }) };

        const cell newCols { gridRect.getWidth() };
        const cell newRows { gridRect.getHeight() };

        if (newCols.value > 0 and newRows.value > 0)
        {
            jam::debug::Log::write ("[DISPLAY::resized] cols=" + juce::String (newCols.value)                          // DIAG
                                    + " rows=" + juce::String (newRows.value)                                           // DIAG
                                    + " availW=" + juce::String (availableWidth) + " scrollbar=" + juce::String (scrollbarWidth) // DIAG
                                    + " contentBounds=" + juce::String (contentBounds.getWidth()) + "x" + juce::String (contentBounds.getHeight())); // DIAG

            // Trigger DST — Screen owns transitioner, Display wires it.
            // DST trigger writes target dims to State (moving target during animation).
            screen.transitioner.liveRows = { numRows + newRows.value, newRows.value };
            screen.transitioner.set (terminal::id::resizeStart, newCols, newRows);
        }
    }

    // Pixel dimensions — needed by SIGWINCH (tty->platformResize).
    state.setValue (jam::ID::width, contentBounds.getWidth());
    state.setValue (jam::ID::height, contentBounds.getHeight());
}

// juce::KeyListener
bool terminal::Display::keyPressed (const juce::KeyPress& key, juce::Component*)
{
    return input.handleKey (key);
}

// juce::Component mouse events
void terminal::Display::mouseDown (const juce::MouseEvent& event) { mouse.handleDown (event); }
void terminal::Display::mouseDoubleClick (const juce::MouseEvent& event) { mouse.handleDoubleClick (event); }
void terminal::Display::mouseDrag (const juce::MouseEvent& event) { mouse.handleDrag (event); }
void terminal::Display::mouseUp (const juce::MouseEvent& event) { mouse.handleUp (event); }
void terminal::Display::mouseMove (const juce::MouseEvent& event) { mouse.handleMove (event, *this); }

void terminal::Display::mouseWheelMove (const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    const int activeScreenIndex { state.getActiveScreen() };
    const juce::Identifier wheelScreenId { Map::Screen::getContext()->get (activeScreenIndex) };
    auto wheelScreenNode { state.get().getChildWithName (wheelScreenId) };
    const int currentOffset { static_cast<int> (jam::ValueTree::getValueFromChildWithID (wheelScreenNode, terminal::id::scrollOffset).getValue()) };
    const int numRows { static_cast<int> (jam::ValueTree::getValueFromChildWithID (wheelScreenNode, terminal::id::numRows).getValue()) };

    mouse.handleWheel (event, wheel, [this, activeScreenIndex, currentOffset, numRows] (int delta)
    {
        const int newOffset { juce::jlimit (0, numRows, currentOffset + delta) };
        const juce::Identifier writeScreenId { Map::Screen::getContext()->get (activeScreenIndex) };
        auto writeScreenNode { state.get().getChildWithName (writeScreenId) };
        auto paramNode { jam::ValueTree::getChildWithID (writeScreenNode, terminal::id::scrollOffset.toString()) };
        paramNode.setProperty (terminal::id::value, newOffset, nullptr);
    });
}
