#pragma once
#include <JuceHeader.h>
#include "../component/PaneComponent.h"
#include "State.h"
#include "Screen.h"
#include "SpinnerOverlay.h"

namespace Whelmed
{ /*____________________________________________________________________________*/

class Component
    : public PaneComponent
{
public:
    Component();
    ~Component() override;

    // PaneComponent interface
    juce::String getPaneType() const noexcept override { return "document"; }
    void switchRenderer (PaneComponent::RendererType type) override;
    void applyConfig() noexcept override;

    // juce::Component
    bool keyPressed (const juce::KeyPress& key) override;
    void paint (juce::Graphics& g) override;
    void resized() override;

    void openFile (const juce::File& file);
    juce::ValueTree getValueTree() noexcept override;

private:
    void buildDocConfig();

    State docState;
    jreng::Markdown::DocConfig docConfig;
    juce::juce_wchar pendingPrefix { 0 };
    int totalBlocks { 0 };

    juce::Viewport viewport;
    Screen screen;
    SpinnerOverlay spinnerOverlay;

    std::unique_ptr<jreng::Mermaid::Parser> mermaidParser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Component)
};

/**_____________________________END OF NAMESPACE______________________________*/
}// namespace Whelmed
