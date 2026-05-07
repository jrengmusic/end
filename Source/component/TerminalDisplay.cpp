#include "TerminalDisplay.h"
#include <jam_tui/jam_tui.h>

Terminal::Display::Display (Terminal::Processor& processorToUse)
    : processor (processorToUse)
    , state (processorToUse.getState())
    , grid (processorToUse.getGrid())
    , screen {}
    , vblank (this, [this] { onVBlank(); })
{
    addAndMakeVisible (screen);
    screen.setScrollBarThickness (config.display.scrollbarWidth);
    screen.setFont (juce::Font (juce::FontOptions (config.display.font.family, config.dpiCorrectedFontSize(), juce::Font::plain)));

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

// Helpers
jam::Cell::Rectangle Terminal::Display::computeGridSize() const noexcept
{
    return jam::Cell::Rectangle (jam::Bounds { cellWidth, cellHeight },
                                 juce::Rectangle<int> { 0, 0, screen.getWidth(), screen.getHeight() });
}

// juce::Component
void Terminal::Display::resized()
{
    screen.setBounds (getLocalBounds()
                          .withTrimmedTop (config.nexus.terminal.paddingTop)
                          .withTrimmedRight (config.nexus.terminal.paddingRight)
                          .withTrimmedBottom (config.nexus.terminal.paddingBottom)
                          .withTrimmedLeft (config.nexus.terminal.paddingLeft));

    // Compute cell metrics inline — max advance across ASCII 32-127.
    auto* typeface { jam::Typeface::findTypeface (config.display.font.family) };

    if (typeface != nullptr)
    {
        const float fontSize { config.dpiCorrectedFontSize() };

        if (fontSize > 0.0f)
        {
            const auto fm { typeface->getMetrics() };

            if (fm.isValid())
            {
                const float ascent  { fm.ascent  * fontSize };
                const float descent { fm.descent * fontSize };
                const float leading { fm.leading * fontSize };

                float maxAdvance { 0.0f };

                for (uint32_t code { 32 }; code <= 127; ++code)
                {
                    const float adv { typeface->getAdvanceWidth (code) * fontSize };

                    if (adv > maxAdvance)
                        maxAdvance = adv;
                }

                if (maxAdvance <= 0.0f)
                    maxAdvance = fontSize;

                cellWidth  = jam::toInt (maxAdvance, true);
                cellHeight = jam::toInt (ascent + descent + leading, true);
                baseline   = jam::toInt (ascent, true);

                screen.setText (jam::Bounds { cellWidth, cellHeight }, baseline);
            }
        }
    }

    if (cellWidth > 0 and cellHeight > 0)
    {
        const auto gridSize { computeGridSize() };
        const int newCols { gridSize.getWidth().value };
        const int newRows { gridSize.getHeight().value };

        if (newCols > 0 and newRows > 0)
        {
            processor.resized (newCols, newRows);

            if (processor.onResize != nullptr)
            {
                const auto px { jam::Cell::Point::totalPixels<int> (jam::Cell { newCols }, jam::Cell { newRows }, jam::Bounds { cellWidth, cellHeight }) };
                processor.onResize (newCols, newRows, px.x, px.y);
            }
        }
    }

    triggerAsyncUpdate();
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
}

// juce::AsyncUpdater
void Terminal::Display::handleAsyncUpdate()
{
    if (cellWidth > 0 and cellHeight > 0)
    {
        const auto gridSize { computeGridSize() };
        const int newCols { gridSize.getWidth().value };
        const int newRows { gridSize.getHeight().value };

        if (newCols > 0 and newRows > 0)
        {
            if (processor.onResize != nullptr)
            {
                const auto px { jam::Cell::Point::totalPixels<int> (jam::Cell { newCols }, jam::Cell { newRows }, jam::Bounds { cellWidth, cellHeight }) };
                processor.onResize (newCols, newRows, px.x, px.y);
            }
        }
    }
}

void Terminal::Display::onVBlank()
{
    const bool dirty { state.consumeSnapshotDirty() };

    if (dirty)
    {
        state.refresh();

        const juce::ScopedTryLock stl { grid.getResizeLock() };

        if (stl.isLocked())
        {
        }
        else
        {
            state.setSnapshotDirty();
        }
    }
}
