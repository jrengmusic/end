#pragma once
#include <JuceHeader.h>
#include "PaneComponent.h"
#include "../terminal/rendering/Screen.h"
#include "../terminal/logic/Processor.h"
#include "../lua/Engine.h"

namespace Terminal
{
/*____________________________________________________________________________*/

class Display
    : public PaneComponent
    , public juce::KeyListener
    , public juce::ValueTree::Listener
{
public:
    Display (Terminal::Processor& processor);

    ~Display() override;

    // PaneComponent interface
    juce::String getPaneType() const noexcept override;
    void switchRenderer (App::RendererType type) noexcept override;
    juce::ValueTree getValueTree() noexcept override;
    void applyConfig() noexcept override;
    void applyZoom (float zoom) noexcept override;
    void enterSelectionMode() noexcept override;
    void copySelection() noexcept override;
    bool hasSelection() const noexcept override;

    // Deferred stubs
    bool isInSelectionMode() const noexcept;
    void exitSelectionMode() noexcept;
    void enterOpenFileMode() noexcept;
    void pasteClipboard();
    void writeToPty (const char* data, int len) noexcept;
    int getHintPage() const noexcept;
    int getHintTotalPages() const noexcept;

    std::function<void()> onProcessExited;
    std::function<void (const juce::File&)> onOpenMarkdown;
    std::function<void (const juce::File&)> onOpenImage;

    /** @brief Returns a mutable reference to the Processor that backs this Display.
     *  @note MESSAGE THREAD. */
    Terminal::Processor& getProcessor() noexcept { return processor; }

    // juce::Component
    void resized() override;
    void focusGained (FocusChangeType cause) override;

    // juce::KeyListener
    bool keyPressed (const juce::KeyPress& key, juce::Component* originatingComponent) override;

    // juce::ValueTree::Listener
    void valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property) override;

private:
    const lua::Engine& config { *lua::Engine::getContext() };
    Terminal::Processor& processor;
    Terminal::State& state;
    Terminal::Grid& grid;

    Terminal::Screen screen;
    juce::VBlankAttachment vblank;

    int lastCols { 0 };///< Previous column count for resize debounce.
    int lastRows { 0 };///< Previous row count for resize debounce.

    void onVBlank();
    void updateDimensions (const juce::Rectangle<int>& contentBounds) noexcept;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Display)
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Terminal
