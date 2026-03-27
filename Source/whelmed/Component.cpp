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
    addChildComponent (spinnerOverlay);
}

Component::~Component()
{
    parser.reset();
}

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
        rebuildBlocks();
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
    spinnerOverlay.setBounds (getLocalBounds());
    layoutBlocks();
}

// ============================================================================
// Public API
// ============================================================================

void Component::openFile (const juce::File& file)
{
    parser.reset();

    state.emplace (file);
    spinnerOverlay.show();
    parser = std::make_unique<Parser> (*state);
    parser->startParsing (file);

    startTimerHz (60);
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

void Component::timerCallback()
{
    if (state and state->flush())
    {
        stopTimer();
        rebuildBlocks();
        spinnerOverlay.hide();
    }
}

void Component::rebuildBlocks()
{
    jassert (state);

    // Remove all existing children from content before clearing blocks
    content->removeAllChildren();

    textBlocks.clear();
    codeBlocks.clear();
    mermaidBlocks.clear();
    tableBlocks.clear();

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

    const auto& doc { state->getDocument() };

    int i { 0 };

    while (i < doc.blockCount)
    {
        const auto& block { doc.blocks[i] };

        if (block.type == jreng::Markdown::BlockType::Markdown)
        {
            auto& textBlock { textBlocks.add (std::make_unique<TextBlock>()) };

            // Merge consecutive Markdown blocks into one TextEditor
            while (i < doc.blockCount and doc.blocks[i].type == jreng::Markdown::BlockType::Markdown)
            {
                appendBlockContent (*textBlock, doc, i, fontConfig);
                ++i;
            }

            content->addAndMakeVisible (textBlock.get());
        }
        else if (block.type == jreng::Markdown::BlockType::CodeFence)
        {
            juce::String blockContent  { juce::String::fromUTF8 (doc.text + block.contentOffset,  block.contentLength) };
            juce::String blockLanguage { juce::String::fromUTF8 (doc.text + block.languageOffset, block.languageLength) };
            auto& codeBlock { codeBlocks.add (std::make_unique<CodeBlock> (blockContent, blockLanguage)) };
            content->addAndMakeVisible (codeBlock.get());
            ++i;
        }
        else if (block.type == jreng::Markdown::BlockType::Mermaid)
        {
            juce::String mermaidCode { juce::String::fromUTF8 (doc.text + block.contentOffset, block.contentLength) };

            if (mermaidParser != nullptr)
            {
                auto& mermaidBlock { mermaidBlocks.add (std::make_unique<MermaidBlock>()) };

                mermaidParser->convertToSVG (mermaidCode, [&mermaidBlock] (const juce::String& svg)
                {
                    auto result { MermaidSVGParser::parse (svg) };
                    mermaidBlock->setParseResult (std::move (result));
                });

                content->addAndMakeVisible (mermaidBlock.get());
            }

            ++i;
        }
        else if (block.type == jreng::Markdown::BlockType::Table)
        {
            juce::String tableMarkdown { juce::String::fromUTF8 (doc.text + block.contentOffset, block.contentLength) };
            auto& tableBlock { tableBlocks.add (std::make_unique<TableBlock>()) };
            tableBlock->setTableMarkdown (tableMarkdown);
            content->addAndMakeVisible (tableBlock.get());
            ++i;
        }
        else
        {
            ++i;
        }
    }

    layoutBlocks();
    setBufferedToImage (true);
}

void Component::appendBlockContent (TextBlock& textBlock,
                                     const jreng::Markdown::ParsedDocument& doc,
                                     int blockIndex,
                                     const jreng::Markdown::FontConfig& fontConfig)
{
    const auto& block { doc.blocks[blockIndex] };
    juce::String blockContent { juce::String::fromUTF8 (doc.text + block.contentOffset, block.contentLength) };

    float fontSize { fontConfig.bodySize };
    juce::Colour headingColour { fontConfig.bodyColour };

    switch (block.level)
    {
        case 1: fontSize = fontConfig.h1Size; headingColour = fontConfig.h1Colour; break;
        case 2: fontSize = fontConfig.h2Size; headingColour = fontConfig.h2Colour; break;
        case 3: fontSize = fontConfig.h3Size; headingColour = fontConfig.h3Colour; break;
        case 4: fontSize = fontConfig.h4Size; headingColour = fontConfig.h4Colour; break;
        case 5: fontSize = fontConfig.h5Size; headingColour = fontConfig.h5Colour; break;
        case 6: fontSize = fontConfig.h6Size; headingColour = fontConfig.h6Colour; break;
        default: break;
    }

    if (block.spanCount == 0)
    {
        textBlock.appendStyledText (blockContent + "\n",
                                    juce::FontOptions().withName (fontConfig.bodyFamily).withPointHeight (fontSize),
                                    headingColour);
    }
    else
    {
        for (int s { block.spanOffset }; s < block.spanOffset + block.spanCount; ++s)
        {
            const auto& span { doc.spans[s] };
            juce::String spanText { blockContent.substring (span.startOffset, span.endOffset) };

            bool isBold   { (span.style & jreng::Markdown::Bold)   != jreng::Markdown::None };
            bool isItalic { (span.style & jreng::Markdown::Italic) != jreng::Markdown::None };
            bool isCode   { (span.style & jreng::Markdown::Code)   != jreng::Markdown::None };
            bool isLink   { (span.style & jreng::Markdown::Link)   != jreng::Markdown::None };

            float size { isCode ? fontConfig.codeSize : fontSize };
            const juce::String& family { isCode ? fontConfig.codeFamily : fontConfig.bodyFamily };

            juce::String style { "Regular" };

            if (isBold and isItalic)
                style = "Bold Italic";
            else if (isBold)
                style = "Bold";
            else if (isItalic)
                style = "Italic";

            juce::Colour colour { headingColour };

            if (isCode)
                colour = fontConfig.codeColour;
            else if (isLink)
                colour = fontConfig.linkColour;

            textBlock.appendStyledText (spanText,
                                        juce::FontOptions().withName (family).withPointHeight (size).withStyle (style),
                                        colour);
        }

        textBlock.appendStyledText ("\n",
                                    juce::FontOptions().withName (fontConfig.bodyFamily).withPointHeight (fontSize),
                                    fontConfig.bodyColour);
    }
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

        if (auto* renderedBlock { dynamic_cast<TextBlock*> (child) })
            height = renderedBlock->getPreferredHeight();
        else if (auto* codeBlock { dynamic_cast<CodeBlock*> (child) })
            height = codeBlock->getPreferredHeight();
        else if (auto* mermaidBlock { dynamic_cast<MermaidBlock*> (child) })
            height = mermaidBlock->getPreferredHeight();
        else if (auto* tableBlock { dynamic_cast<TableBlock*> (child) })
            height = tableBlock->getPreferredHeight();

        // Second pass: set actual height
        child->setBounds (0, contentHeight, availableWidth, height);
        contentHeight += height;
    }

    content->setSize (availableWidth, juce::jmax (1, contentHeight));
}

/**_____________________________END OF NAMESPACE______________________________*/
}// namespace Whelmed
