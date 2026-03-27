#include <JuceHeader.h>
#include "Component.h"
#include "config/Config.h"
#include "../terminal/action/Action.h"

namespace Whelmed
{ /*____________________________________________________________________________*/

Component::Component()
    : content { std::make_unique<juce::Component>() }
    , state { docState.getValueTree() }
{
    state.addListener (this);
    viewport.setViewedComponent (content.get(), false);
    viewport.setScrollBarsShown (true, false);
    addAndMakeVisible (viewport);
    addAndMakeVisible (spinnerOverlay);
}

Component::~Component() { state.removeListener (this); }

// ============================================================================
// PaneComponent interface
// ============================================================================

bool Component::keyPressed (const juce::KeyPress& key)
{
    const auto* config { Whelmed::Config::getContext() };
    const int scrollStep { config->getInt (Whelmed::Config::Key::scrollStep) };
    const juce::String scrollDown { config->getString (Whelmed::Config::Key::scrollDown) };
    const juce::String scrollUp { config->getString (Whelmed::Config::Key::scrollUp) };
    const juce::String scrollTop { config->getString (Whelmed::Config::Key::scrollTop) };
    const juce::String scrollBottom { config->getString (Whelmed::Config::Key::scrollBottom) };

    const auto keyChar { key.getTextCharacter() };
    bool handled { false };

    // Check for pending prefix sequence (e.g. "gg")
    if (pendingPrefix != 0)
    {
        juce::String sequence { juce::String::charToString (pendingPrefix) + juce::String::charToString (keyChar) };
        pendingPrefix = 0;

        if (sequence == scrollTop)
        {
            viewport.setViewPosition (0, 0);
            handled = true;
        }
        else if (sequence == scrollBottom)
        {
            viewport.setViewPosition (0, content->getHeight() - viewport.getHeight());
            handled = true;
        }
    }

    if (not handled)
    {
        const juce::String keyString { juce::String::charToString (keyChar) };

        // Check if this char is the start of any multi-key binding
        if (scrollTop.length() > 1 and scrollTop.substring (0, 1) == keyString)
        {
            pendingPrefix = keyChar;
            handled = true;
        }
        else if (scrollBottom.length() > 1 and scrollBottom.substring (0, 1) == keyString)
        {
            pendingPrefix = keyChar;
            handled = true;
        }
        else if (keyString == scrollDown)
        {
            viewport.setViewPosition (0, viewport.getViewPositionY() + scrollStep);
            handled = true;
        }
        else if (keyString == scrollUp)
        {
            viewport.setViewPosition (0, juce::jmax (0, viewport.getViewPositionY() - scrollStep));
            handled = true;
        }
        else if (keyString == scrollBottom)
        {
            viewport.setViewPosition (0, content->getHeight() - viewport.getHeight());
            handled = true;
        }
    }

    if (not handled)
        handled = Terminal::Action::getContext()->handleKeyPress (key);

    return handled;
}

void Component::switchRenderer (PaneComponent::RendererType type) { juce::ignoreUnused (type); }

void Component::applyConfig() noexcept
{
    buildDocConfig();

    const auto& doc { docState.getDocument() };
    const int blockCount { content->getNumChildComponents() };

    for (int i { 0 }; i < blockCount; ++i)
    {
        if (doc.blocks[i].type == jreng::Markdown::BlockType::Markdown)
        {
            auto* textBlock { static_cast<TextBlock*> (content->getChildComponent (i)) };
            textBlock->clear();
            appendBlockContent (*textBlock, doc, i);
        }
    }

    resized();
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
    contentArea.removeFromTop (config->getInt (Whelmed::Config::Key::paddingTop));
    contentArea.removeFromRight (config->getInt (Whelmed::Config::Key::paddingRight));
    contentArea.removeFromBottom (config->getInt (Whelmed::Config::Key::paddingBottom));
    contentArea.removeFromLeft (config->getInt (Whelmed::Config::Key::paddingLeft));

    viewport.setBounds (contentArea);
    static constexpr int progressBarHeight { 24 };
    spinnerOverlay.setBounds (0, getHeight() - progressBarHeight, getWidth(), progressBarHeight);
    updateLayout();
}

// ============================================================================
// Public API
// ============================================================================

void Component::openFile (const juce::File& file)
{
    clearBlocks();
    buildDocConfig();

    // Synchronous parse — fast, no I/O blocking on message thread beyond file load
    const juce::String fileContent { file.loadFileAsString() };
    auto parsed { jreng::Markdown::Parser::parse (fileContent) };
    totalBlocks = parsed.blockCount;

    // Set ValueTree metadata before moving document
    juce::ValueTree tree { docState.getValueTree() };
    tree.setProperty (App::ID::filePath, file.getFullPathName(), nullptr);
    tree.setProperty (App::ID::displayName, file.getFileNameWithoutExtension(), nullptr);
    tree.setProperty (App::ID::scrollOffset, 0.0f, nullptr);
    tree.setProperty (App::ID::blockCount, 0, nullptr);
    tree.setProperty (App::ID::parseComplete, false, nullptr);

    // Create ALL empty components by block type before moving document
    for (int i { 0 }; i < totalBlocks; ++i)
    {
        const auto& block { parsed.blocks[i] };

        if (block.type == jreng::Markdown::BlockType::Markdown)
        {
            auto concrete { std::make_unique<TextBlock>() };
            content->addAndMakeVisible (concrete.get());
            blocks.add (std::move (concrete));
        }
        else if (block.type == jreng::Markdown::BlockType::CodeFence)
        {
            juce::String code { juce::String::fromUTF8 (parsed.text + block.contentOffset, block.contentLength) };
            juce::String language { juce::String::fromUTF8 (parsed.text + block.languageOffset, block.languageLength) };
            auto concrete { std::make_unique<CodeBlock> (code, language) };
            content->addAndMakeVisible (concrete.get());
            blocks.add (std::move (concrete));
        }
        else if (block.type == jreng::Markdown::BlockType::Mermaid)
        {
            auto concrete { std::make_unique<MermaidBlock>() };
            content->addAndMakeVisible (concrete.get());
            blocks.add (std::move (concrete));
        }
        else if (block.type == jreng::Markdown::BlockType::Table)
        {
            auto concrete { std::make_unique<TableBlock>() };
            content->addAndMakeVisible (concrete.get());
            blocks.add (std::move (concrete));
        }
    }

    // Move document to State — parser thread reads it via getDocumentForWriting()
    docState.setDocument (std::move (parsed));

    spinnerOverlay.show (totalBlocks);
    spinnerOverlay.toFront (false);
    resized();

    // Background thread resolves styles per block, signals via appendBlock()
    parser = std::make_unique<Parser> (docState, docConfig);
    parser->start();
}

juce::ValueTree Component::getValueTree() noexcept { return state; }

// ============================================================================
// Private
// ============================================================================

void Component::valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property)
{
    if (property == App::ID::blockCount)
    {
        const int blockCount { static_cast<int> (tree.getProperty (App::ID::blockCount)) };
        const int blockIndex { blockCount - 1 };

        if (blockIndex >= 0)
        {
            const auto& doc { docState.getDocument() };

            // blockCount increments by exactly 1 per flush tick (State::flush guarantees this).
            // The component at blockIndex already exists — created in openFile.
            // Apply styling now that the parser has resolved the style for this block.
            const auto& block { doc.blocks[blockIndex] };

            if (block.type == jreng::Markdown::BlockType::Markdown)
            {
                auto* textBlock { static_cast<TextBlock*> (content->getChildComponent (blockIndex)) };
                appendBlockContent (*textBlock, doc, blockIndex);
            }
            else if (block.type == jreng::Markdown::BlockType::Mermaid)
            {
                juce::String mermaidCode { juce::String::fromUTF8 (
                    doc.text + block.contentOffset, block.contentLength) };
                auto* mermaidBlock { static_cast<MermaidBlock*> (content->getChildComponent (blockIndex)) };

                if (mermaidParser != nullptr)
                {
                    mermaidParser->convertToSVG (mermaidCode,
                                                 [mermaidBlock] (const juce::String& svg)
                                                 {
                                                     auto result { MermaidSVGParser::parse (svg) };
                                                     mermaidBlock->setParseResult (std::move (result));
                                                 });
                }
            }
            else if (block.type == jreng::Markdown::BlockType::Table)
            {
                juce::String tableMarkdown { juce::String::fromUTF8 (
                    doc.text + block.contentOffset, block.contentLength) };
                auto* tableBlock { static_cast<TableBlock*> (content->getChildComponent (blockIndex)) };
                tableBlock->setTableMarkdown (tableMarkdown);
            }

            spinnerOverlay.update (blockCount, totalBlocks);
            resized();

            if (blockCount >= totalBlocks)
            {
                spinnerOverlay.hide();
                setBufferedToImage (true);
            }
        }
    }
}

void Component::appendBlockContent (TextBlock& textBlock, const jreng::Markdown::ParsedDocument& doc, int blockIndex)
{
    const auto& block { doc.blocks[blockIndex] };
    juce::String blockContent { juce::String::fromUTF8 (doc.text + block.contentOffset, block.contentLength) };

    if (block.spanCount == 0)
    {
        textBlock.appendStyledText (
            blockContent + "\n",
            juce::FontOptions().withName (docConfig.bodyFamily).withPointHeight (block.fontSize),
            block.colour);
    }
    else
    {
        for (int s { block.spanOffset }; s < block.spanOffset + block.spanCount; ++s)
        {
            const auto& span { doc.spans[s] };
            juce::String spanText { blockContent.substring (span.startOffset, span.endOffset) };

            const juce::String& family { span.fontFamily == 1 ? docConfig.codeFamily : docConfig.bodyFamily };

            bool isBold { (span.style & jreng::Markdown::Bold) != jreng::Markdown::None };
            bool isItalic { (span.style & jreng::Markdown::Italic) != jreng::Markdown::None };

            juce::String style { "Regular" };

            if (isBold and isItalic)
                style = "Bold Italic";
            else if (isBold)
                style = "Bold";
            else if (isItalic)
                style = "Italic";

            textBlock.appendStyledText (
                spanText,
                juce::FontOptions().withName (family).withPointHeight (span.fontSize).withStyle (style),
                span.colour);
        }

        textBlock.appendStyledText (
            "\n", juce::FontOptions().withName (docConfig.bodyFamily).withPointHeight (block.fontSize), block.colour);
    }
}

void Component::clearBlocks()
{
    content->removeAllChildren();
    blocks.clear();
}

void Component::buildDocConfig()
{
    const auto* config { Whelmed::Config::getContext() };
    docConfig.bodyFamily = config->getString (Whelmed::Config::Key::fontFamily);
    docConfig.bodySize = config->getFloat (Whelmed::Config::Key::fontSize);
    docConfig.codeFamily = config->getString (Whelmed::Config::Key::codeFamily);
    docConfig.codeSize = config->getFloat (Whelmed::Config::Key::codeSize);
    docConfig.h1Size = config->getFloat (Whelmed::Config::Key::h1Size);
    docConfig.h2Size = config->getFloat (Whelmed::Config::Key::h2Size);
    docConfig.h3Size = config->getFloat (Whelmed::Config::Key::h3Size);
    docConfig.h4Size = config->getFloat (Whelmed::Config::Key::h4Size);
    docConfig.h5Size = config->getFloat (Whelmed::Config::Key::h5Size);
    docConfig.h6Size = config->getFloat (Whelmed::Config::Key::h6Size);
    docConfig.bodyColour = config->getColour (Whelmed::Config::Key::bodyColour);
    docConfig.codeColour = config->getColour (Whelmed::Config::Key::codeColour);
    docConfig.linkColour = config->getColour (Whelmed::Config::Key::linkColour);
    docConfig.h1Colour = config->getColour (Whelmed::Config::Key::h1Colour);
    docConfig.h2Colour = config->getColour (Whelmed::Config::Key::h2Colour);
    docConfig.h3Colour = config->getColour (Whelmed::Config::Key::h3Colour);
    docConfig.h4Colour = config->getColour (Whelmed::Config::Key::h4Colour);
    docConfig.h5Colour = config->getColour (Whelmed::Config::Key::h5Colour);
    docConfig.h6Colour = config->getColour (Whelmed::Config::Key::h6Colour);
}

void Component::updateLayout()
{
    const int availableWidth { viewport.getMaximumVisibleWidth() };
    int contentHeight { 0 };

    for (int i { 0 }; i < content->getNumChildComponents(); ++i)
    {
        auto* child { content->getChildComponent (i) };

        // First pass: set width to trigger resized / rebuildLayout
        child->setBounds (0, contentHeight, availableWidth, 1);

        const auto* block { dynamic_cast<Block*> (child) };
        const int height { block->getPreferredHeight() };

        // Second pass: set actual height
        child->setBounds (0, contentHeight, availableWidth, height);
        contentHeight += height;
    }

    content->setSize (availableWidth, juce::jmax (1, contentHeight));
}

/**_____________________________END OF NAMESPACE______________________________*/
}// namespace Whelmed
