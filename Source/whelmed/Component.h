#pragma once
#include <JuceHeader.h>
#include "../component/PaneComponent.h"
#include "State.h"
#include "Block.h"

namespace Whelmed
{ /*____________________________________________________________________________*/

class Component : public PaneComponent
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
    void resized() override;

    void openFile (const juce::File& file);
    juce::ValueTree getValueTree() noexcept override;

private:
    void rebuildBlocks();
    void layoutBlocks();

    std::optional<State> state;

    juce::Viewport viewport;
    std::unique_ptr<juce::Component> content;
    jreng::Owner<Block> blocks;

    std::unique_ptr<jreng::Mermaid::Parser> mermaidParser;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Component)
};

/**_____________________________END OF NAMESPACE______________________________*/
}// namespace Whelmed
