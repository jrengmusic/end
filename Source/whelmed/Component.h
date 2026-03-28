#pragma once
#include <JuceHeader.h>
#include "../component/PaneComponent.h"
#include "State.h"
#include "Screen.h"
#include "../component/LoaderOverlay.h"
#include "Parser.h"

namespace Whelmed
{ /*____________________________________________________________________________*/

class Component
    : public PaneComponent
    , private juce::ValueTree::Listener
{
public:
    Component();
    ~Component() override;

    juce::String getPaneType() const noexcept override { return "document"; }
    void switchRenderer (PaneComponent::RendererType type) override;
    void applyConfig() noexcept override;

    bool keyPressed (const juce::KeyPress& key) override;
    void paint (juce::Graphics& g) override;
    void resized() override;

    void openFile (const juce::File& file);
    juce::ValueTree getValueTree() noexcept override;

private:
    void valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property) override;

    State docState;
    juce::ValueTree state;
    std::unique_ptr<Parser> parser;
    juce::juce_wchar pendingPrefix { 0 };
    int totalBlocks { 0 };

    juce::Viewport viewport;
    Screen screen;
    ::LoaderOverlay loaderOverlay;

    std::unique_ptr<jreng::Mermaid::Parser> mermaidParser;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Component)
};

/**_____________________________END OF NAMESPACE______________________________*/
}// namespace Whelmed
