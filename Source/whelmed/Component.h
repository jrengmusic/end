#pragma once
#include <JuceHeader.h>
#include "../component/PaneComponent.h"
#include "State.h"
#include "Block.h"
#include "TextBlock.h"
#include "CodeBlock.h"
#include "MermaidBlock.h"
#include "TableBlock.h"
#include "SpinnerOverlay.h"
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
    // juce::ValueTree::Listener
    void valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property) override;

    void updateLayout();
    void clearBlocks();
    void buildDocConfig();
    void appendBlockContent (TextBlock& textBlock,
                             const jreng::Markdown::ParsedDocument& doc,
                             int blockIndex);

    State docState;
    juce::ValueTree state;
    std::unique_ptr<Parser> parser;
    jreng::Markdown::DocConfig docConfig;
    juce::juce_wchar pendingPrefix { 0 };  ///< For multi-key sequences (e.g. gg)
    int totalBlocks { 0 };  ///< Set once in openFile, constant for document lifetime

    juce::Viewport viewport;
    std::unique_ptr<juce::Component> content;
    jreng::Owner<Block> blocks;
    SpinnerOverlay spinnerOverlay;

    std::unique_ptr<jreng::Mermaid::Parser> mermaidParser;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Component)
};

/**_____________________________END OF NAMESPACE______________________________*/
}// namespace Whelmed
