#include "TerminalDisplay.h"
#include <jam_tui/jam_tui.h>

Terminal::Display::Display (Terminal::Processor& processorToUse)
    : processor (processorToUse)
    , state (processorToUse.getState())
    , grid (processorToUse.getGrid())
    , screen (processorToUse.getState())
    , vblank (this, [this] { onVBlank(); })
{
    addAndMakeVisible (screen);
    screen.setScrollBarThickness (config.display.scrollbarWidth);
    // Screen receives content via makeLayout() from Grid FIFO drain

    setWantsKeyboardFocus (true);
    addKeyListener (this);
    state.get().addListener (this);
}

Terminal::Display::~Display()
{
    state.get().removeListener (this);
    vblank = juce::VBlankAttachment {};
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
    if (processor.writeInput)
        processor.writeInput (data, len);
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
                    if (newCols != lastDimensions.first or newRows != lastDimensions.second)
                    {
                        lastDimensions = { newCols, newRows };
                        screen.setDimensions (newCols, newRows);
                        processor.resized (newCols, newRows);

                        if (processor.onResize != nullptr)
                            processor.onResize (newCols, newRows, contentBounds.getWidth(), contentBounds.getHeight());
                    }
                }
            }
        }
    }
}

// juce::KeyListener
bool Terminal::Display::keyPressed (const juce::KeyPress& key, juce::Component*)
{
    if (processor.writeInput != nullptr)
    {
        const auto encoded { processor.encodeKeyPress (key) };

        if (encoded.isNotEmpty())
            processor.writeInput (encoded.toRawUTF8(), static_cast<int> (encoded.getNumBytesAsUTF8()));
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
    const bool dirty { state.consumeSnapshotDirty() };

    if (dirty)
    {
        state.refresh();

        // Drain Grid FIFO → screen.makeLayout() in batches
        static constexpr int batchSize { 256 };
        static constexpr int maxOpsPerFrame { 8192 };
        Terminal::Command batch[batchSize];
        int totalDrained { 0 };

        int drained { grid.drain (batch, batchSize) };

        while (drained > 0 and totalDrained < maxOpsPerFrame)
        {
            screen.makeLayout (batch, drained);
            totalDrained += drained;
            drained = grid.drain (batch, batchSize);
        }

        // Caret position — Display reads State, tells Screen
        const int cols { lastDimensions.first };
        const int rows { lastDimensions.second };

        if (cols > 0 and rows > 0)
        {
            const int caretIndex { juce::jlimit (0, cols * rows, state.getCursorRow() * cols + state.getCursorCol()) };
            screen.setCaretPosition (caretIndex);
        }
    }
}
