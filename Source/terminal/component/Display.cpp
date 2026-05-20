#include "Display.h"

terminal::Display::Display (terminal::Processor& processorToUse)
    : processor (processorToUse)
    , state (processorToUse.getState())
    , screen (processorToUse.getState(), processorToUse.getGrid())
    , linkManager (processorToUse.getState(),
                   [&processorToUse] (const char* data, int len)
                   {
                       processorToUse.writeInput (data, len);
                   })
    , input (processorToUse, linkManager)
    , mouse (processorToUse, 0, 0, linkManager)
{
    addAndMakeVisible (screen);
    screen.addKeyListener (this);

    // Seed DISPLAY node with zero-valued properties before graft.
    // registerNodeAtomics (called on appendChild) consumes these and
    // creates Parameter<int> entries in the DISPLAY group.
    displayNode.setProperty (terminal::id::cellWidth,  0, nullptr);
    displayNode.setProperty (terminal::id::cellHeight, 0, nullptr);
    displayNode.setProperty (terminal::id::baseline,   0, nullptr);
    displayNode.setProperty (terminal::id::fontSize,   0, nullptr);
    state.get().appendChild (displayNode, nullptr);

    applyConfig();
}

terminal::Display::~Display() { screen.removeKeyListener (this); }

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


    displayNode.setProperty (terminal::id::cellWidth,  font.cellWidth, nullptr);
    displayNode.setProperty (terminal::id::cellHeight, font.cellHeight, nullptr);
    displayNode.setProperty (terminal::id::baseline,   font.baseline, nullptr);
    displayNode.setProperty (terminal::id::fontSize,   static_cast<int> (font.fontSize), nullptr);

    mouse.setCellSize (font.cellWidth, font.cellHeight);
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
    const auto node { state.get().getChildWithName (jam::ID::textEditor) };
    bool result { false };

    if (node.isValid())
        result = static_cast<int> (node.getProperty (jam::ID::selectionType)) != static_cast<int> (terminal::SelectionType::none);

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
    const int cellWidth  { displayNode.getProperty (terminal::id::cellWidth) };
    const int cellHeight { displayNode.getProperty (terminal::id::cellHeight) };

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
    const int currentOffset { state.getScrollOffset (activeScreenIndex) };
    const int numRows { state.getNumRows (activeScreenIndex) };

    mouse.handleWheel (event, wheel, [this, activeScreenIndex, currentOffset, numRows] (int delta)
    {
        const int newOffset { juce::jlimit (0, numRows, currentOffset + delta) };
        state.setScrollOffset (activeScreenIndex, newOffset);
    });
}
