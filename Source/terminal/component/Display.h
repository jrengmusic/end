#pragma once
#include <JuceHeader.h>
#include "PaneComponent.h"
#include "Screen.h"
#include "../Session.h"
#include "../Input.h"
#include "../Mouse.h"
#include "../LinkManager.h"

namespace terminal
{
/*____________________________________________________________________________*/

class Display
    : public PaneComponent
    , public juce::KeyListener
    , public juce::ValueTree::Listener
{
public:
    /**
     * @brief Constructs Display and parents Session's Screen for rendering.
     *
     * Takes Session& — Display parents Screen (owned by Session) for rendering
     * via addAndMakeVisible. Resize flows through State: Screen writes viewport,
     * Processor's vTPC calls setWinsize().
     *
     * @param session  The terminal session — provides both Screen (for rendering) and
     *                 Processor (for input/events).  Must outlive this Display.
     * @note MESSAGE THREAD.
     */
    Display (terminal::Session& session);

    ~Display() override;

    // PaneComponent interface
    juce::String getPaneType() const noexcept override;
    juce::ValueTree getValueTree() noexcept override;
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

private:
    terminal::Session& session;
    terminal::Processor& processor;
    terminal::State& state;

    std::unique_ptr<jam::ComponentAttachment> attachment;

    juce::ValueTree terminalState;  ///< Per-session state tree — Display listens for content updates.

    terminal::LinkManager linkManager;
    terminal::Input input;
    terminal::Mouse mouse;

    void applyFromAppState() noexcept;
    void valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property) override;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Display)
};

/**______________________________END OF NAMESPACE______________________________*/
} // namespace terminal
