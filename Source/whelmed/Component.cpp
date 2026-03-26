#include <JuceHeader.h>
#include "Component.h"
#include "config/Config.h"
#include "../terminal/action/Action.h"

namespace Whelmed
{ /*____________________________________________________________________________*/

Component::Component()
    : content { std::make_unique<juce::Component>() }
{
    viewport.setViewedComponent (content.get(), false);
    viewport.setScrollBarsShown (true, false);
    addAndMakeVisible (viewport);
}

Component::~Component() = default;

// ============================================================================
// PaneComponent interface
// ============================================================================

bool Component::keyPressed (const juce::KeyPress& key)
{
    return Terminal::Action::getContext()->handleKeyPress (key);
}

void Component::switchRenderer (PaneComponent::RendererType type)
{
    juce::ignoreUnused (type);
}

void Component::applyConfig() noexcept
{
    if (state)
    {
        state->reload();
        rebuildBlocks();
    }
}

// ============================================================================
// juce::Component
// ============================================================================

void Component::paint (juce::Graphics& g)
{
    const juce::Colour bg { Whelmed::Config::getContext()->getColour (Whelmed::Config::Key::background) };
    g.fillAll (bg);
}

void Component::resized()
{
    const auto* config { Whelmed::Config::getContext() };
    auto contentArea { getLocalBounds() };
    contentArea.removeFromTop    (config->getInt (Whelmed::Config::Key::paddingTop));
    contentArea.removeFromRight  (config->getInt (Whelmed::Config::Key::paddingRight));
    contentArea.removeFromBottom (config->getInt (Whelmed::Config::Key::paddingBottom));
    contentArea.removeFromLeft   (config->getInt (Whelmed::Config::Key::paddingLeft));

    viewport.setBounds (contentArea);
    layoutBlocks();
}

// ============================================================================
// Public API
// ============================================================================

void Component::openFile (const juce::File& file)
{
    state.emplace (file);
    rebuildBlocks();
}

juce::ValueTree Component::getValueTree() noexcept
{
    juce::ValueTree result;

    if (state)
        result = state->getValueTree();

    return result;
}

// ============================================================================
// Private
// ============================================================================

void Component::rebuildBlocks()
{
    jassert (state);

    blocks.clear();
    codeBlocks.clear();

    const auto* config { Whelmed::Config::getContext() };

    jreng::Markdown::FontConfig fontConfig;
    fontConfig.bodyFamily = config->getString (Whelmed::Config::Key::fontFamily);
    fontConfig.bodySize   = config->getFloat (Whelmed::Config::Key::fontSize);
    fontConfig.codeFamily = config->getString (Whelmed::Config::Key::codeFamily);
    fontConfig.codeSize   = config->getFloat (Whelmed::Config::Key::codeSize);
    fontConfig.h1Size     = config->getFloat (Whelmed::Config::Key::h1Size);
    fontConfig.h2Size     = config->getFloat (Whelmed::Config::Key::h2Size);
    fontConfig.h3Size     = config->getFloat (Whelmed::Config::Key::h3Size);
    fontConfig.h4Size     = config->getFloat (Whelmed::Config::Key::h4Size);
    fontConfig.h5Size     = config->getFloat (Whelmed::Config::Key::h5Size);
    fontConfig.h6Size     = config->getFloat (Whelmed::Config::Key::h6Size);
    fontConfig.bodyColour = config->getColour (Whelmed::Config::Key::bodyColour);
    fontConfig.codeColour = config->getColour (Whelmed::Config::Key::codeColour);
    fontConfig.linkColour = config->getColour (Whelmed::Config::Key::linkColour);
    fontConfig.h1Colour   = config->getColour (Whelmed::Config::Key::h1Colour);
    fontConfig.h2Colour   = config->getColour (Whelmed::Config::Key::h2Colour);
    fontConfig.h3Colour   = config->getColour (Whelmed::Config::Key::h3Colour);
    fontConfig.h4Colour   = config->getColour (Whelmed::Config::Key::h4Colour);
    fontConfig.h5Colour   = config->getColour (Whelmed::Config::Key::h5Colour);
    fontConfig.h6Colour   = config->getColour (Whelmed::Config::Key::h6Colour);

    for (const auto& parsedBlock : state->getBlocks())
    {
        if (parsedBlock.type == jreng::Markdown::BlockType::Markdown)
        {
            auto attrString { jreng::Markdown::Parser::toAttributedString (parsedBlock, fontConfig) };
            auto& block { blocks.add (std::make_unique<Block> (attrString)) };
            content->addAndMakeVisible (block.get());
        }
        else if (parsedBlock.type == jreng::Markdown::BlockType::CodeFence)
        {
            auto& codeBlock { codeBlocks.add (std::make_unique<CodeBlock> (parsedBlock.content, parsedBlock.language)) };
            content->addAndMakeVisible (codeBlock.get());
        }
        else if (parsedBlock.type == jreng::Markdown::BlockType::Mermaid)
        {
            // TODO: mermaid rendering
        }
        else if (parsedBlock.type == jreng::Markdown::BlockType::Table)
        {
            // TODO: Step 5.10 — table component
        }
    }

    layoutBlocks();
}

void Component::layoutBlocks()
{
    const int availableWidth { viewport.getMaximumVisibleWidth() };
    int contentHeight { 0 };

    for (int i { 0 }; i < content->getNumChildComponents(); ++i)
    {
        auto* child { content->getChildComponent (i) };

        // First pass: set width to trigger resized / rebuildLayout
        child->setBounds (0, contentHeight, availableWidth, 1);

        int height { 0 };

        if (auto* block { dynamic_cast<Block*> (child) })
            height = block->getPreferredHeight();
        else if (auto* codeBlock { dynamic_cast<CodeBlock*> (child) })
            height = codeBlock->getPreferredHeight();

        // Second pass: set actual height
        child->setBounds (0, contentHeight, availableWidth, height);
        contentHeight += height;
    }

    content->setSize (availableWidth, juce::jmax (1, contentHeight));
}

/**_____________________________END OF NAMESPACE______________________________*/
}// namespace Whelmed
