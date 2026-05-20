#include "Display.h"

terminal::Display::Display (terminal::Processor& processorToUse)
    : processor (processorToUse)
    , state (processorToUse.getState())
    , screen (Display::seedScreenNodes (processorToUse.getState(), normalScreen, alternateScreen),
              processorToUse.getGrid())
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

    applyConfig();
}

terminal::State& terminal::Display::seedScreenNodes (terminal::State& stateToSeed,
                                                      juce::ValueTree& normalScreenNode,
                                                      juce::ValueTree& alternateScreenNode) noexcept
{
    normalScreenNode = juce::ValueTree (terminal::id::NORMAL);
    normalScreenNode.setProperty (terminal::id::cursorRow,     0, nullptr);
    normalScreenNode.setProperty (terminal::id::cursorCol,     0, nullptr);
    normalScreenNode.setProperty (terminal::id::cursorVisible, 1, nullptr);
    normalScreenNode.setProperty (terminal::id::cursorShape,   0, nullptr);
    normalScreenNode.setProperty (terminal::id::cursorColor,  -1, nullptr);
    normalScreenNode.setProperty (terminal::id::keyboardFlags, 0, nullptr);
    normalScreenNode.setProperty (terminal::id::numRows,       0, nullptr);
    normalScreenNode.setProperty (terminal::id::scrollOffset,  0, nullptr);
    normalScreenNode.setProperty (terminal::id::screenDirty,   0, nullptr);

    alternateScreenNode = juce::ValueTree (terminal::id::ALTERNATE);
    alternateScreenNode.setProperty (terminal::id::cursorRow,     0, nullptr);
    alternateScreenNode.setProperty (terminal::id::cursorCol,     0, nullptr);
    alternateScreenNode.setProperty (terminal::id::cursorVisible, 1, nullptr);
    alternateScreenNode.setProperty (terminal::id::cursorShape,   0, nullptr);
    alternateScreenNode.setProperty (terminal::id::cursorColor,  -1, nullptr);
    alternateScreenNode.setProperty (terminal::id::keyboardFlags, 0, nullptr);
    alternateScreenNode.setProperty (terminal::id::numRows,       0, nullptr);
    alternateScreenNode.setProperty (terminal::id::scrollOffset,  0, nullptr);
    alternateScreenNode.setProperty (terminal::id::screenDirty,   0, nullptr);

    stateToSeed.get().appendChild (normalScreenNode, nullptr);
    stateToSeed.get().appendChild (alternateScreenNode, nullptr);

    return stateToSeed;
}

terminal::Display::~Display()
{
    screen.removeKeyListener (this);
    state.get().removeChild (normalScreen, nullptr);
    state.get().removeChild (alternateScreen, nullptr);
}

// PaneComponent
juce::String terminal::Display::getPaneType() const noexcept { return app::id::paneTypeTerminal; }
void terminal::Display::switchRenderer (app::RendererType) noexcept {}
juce::ValueTree terminal::Display::getValueTree() noexcept { return state.get(); }
void terminal::Display::applyConfig() noexcept
{
    const jam::Font font { config.display.font.family,
                           config.dpiCorrectedFontSize(),
                           config.display.font.cellWidth,
                           config.display.font.lineHeight };

    screen.setFont (font);
    screen.setCaretChar (jam::toChar (config.display.cursor.codepoint));
    screen.setCaretShape (config.display.cursor.style);
    screen.setCaretBlinkRate (config.display.cursor.blinkInterval);


    attachment->setValue (terminal::id::cellWidth,  font.bounds.width);
    attachment->setValue (terminal::id::cellHeight, font.bounds.height);
    attachment->setValue (terminal::id::baseline,   font.baseline);
    attachment->setValue (terminal::id::fontSize,   static_cast<int> (font.fontSize));

    mouse.setCellSize (font.bounds.width, font.bounds.height);
    input.buildKeyMap (config.keys.selection);

    resized();
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
    const auto contentBounds { getLocalBounds()
                                   .withTrimmedTop (config.nexus.terminal.paddingTop)
                                   .withTrimmedRight (config.nexus.terminal.paddingRight)
                                   .withTrimmedBottom (config.nexus.terminal.paddingBottom)
                                   .withTrimmedLeft (config.nexus.terminal.paddingLeft) };

    screen.setBounds (contentBounds);
    updateDimensions (contentBounds);
}

void terminal::Display::updateDimensions (const juce::Rectangle<int>& contentBounds) noexcept
{
    const int cellWidth  { static_cast<int> (jam::ValueTree::getValueFromChildWithID (attachment->getNode(), terminal::id::cellWidth).getValue()) };
    const int cellHeight { static_cast<int> (jam::ValueTree::getValueFromChildWithID (attachment->getNode(), terminal::id::cellHeight).getValue()) };

    if (cellWidth > 0 and cellHeight > 0)
    {
        const cell newCols { contentBounds.getWidth()  / cellWidth };
        const cell newRows { contentBounds.getHeight() / cellHeight };

        if (newCols.value > 0 and newRows.value > 0)
        {
            if (newCols != lastCols or newRows != lastRows)
            {
                lastCols = newCols;
                lastRows = newRows;

                state.setValue (terminal::id::cols, newCols.value);
                state.setValue (terminal::id::visibleRows, newRows.value);
                state.setValue (jam::ID::width, int (contentBounds.getWidth()));
                state.setValue (jam::ID::height, int (contentBounds.getHeight()));
            }
        }
    }
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
