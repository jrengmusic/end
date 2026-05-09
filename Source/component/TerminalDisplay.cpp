#include "TerminalDisplay.h"
#include <jam_tui/jam_tui.h>
#include <array>

Terminal::Display::Display (Terminal::Processor& processorToUse)
    : processor (processorToUse)
    , state (processorToUse.getState())
    , grid (processorToUse.getGrid())
    , screen (processorToUse.getState())
    , vblank (this, [this] { onVBlank(); })
{
    addAndMakeVisible (screen);
    screen.setScrollBarThickness (config.display.scrollbarWidth);
    // Screen receives content via appendScrollbackRow / updateVisibleRow from Grid

    setWantsKeyboardFocus (true);
    addKeyListener (this);
    state.get().addListener (this);
}

Terminal::Display::~Display()
{
    cancelPendingUpdate();
    removeKeyListener (this);
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
void Terminal::Display::resized()
{
    const auto contentBounds { getLocalBounds()
                                   .withTrimmedTop (config.nexus.terminal.paddingTop)
                                   .withTrimmedRight (config.nexus.terminal.paddingRight)
                                   .withTrimmedBottom (config.nexus.terminal.paddingBottom)
                                   .withTrimmedLeft (config.nexus.terminal.paddingLeft) };

    screen.setBounds (contentBounds);

    // Compute grid dimensions from pixel bounds
    const float scale { jam::Typeface::getDisplayScale() };
    auto* typeface { jam::Typeface::findTypeface (config.display.font.family) };

    if (typeface != nullptr and contentBounds.getWidth() > 0 and contentBounds.getHeight() > 0)
    {
        const float fontSize { config.dpiCorrectedFontSize() };
        const auto fm { typeface->getMetrics() };

        if (fm.isValid() and fontSize > 0.0f)
        {
            float maxAdvance { 0.0f };

            for (uint32_t code { 32 }; code <= 127; ++code)
            {
                const float adv { typeface->getAdvanceWidth (code) * fontSize };

                if (adv > maxAdvance)
                    maxAdvance = adv;
            }

            if (maxAdvance <= 0.0f)
                maxAdvance = fontSize;

            const int logCellW { jam::toInt (maxAdvance, true) };
            const int logCellH { jam::toInt ((fm.ascent + fm.descent + fm.leading) * fontSize, true) };
            const int physCellW { jam::toInt (static_cast<float> (logCellW) * scale * config.display.font.cellWidth, true) };
            const int physCellH { jam::toInt (static_cast<float> (logCellH) * scale * config.display.font.lineHeight, true) };

            if (physCellW > 0 and physCellH > 0)
            {
                const int physContentW { jam::toInt (static_cast<float> (contentBounds.getWidth()) * scale, true) };
                const int physContentH { jam::toInt (static_cast<float> (contentBounds.getHeight()) * scale, true) };
                const auto gridRect { jam::metrics::Cell::Rectangle (jam::Bounds { physCellW, physCellH },
                                                                     juce::Rectangle<int> { 0, 0, physContentW, physContentH }) };
                const int newCols { gridRect.getWidth().value };
                const int newRows { gridRect.getHeight().value };

                if (newCols > 0 and newRows > 0)
                {
                    // Resize debounce — only act when grid dimensions actually change
                    if (newCols != lastCols or newRows != lastRows)
                    {
                        lastCols = newCols;
                        lastRows = newRows;
                        screen.setDimensions (newCols, newRows);
                        // Top-down: Display writes State → Processor::valueTreePropertyChanged → Video.
                        processor.getState().setDimensions (newCols, newRows);

                        if (processor.events.contains (Terminal::ID::terminalResize))
                            processor.events.get (Terminal::ID::terminalResize, int (newCols), int (newRows), int (contentBounds.getWidth()), int (contentBounds.getHeight()));
                    }
                }
            }
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

    const int activeScreen { static_cast<int> (state.getActiveScreen()) };

    if (activeScreen != lastActiveScreen)
    {
        lastActiveScreen = activeScreen;
        screen.setActive (activeScreen);
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
        // 1. Drain scroll-off rows
        if (scrolledRows > 0)
        {
            const int numCols { grid.getNumCols() };
            static constexpr int maxBatchRows { 512 };
            const int batchCount { juce::jmin (scrolledRows, maxBatchRows) };
            std::array<const jam::Cell*, maxBatchRows> scrolledPtrs {};

            for (int i { 0 }; i < batchCount; ++i)
                scrolledPtrs.at (static_cast<size_t> (i)) = grid.getScrolledReadPointer (i);

            screen.appendScrollbackRows (scrolledPtrs.data(), batchCount, numCols);
            grid.consumeScrolledRows (batchCount);
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

        // 3. Trigger repaint
        screen.repaintContent();
    }

    // 4. Caret position — always update when State or Grid changed
    if (stateDirty or gridDirty)
    {
        const int cols { grid.getNumCols() };
        const int rows { grid.getNumRows() };

        if (cols > 0 and rows > 0)
        {
            const int caretIndex { juce::jlimit (0, cols * rows, state.getCursorRow() * cols + state.getCursorCol()) };
            screen.setCaretPosition (caretIndex);
        }
    }
}
