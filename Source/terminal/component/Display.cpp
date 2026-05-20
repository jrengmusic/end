#include "Display.h"

terminal::Display::Display (terminal::Processor& processorToUse)
    : processor (processorToUse)
    , state (processorToUse.getState())
    , screen (processorToUse.getState(), processorToUse.getGrid())
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
    screen.setScrollBarThickness (config.display.scrollbarWidth);

    state.storeValue (terminal::id::DISPLAY, terminal::id::cellWidth,  font.cellWidth);
    state.storeValue (terminal::id::DISPLAY, terminal::id::cellHeight, font.cellHeight);
    state.storeValue (terminal::id::DISPLAY, terminal::id::baseline,   font.baseline);
    state.storeValue (terminal::id::DISPLAY, terminal::id::fontSize,   static_cast<int> (font.fontSize));
    state.refresh();

    resized();
}
void terminal::Display::applyZoom (float) noexcept {}
void terminal::Display::enterSelectionMode() noexcept {}
void terminal::Display::copySelection() noexcept {}
bool terminal::Display::hasSelection() const noexcept { return false; }

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
    const auto cellArea { screen.getCellArea() };
    const cell newCols { cellArea.width };
    const cell newRows { cellArea.height };

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

// juce::KeyListener
bool terminal::Display::keyPressed (const juce::KeyPress& key, juce::Component*)
{
    static constexpr int live { 0 };
    state.setScrollOffset (state.getActiveScreen(), live);

    const auto encoded { processor.encodeKeyPress (key) };

    if (encoded.isNotEmpty())
        processor.writeInput (encoded.toRawUTF8(), int (encoded.getNumBytesAsUTF8()));

    return true;
}
