#include <JuceHeader.h>
#include "Component.h"
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

void Component::resized()
{
    viewport.setBounds (getLocalBounds());
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

    for (const auto& parsedBlock : state->getBlocks())
    {
        if (parsedBlock.type == jreng::Markdown::BlockType::Markdown
            or parsedBlock.type == jreng::Markdown::BlockType::CodeFence)
        {
            auto attrString { jreng::Markdown::Parser::toAttributedString (parsedBlock) };
            auto& block { blocks.add (std::make_unique<Block> (attrString)) };
            content->addAndMakeVisible (block.get());
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

    for (auto& block : blocks)
    {
        block->setBounds (0, contentHeight, availableWidth, 1);
        const int blockHeight { block->getPreferredHeight() };
        block->setBounds (0, contentHeight, availableWidth, blockHeight);
        contentHeight += blockHeight;
    }

    content->setSize (availableWidth, juce::jmax (1, contentHeight));
}

/**_____________________________END OF NAMESPACE______________________________*/
}// namespace Whelmed
