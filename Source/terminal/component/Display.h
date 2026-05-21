#pragma once
#include <JuceHeader.h>
#include "PaneComponent.h"
#include "Screen.h"
#include "../../lua/Engine.h"
#include "../Processor.h"
#include "../Input.h"
#include "../Mouse.h"
#include "../LinkManager.h"

namespace terminal
{
/*____________________________________________________________________________*/

class Display
    : public PaneComponent
    , public juce::KeyListener
{
public:
    Display (terminal::Processor& processor);

    ~Display() override;

    // PaneComponent interface
    juce::String getPaneType() const noexcept override;
    void switchRenderer (app::RendererType type) noexcept override;
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

    std::function<void (const juce::File&)> onOpenMarkdown;
    std::function<void (const juce::File&)> onOpenImage;

    /** @brief Returns a mutable reference to the Processor that backs this Display.
     *  @note MESSAGE THREAD. */
    terminal::Processor& getProcessor() noexcept { return processor; }

    // juce::Component
    void resized() override;
    void focusGained (FocusChangeType cause) override;

    // juce::KeyListener
    bool keyPressed (const juce::KeyPress& key, juce::Component* originatingComponent) override;

    // juce::Component mouse events
    void mouseDown (const juce::MouseEvent& event) override;
    void mouseDoubleClick (const juce::MouseEvent& event) override;
    void mouseDrag (const juce::MouseEvent& event) override;
    void mouseUp (const juce::MouseEvent& event) override;
    void mouseMove (const juce::MouseEvent& event) override;
    void mouseWheelMove (const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override;

private:
    const lua::Engine& config { *lua::Engine::getContext() };
    terminal::Processor& processor;
    terminal::State& state;

    std::unique_ptr<jam::ComponentAttachment> attachment;
    juce::ValueTree normalScreen;
    juce::ValueTree alternateScreen;
    terminal::Screen screen;

    terminal::LinkManager linkManager;
    terminal::Input input;
    terminal::Mouse mouse;

    /** @brief Creates NORMAL/ALTERNATE screen nodes, grafts them into State, and returns
     *         the State reference so Screen's member initializer can run after nodes are
     *         in the tree.
     *  @note Called from member initializer list — executes before Screen's constructor. */
    static terminal::State& createAndAttachState (terminal::State& stateToSeed,
                                                  juce::ValueTree& normalScreenNode,
                                                  juce::ValueTree& alternateScreenNode) noexcept;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Display)
};

/**______________________________END OF NAMESPACE______________________________*/
} // namespace terminal
