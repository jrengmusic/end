#include "TerminalDisplay.h"
#include <jam_tui/jam_tui.h>

Terminal::Display::Display (Terminal::Processor& processorToUse)
    : processor (processorToUse)
    , state (processorToUse.getState())
    , grid (processorToUse.getGrid())
    , vblank (this, [this] { onVBlank(); })
{
    addAndMakeVisible (screen);
    screen.addKeyListener (this);
    state.get().addListener (this);

    applyConfig();
}

Terminal::Display::~Display()
{
    screen.removeKeyListener (this);
}

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
    screen.setScrollBarThickness (config.display.scrollbarWidth);
    screen.setScrollbackLines (config.nexus.terminal.scrollbackLines);

    state.setCellMetrics (font.cellWidth, font.cellHeight, font.baseline, font.fontSize);
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
void Terminal::Display::focusGained (FocusChangeType)
{
    screen.grabKeyboardFocus();
}

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
    const int newCols   { cellArea.width };
    const int newRows   { cellArea.height };

    if (newCols > 0 and newRows > 0)
    {
        if (newCols != lastCols or newRows != lastRows)
        {
            lastCols = newCols;
            lastRows = newRows;

            state.setValue (Terminal::ID::cols, newCols);
            state.setValue (Terminal::ID::visibleRows, newRows);

            if (processor.events.contains (Terminal::ID::terminalResize))
                processor.events.get (Terminal::ID::terminalResize, int (newCols), int (newRows),
                                      int (contentBounds.getWidth()), int (contentBounds.getHeight()));
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
}

void Terminal::Display::onVBlank()
{
    const bool stateDirty { state.consumeSnapshotDirty() };

    if (stateDirty)
        state.refresh();

    {
        const int activeScreen { state.getActiveScreen() };
        const int numRows { grid.getNumRows (activeScreen) };
        const int numCols { grid.getNumCols (activeScreen) };

        if (numRows > 0 and numCols > 0)
        {
            for (int r { 0 }; r < numRows; ++r)
            {
                const auto* row { grid.getReadPointer (activeScreen, r) };

                if (row != nullptr)
                    screen.setVisibleRow (r, row, numCols);
            }
        }

        // Caret position.
        {
            const int cols { grid.getNumCols (activeScreen) };
            const int rows { grid.getNumRows (activeScreen) };

            if (cols > 0 and rows > 0)
            {
                const int cursorCol { juce::jlimit (0, cols - 1, state.getCursorCol()) };
                const int cursorRow { juce::jlimit (0, rows - 1, state.getCursorRow()) };
                screen.setCaretPosition (cursorCol, cursorRow);
            }
        }
    }
}
