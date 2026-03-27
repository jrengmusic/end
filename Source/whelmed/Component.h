#pragma once
#include <JuceHeader.h>
#include "../component/PaneComponent.h"
#include "State.h"
#include "TextBlock.h"
#include "CodeBlock.h"
#include "MermaidBlock.h"
#include "TableBlock.h"
#include "SpinnerOverlay.h"
#include "Parser.h"

namespace Whelmed
{ /*____________________________________________________________________________*/

class Component : public PaneComponent,
                  private juce::Timer
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
    // juce::Timer
    void timerCallback() override;

    void rebuildBlocks();
    void layoutBlocks();
    void appendBlockContent (TextBlock& textBlock,
                             const jreng::Markdown::ParsedDocument& doc,
                             int blockIndex,
                             const jreng::Markdown::FontConfig& fontConfig);

    std::optional<State> state;
    std::unique_ptr<Parser> parser;

    juce::Viewport viewport;
    std::unique_ptr<juce::Component> content;
    jreng::Owner<TextBlock> textBlocks;
    jreng::Owner<CodeBlock> codeBlocks;
    jreng::Owner<MermaidBlock> mermaidBlocks;
    jreng::Owner<TableBlock> tableBlocks;
    SpinnerOverlay spinnerOverlay;

    std::unique_ptr<jreng::Mermaid::Parser> mermaidParser;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Component)
};

/**_____________________________END OF NAMESPACE______________________________*/
}// namespace Whelmed
