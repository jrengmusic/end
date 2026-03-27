#include "Screen.h"
#include "config/Config.h"

namespace Whelmed
{ /*____________________________________________________________________________*/

Screen::Screen()
{
    setOpaque (false);
}

void Screen::buildFromDocument (const jreng::Markdown::ParsedDocument& doc,
                                 const jreng::Markdown::DocConfig& config)
{
    clear();

    for (int i { 0 }; i < doc.blockCount; ++i)
    {
        const auto& block { doc.blocks[i] };

        BlockEntry entry;
        entry.type = block.type;

        if (block.type == jreng::Markdown::BlockType::Markdown)
        {
            juce::AttributedString as { buildAttributedString (doc, i, config) };
            entry.block = std::make_unique<TextBlock> (std::move (as));
        }
        else if (block.type == jreng::Markdown::BlockType::CodeFence)
        {
            juce::AttributedString as;
            as.setWordWrap (juce::AttributedString::WordWrap::byWord);

            juce::String code { juce::String::fromUTF8 (doc.text + block.contentOffset,
                                                         block.contentLength) };
            as.append (code,
                       juce::Font (juce::FontOptions()
                                       .withName (config.codeFamily)
                                       .withPointHeight (config.codeSize)),
                       config.codeColour);

            const auto bgColour { Whelmed::Config::getContext()->getColour (Whelmed::Config::Key::codeFenceBackground) };
            entry.block = std::make_unique<TextBlock> (std::move (as), bgColour);
        }
        else if (block.type == jreng::Markdown::BlockType::Mermaid)
        {
            entry.block = std::make_unique<MermaidBlock>();
        }
        else if (block.type == jreng::Markdown::BlockType::Table)
        {
            juce::String tableMarkdown { juce::String::fromUTF8 (doc.text + block.contentOffset,
                                                                   block.contentLength) };
            auto tableBlock { std::make_unique<TableBlock>() };
            tableBlock->setTableMarkdown (tableMarkdown);
            entry.block = std::move (tableBlock);
        }

        entries.push_back (std::move (entry));
    }

    if (getWidth() > 0)
        relayout (getWidth());
}

juce::AttributedString Screen::buildAttributedString (const jreng::Markdown::ParsedDocument& doc,
                                                        int blockIndex,
                                                        const jreng::Markdown::DocConfig& config) const
{
    const auto& block { doc.blocks[blockIndex] };
    juce::String blockContent { juce::String::fromUTF8 (doc.text + block.contentOffset,
                                                         block.contentLength) };

    static constexpr int kMaxHeadingLevel { 6 };

    auto resolveBlockStyle = [&] (const jreng::Markdown::Block& b)
        -> std::pair<float, juce::Colour>
    {
        const float sizes[] { config.bodySize, config.h1Size, config.h2Size,
                              config.h3Size,   config.h4Size, config.h5Size, config.h6Size };
        const juce::Colour colours[] { config.bodyColour, config.h1Colour, config.h2Colour,
                                        config.h3Colour,   config.h4Colour, config.h5Colour,
                                        config.h6Colour };

        const int level { juce::jlimit (0, kMaxHeadingLevel, b.level) };
        return { sizes[level], colours[level] };
    };

    const auto [blockFontSize, blockColour] { resolveBlockStyle (block) };

    juce::AttributedString as;
    as.setWordWrap (juce::AttributedString::WordWrap::byWord);

    if (block.spanCount == 0)
    {
        as.append (blockContent + "\n",
                   juce::Font (juce::FontOptions()
                                   .withName (config.bodyFamily)
                                   .withPointHeight (blockFontSize)),
                   blockColour);
    }
    else
    {
        for (int s { block.spanOffset }; s < block.spanOffset + block.spanCount; ++s)
        {
            const auto& span { doc.spans[s] };
            juce::String spanText { blockContent.substring (span.startOffset, span.endOffset) };

            const bool isCode { (span.style & jreng::Markdown::Code) != jreng::Markdown::None };
            const bool isLink { (span.style & jreng::Markdown::Link) != jreng::Markdown::None };
            const bool isBold { (span.style & jreng::Markdown::Bold) != jreng::Markdown::None };
            const bool isItalic { (span.style & jreng::Markdown::Italic) != jreng::Markdown::None };

            const juce::String& family { isCode ? config.codeFamily : config.bodyFamily };

            juce::Colour spanColour { blockColour };

            if (isCode)
                spanColour = config.codeColour;
            else if (isLink)
                spanColour = config.linkColour;

            juce::String style { "Regular" };

            if (isBold and isItalic)
                style = "Bold Italic";
            else if (isBold)
                style = "Bold";
            else if (isItalic)
                style = "Italic";

            as.append (spanText,
                       juce::Font (juce::FontOptions()
                                       .withName (family)
                                       .withPointHeight (blockFontSize)
                                       .withStyle (style)),
                       spanColour);
        }

        as.append ("\n",
                   juce::Font (juce::FontOptions()
                                   .withName (config.bodyFamily)
                                   .withPointHeight (blockFontSize)),
                   blockColour);
    }

    return as;
}

void Screen::relayout (int width)
{
    if (width <= 0)
        return; // NOTE: this is a precondition guard, not an early return from logic

    layoutWidth = width;
    contentHeight = 0;

    for (auto& entry : entries)
    {
        auto* textBlock { dynamic_cast<TextBlock*> (entry.block.get()) };
        auto* tableBlock { dynamic_cast<TableBlock*> (entry.block.get()) };

        if (textBlock != nullptr)
            textBlock->layout (width);
        else if (tableBlock != nullptr)
            tableBlock->layout (width);

        entry.y = contentHeight;
        entry.height = entry.block->getPreferredHeight (width);
        contentHeight += entry.height;
    }

    setSize (width, juce::jmax (1, contentHeight));
}

void Screen::paint (juce::Graphics& g)
{
    const auto clipBounds { g.getClipBounds() };

    for (const auto& entry : entries)
    {
        if (entry.y + entry.height > clipBounds.getY()
            and entry.y < clipBounds.getBottom())
        {
            const juce::Rectangle<int> area { 0, entry.y, getWidth(), entry.height };
            entry.block->paint (g, area);
        }
    }
}

void Screen::resized()
{
    const int width { getWidth() };

    if (width > 0 and width != layoutWidth)
        relayout (width);
}

void Screen::clear()
{
    entries.clear();
    contentHeight = 0;
    layoutWidth = 0;
}

int Screen::getBlockCount() const noexcept
{
    return static_cast<int> (entries.size());
}

juce::Array<int> Screen::getMermaidBlockIndices() const
{
    juce::Array<int> indices;

    for (int i { 0 }; i < static_cast<int> (entries.size()); ++i)
    {
        if (entries.at (static_cast<size_t> (i)).type == jreng::Markdown::BlockType::Mermaid)
            indices.add (i);
    }

    return indices;
}

void Screen::updateMermaidBlock (int blockIndex, MermaidParseResult&& result)
{
    jassert (blockIndex >= 0 and blockIndex < static_cast<int> (entries.size()));

    auto& entry { entries.at (static_cast<size_t> (blockIndex)) };

    auto* mermaidBlock { dynamic_cast<MermaidBlock*> (entry.block.get()) };

    if (mermaidBlock != nullptr)
    {
        mermaidBlock->setParseResult (std::move (result));

        if (layoutWidth > 0)
            relayout (layoutWidth);
    }
}

/**_____________________________END OF NAMESPACE______________________________*/
} // namespace Whelmed
