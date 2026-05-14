#include "TerminalDisplay.h"
#include <jam_tui/jam_tui.h>

Terminal::Display::Display (Terminal::Processor& processorToUse)
    : processor (processorToUse)
    , state (processorToUse.getState())
    , grid (processorToUse.getGrid())
    , vblank (this, [this] { onVBlank(); })
{
    addAndMakeVisible (screen);
    screen.setScrollBarThickness (config.display.scrollbarWidth);
    screen.setScrollbackLines (config.nexus.terminal.scrollbackLines);

    screen.addKeyListener (this);
    state.get().addListener (this);
}

Terminal::Display::~Display()
{
    cancelPendingUpdate();
    screen.removeKeyListener (this);
}

// PaneComponent
juce::String Terminal::Display::getPaneType() const noexcept { return App::ID::paneTypeTerminal; }
void Terminal::Display::switchRenderer (App::RendererType) noexcept {}
juce::ValueTree Terminal::Display::getValueTree() noexcept { return state.get(); }
void Terminal::Display::applyConfig() noexcept {}
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

    const float fontSize { config.dpiCorrectedFontSize() };

    // Font set once per config change — not on every resize.
    if (fontSize != lastFontSize)
    {
        lastFontSize = fontSize;

        lastFont = jam::Font { config.display.font.family, fontSize,
                               config.display.font.cellWidth, config.display.font.lineHeight };

        screen.setFont (lastFont);
        screen.setCaretChar (jam::toChar (config.display.cursor.codepoint));
        screen.setCaretShape (config.display.cursor.style);
    }

    screen.setBounds (contentBounds);

    // PROJECTION — read logical dims from Screen (computed in TextEditor::resized()).
    const auto cellArea { screen.getCellArea() };
    const int newCols   { cellArea.width };
    const int newRows   { cellArea.height };

    if (newCols > 0 and newRows > 0)
    {
        // Resize debounce — only act when grid dimensions actually change.
        if (newCols != lastCols or newRows != lastRows)
        {
            lastCols = newCols;
            lastRows = newRows;

            processor.getState().setCellMetrics (lastFont.cellWidth, lastFont.cellHeight, lastFont.baseline, fontSize);
            processor.getState().refresh();

            // Top-down: Display writes State → Processor::valueTreePropertyChanged → Video.
            processor.getState().setValue (Terminal::ID::cols, newCols);
            processor.getState().setValue (Terminal::ID::visibleRows, newRows);

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

    const int activeScreen { state.getActiveScreen() };

    if (activeScreen != lastActiveScreen)
    {
        lastActiveScreen = activeScreen;
        screen.setActiveScreen (activeScreen);
    }
}

// juce::AsyncUpdater
void Terminal::Display::handleAsyncUpdate()
{
}

void Terminal::Display::onVBlank()
{
    const bool stateDirty { state.consumeSnapshotDirty() };

    if (stateDirty)
        state.refresh();

    // Grid dirty-row drain — transfers cells from Grid access buffer to Screen
    const int scrolledRows { grid.getNumScrolledRows() };
    const uint64_t dirtyMask { grid.consumeDirtyRows() };
    const bool gridDirty { scrolledRows > 0 or dirtyMask != 0 };

    if (gridDirty)
    {
        // 1. Drain all scroll-off rows
        if (scrolledRows > 0)
        {
            const int numCols { grid.getNumCols() };

            for (int i { 0 }; i < scrolledRows; ++i)
            {
                const jam::Cell* row { grid.getScrolledReadPointer (i) };
                screen.append (&row, 1, numCols);
            }

            grid.consumeScrolledRows (scrolledRows);
        }

        // 2. Read dirty rows
        if (dirtyMask != 0)
        {
            const int numRows { grid.getNumRows() };
            const int numCols { grid.getNumCols() };

            for (int r { 0 }; r < numRows and r < 64; ++r)
            {
                if (dirtyMask & (uint64_t (1) << r))
                {
                    const jam::Cell* rowData { grid.getReadPointer (r) };
                    screen.updateVisibleRow (r, rowData, numCols);
                }
            }
        }

        // No reshapeContent() call — setters handle calc() internally (TETRIS contract).
    }

    // 3. Caret position — always update when State or Grid changed
    if (stateDirty or gridDirty)
    {
        const int cols { grid.getNumCols() };
        const int rows { grid.getNumRows() };

        if (cols > 0 and rows > 0)
        {
            const int cursorCol { juce::jlimit (0, cols - 1, state.getCursorCol()) };
            const int cursorRow { juce::jlimit (0, rows - 1, state.getCursorRow()) };
            screen.setCaretPosition (cursorCol, cursorRow);
        }
    }
}
