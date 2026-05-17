#include "TerminalDisplay.h"
#include <jam_tui/jam_tui.h>

Terminal::Display::Display (Terminal::Processor& processorToUse)
    : processor (processorToUse)
    , state (processorToUse.getState())
    , grid (processorToUse.getGrid())
{
    addAndMakeVisible (screen);
    screen.addKeyListener (this);
    state.get().addListener (this);

    // Seed DISPLAY node with zero-valued properties before graft.
    // registerNodeAtomics (called on appendChild) consumes these and
    // creates Parameter<int> entries in the DISPLAY group.
    displayNode.setProperty (Terminal::ID::cellWidth,  0, nullptr);
    displayNode.setProperty (Terminal::ID::cellHeight, 0, nullptr);
    displayNode.setProperty (Terminal::ID::baseline,   0, nullptr);
    displayNode.setProperty (Terminal::ID::fontSize,   0, nullptr);
    state.get().appendChild (displayNode, nullptr);

    applyConfig();
}

Terminal::Display::~Display() { screen.removeKeyListener (this); }

// PaneComponent
juce::String Terminal::Display::getPaneType() const noexcept { return App::ID::paneTypeTerminal; }
void Terminal::Display::switchRenderer (App::RendererType) noexcept {}
juce::ValueTree Terminal::Display::getValueTree() noexcept { return state.get(); }
void Terminal::Display::applyConfig() noexcept
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
    screen.setScrollbackLines (config.nexus.terminal.scrollbackLines);

    state.storeValue (Terminal::ID::DISPLAY, Terminal::ID::cellWidth,  font.cellWidth);
    state.storeValue (Terminal::ID::DISPLAY, Terminal::ID::cellHeight, font.cellHeight);
    state.storeValue (Terminal::ID::DISPLAY, Terminal::ID::baseline,   font.baseline);
    state.storeValue (Terminal::ID::DISPLAY, Terminal::ID::fontSize,   static_cast<int> (font.fontSize));
    state.refresh();

    resized();
}
void Terminal::Display::applyZoom (float) noexcept {}
void Terminal::Display::enterSelectionMode() noexcept {}
void Terminal::Display::copySelection() noexcept {}
bool Terminal::Display::hasSelection() const noexcept { return false; }

// Deferred stubs
bool Terminal::Display::isInSelectionMode() const noexcept { return false; }
void Terminal::Display::exitSelectionMode() noexcept {}
void Terminal::Display::enterOpenFileMode() noexcept {}
void Terminal::Display::pasteClipboard() {}
void Terminal::Display::writeToPty (const char* data, int len) noexcept
{
    if (processor.events.contains (Terminal::ID::writeInput))
        processor.events.get (Terminal::ID::writeInput, data, len);
}
int Terminal::Display::getHintPage() const noexcept { return 0; }
int Terminal::Display::getHintTotalPages() const noexcept { return 0; }

// juce::Component
void Terminal::Display::focusGained (FocusChangeType) { screen.grabKeyboardFocus(); }

void Terminal::Display::resized()
{
    const auto contentBounds { getLocalBounds()
                                   .withTrimmedTop (config.nexus.terminal.paddingTop)
                                   .withTrimmedRight (config.nexus.terminal.paddingRight)
                                   .withTrimmedBottom (config.nexus.terminal.paddingBottom)
                                   .withTrimmedLeft (config.nexus.terminal.paddingLeft) };

    screen.setBounds (contentBounds);
    updateDimensions (contentBounds);
}

void Terminal::Display::updateDimensions (const juce::Rectangle<int>& contentBounds) noexcept
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

            state.setValue (Terminal::ID::cols, newCols.value);
            state.setValue (Terminal::ID::visibleRows, newRows.value);

            if (processor.events.contains (Terminal::ID::terminalResize))
                processor.events.get (Terminal::ID::terminalResize,
                                      newCols.value,
                                      newRows.value,
                                      int (contentBounds.getWidth()),
                                      int (contentBounds.getHeight()));
        }
    }
}

// juce::KeyListener
bool Terminal::Display::keyPressed (const juce::KeyPress& key, juce::Component*)
{
    if (processor.events.contains (Terminal::ID::writeInput))
    {
        const auto encoded { processor.encodeKeyPress (key) };

        if (encoded.isNotEmpty())
            processor.events.get (Terminal::ID::writeInput, encoded.toRawUTF8(), int (encoded.getNumBytesAsUTF8()));
    }

    return true;
}

// juce::ValueTree::Listener
void Terminal::Display::valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&)
{
    state.setSnapshotDirty();
    screen.setActiveScreen (state.getActiveScreen());
    screen.setCaretPosition (state.getCursorCol(), state.getCursorRow());

    const int historyRows { state.getHistoryRows() };
    const int delta { historyRows - previousHistoryRows };

    if (delta > 0)
    {
        screen.setText (grid.getBuffer(), Screen::Map::normal,
                        grid.getHead (Screen::Map::normal),
                        { previousHistoryRows, historyRows });
        previousHistoryRows = historyRows;
    }

}

std::function<void()> Terminal::Display::onVBlank() noexcept
{
    return [this]
    {
        if (state.consumeSnapshotDirty())
            state.refresh();

        const int activeScreen { state.getActiveScreen() };
        const int numRows { grid.getNumRows (activeScreen) };

        screen.setText (grid.getBuffer(), activeScreen,
                        grid.getHead (activeScreen),
                        { 0, numRows });
    };
}
