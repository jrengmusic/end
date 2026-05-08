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
    screen.setText();

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
    screen.setBounds (getLocalBounds()
                          .withTrimmedTop (config.nexus.terminal.paddingTop)
                          .withTrimmedRight (config.nexus.terminal.paddingRight)
                          .withTrimmedBottom (config.nexus.terminal.paddingBottom)
                          .withTrimmedLeft (config.nexus.terminal.paddingLeft));
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
