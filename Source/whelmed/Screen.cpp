#include "Screen.h"
#include "Tokenizer.h"
#include "../config/WhelmedConfig.h"
#include "../config/Config.h"
#include "../AppState.h"
#include "../SelectionType.h"
#include "../ModalType.h"

namespace Whelmed
{ /*____________________________________________________________________________*/

Screen::Screen() { setOpaque (false); }

int Screen::load (const jreng::Markdown::ParsedDocument& doc, int viewportHeight)
{
    clear();
    bodyFontSize = Whelmed::Config::getContext()->getFloat (Whelmed::Config::Key::fontSize);
    lineHeight = Whelmed::Config::getContext()->getFloat (Whelmed::Config::Key::lineHeight);

    // Create all entries
    for (int i { 0 }; i < doc.blockCount; ++i)
    {
        BlockEntry entry;
        entry.type = doc.blocks[i].type;
        entries.push_back (std::move (entry));
    }

    // Build blocks until viewport is filled
    int initialBatch { 0 };
    int accumulatedHeight { 0 };

    for (int i { 0 }; i < doc.blockCount; ++i)
    {
        auto& entry { entries.at (static_cast<size_t> (i)) };
        entry.block = createBlock (doc, i);
        ++builtCount;
        ++initialBatch;

        // Get real height
        const int width { getWidth() };
        entry.height = entry.block->getPreferredHeight (width);
        accumulatedHeight += entry.height;

        if (accumulatedHeight >= viewportHeight)
            break;
    }

    updateLayout();

    if (hasPendingMermaids())
        startTimerHz (10);

    return initialBatch;
}

void Screen::build (int blockIndex, const jreng::Markdown::ParsedDocument& doc)
{
    jassert (blockIndex >= 0 and blockIndex < static_cast<int> (entries.size()));

    const int width { getWidth() };
    auto& entry { entries.at (static_cast<size_t> (blockIndex)) };

    if (entry.block == nullptr)
    {
        entry.block = createBlock (doc, blockIndex);

        // Update height from estimate to real, adjust total
        const int realHeight { entry.block->getPreferredHeight (width) };
        const int delta { realHeight - entry.height };
        entry.height = realHeight;
        contentHeight += delta;

        // Restack subsequent entries
        const int blockGap { static_cast<int> (bodyFontSize * lineHeight) };
        int y { entry.y + entry.height + blockGap };
        const int lastIndex { static_cast<int> (entries.size()) - 1 };

        for (int i { blockIndex + 1 }; i < static_cast<int> (entries.size()); ++i)
        {
            entries.at (static_cast<size_t> (i)).y = y;
            y += entries.at (static_cast<size_t> (i)).height;

            if (i < lastIndex)
                y += blockGap;
        }

        ++builtCount;
        setSize (getWidth(), juce::jmax (1, contentHeight));
    }
}

std::unique_ptr<Block> Screen::createBlock (const jreng::Markdown::ParsedDocument& doc, int blockIndex)
{
    const int width { getWidth() };
    const auto& block { doc.blocks[blockIndex] };
    std::unique_ptr<Block> result;

    const auto* cfg { Whelmed::Config::getContext() };

    if (block.type == jreng::Markdown::BlockType::Markdown)
    {
        const bool isThematicBreak { block.contentLength == 3
            and std::memcmp (doc.text + block.contentOffset, "---", 3) == 0 };

        if (isThematicBreak)
        {
            juce::AttributedString as;
            as.append ("\n",
                       juce::Font (juce::FontOptions()
                           .withName (cfg->getString (Whelmed::Config::Key::fontFamily))
                           .withPointHeight (cfg->getFloat (Whelmed::Config::Key::fontSize))),
                       juce::Colours::transparentBlack);
            auto textBlock { std::make_unique<TextBlock> (std::move (as)) };

            if (width > 0)
                textBlock->layout (width);

            result = std::move (textBlock);
        }
        else
        {
            juce::AttributedString as { buildAttributedString (doc, blockIndex) };
            auto textBlock { std::make_unique<TextBlock> (std::move (as)) };

            if (width > 0)
                textBlock->layout (width);

            result = std::move (textBlock);
        }
    }
    else if (block.type == jreng::Markdown::BlockType::CodeFence)
    {
        juce::String code { juce::String::fromUTF8 (doc.text + block.contentOffset, block.contentLength) };
        juce::String language { juce::String::fromUTF8 (doc.text + block.languageOffset, block.languageLength) };

        juce::AttributedString as { Whelmed::tokenize (code, language) };

        const auto bgColour { cfg->getColour (Whelmed::Config::Key::codeFenceBackground) };

        auto textBlock { std::make_unique<TextBlock> (std::move (as), bgColour) };

        if (width > 0)
            textBlock->layout (width);

        result = std::move (textBlock);
    }
    else if (block.type == jreng::Markdown::BlockType::Mermaid)
    {
        result = std::make_unique<MermaidBlock>();
    }
    else if (block.type == jreng::Markdown::BlockType::Table)
    {
        juce::String tableMarkdown { juce::String::fromUTF8 (doc.text + block.contentOffset, block.contentLength) };
        auto tableBlock { std::make_unique<TableBlock> (
            juce::Font (juce::FontOptions()
                .withName (cfg->getString (Whelmed::Config::Key::fontFamily))
                .withPointHeight (cfg->getFloat (Whelmed::Config::Key::fontSize))
                .withStyle (cfg->getString (Whelmed::Config::Key::fontStyle)))) };
        tableBlock->setTableMarkdown (tableMarkdown);

        TableBlock::ColourScheme colours;
        colours.background       = cfg->getColour (Whelmed::Config::Key::tableBackground);
        colours.headerBackground = cfg->getColour (Whelmed::Config::Key::tableHeaderBackground);
        colours.rowAlt           = cfg->getColour (Whelmed::Config::Key::tableRowAlt);
        colours.borderColour     = cfg->getColour (Whelmed::Config::Key::tableBorderColour);
        colours.headerText       = cfg->getColour (Whelmed::Config::Key::tableHeaderText);
        colours.cellText         = cfg->getColour (Whelmed::Config::Key::tableCellText);
        tableBlock->setColourScheme (colours);

        if (width > 0)
            tableBlock->layout (width);

        result = std::move (tableBlock);
    }

    return result;
}

juce::AttributedString Screen::buildAttributedString (const jreng::Markdown::ParsedDocument& doc,
                                                      int blockIndex) const
{
    const auto& block { doc.blocks[blockIndex] };
    juce::String blockContent { juce::String::fromUTF8 (doc.text + block.contentOffset, block.contentLength) };

    static constexpr int kMaxHeadingLevel { 6 };

    const auto* cfg { Whelmed::Config::getContext() };

    auto resolveBlockStyle = [&] (const jreng::Markdown::Block& b) -> std::pair<float, juce::Colour>
    {
        const float sizes[] { cfg->getFloat (Whelmed::Config::Key::fontSize),
                              cfg->getFloat (Whelmed::Config::Key::h1Size),
                              cfg->getFloat (Whelmed::Config::Key::h2Size),
                              cfg->getFloat (Whelmed::Config::Key::h3Size),
                              cfg->getFloat (Whelmed::Config::Key::h4Size),
                              cfg->getFloat (Whelmed::Config::Key::h5Size),
                              cfg->getFloat (Whelmed::Config::Key::h6Size) };
        const juce::Colour colours[] { cfg->getColour (Whelmed::Config::Key::bodyColour),
                                       cfg->getColour (Whelmed::Config::Key::h1Colour),
                                       cfg->getColour (Whelmed::Config::Key::h2Colour),
                                       cfg->getColour (Whelmed::Config::Key::h3Colour),
                                       cfg->getColour (Whelmed::Config::Key::h4Colour),
                                       cfg->getColour (Whelmed::Config::Key::h5Colour),
                                       cfg->getColour (Whelmed::Config::Key::h6Colour) };

        const int level { juce::jlimit (0, kMaxHeadingLevel, b.level) };
        return { sizes[level], colours[level] };
    };

    const auto [blockFontSize, blockColour] { resolveBlockStyle (block) };

    juce::AttributedString as;
    as.setWordWrap (juce::AttributedString::WordWrap::byWord);

    if (block.spanCount == 0)
    {
        juce::String fontStyle { block.level > 0 ? "Bold" : cfg->getString (Whelmed::Config::Key::fontStyle) };
        as.append (blockContent + "\n",
                   juce::Font (juce::FontOptions()
                       .withName (cfg->getString (Whelmed::Config::Key::fontFamily))
                       .withPointHeight (blockFontSize)
                       .withStyle (fontStyle)),
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

            const juce::String family { isCode ? cfg->getString (Whelmed::Config::Key::codeFamily)
                                               : cfg->getString (Whelmed::Config::Key::fontFamily) };

            juce::Colour spanColour { blockColour };

            if (isCode)
                spanColour = cfg->getColour (Whelmed::Config::Key::codeColour);
            else if (isLink)
                spanColour = cfg->getColour (Whelmed::Config::Key::linkColour);

            juce::String style { block.level > 0 ? "Bold" : cfg->getString (Whelmed::Config::Key::fontStyle) };

            if (isBold and isItalic)
                style = "Bold Italic";
            else if (isBold)
                style = "Bold";
            else if (isItalic)
                style = "Italic";

            as.append (
                spanText,
                juce::Font (juce::FontOptions().withName (family).withPointHeight (blockFontSize).withStyle (style)),
                spanColour);
        }

        as.append ("\n",
                   juce::Font (juce::FontOptions()
                       .withName (cfg->getString (Whelmed::Config::Key::fontFamily))
                       .withPointHeight (blockFontSize)),
                   blockColour);
    }

    return as;
}

void Screen::setSelection (int anchorBlock, int anchorChar, int cursorBlock, int cursorChar) noexcept
{
    selAnchorBlock = anchorBlock;
    selAnchorChar  = anchorChar;
    selCursorBlock = cursorBlock;
    selCursorChar  = cursorChar;
}

void Screen::setCursor (const Cursor& c) noexcept
{
    cursor = c;
}

void Screen::updateCursor (int blockIndex, int charIndex) noexcept
{
    const auto modal { static_cast<ModalType> (AppState::getContext()->getModalType()) };

    if (modal == ModalType::selection
        and blockIndex >= 0
        and blockIndex < static_cast<int> (entries.size()))
    {
        const auto& entry { entries.at (static_cast<size_t> (blockIndex)) };

        if (entry.block != nullptr)
        {
            auto glyphBounds { entry.block->getGlyphBounds (charIndex) };

            if (not glyphBounds.isEmpty())
            {
                glyphBounds.translate (0.0f, static_cast<float> (entry.y));

                const auto theme { ::Config::getContext()->buildTheme() };

                cursor.bounds    = glyphBounds;
                cursor.colour    = theme.selectionCursorColour;
                cursor.codepoint = theme.cursorCodepoint;
                cursor.shape     = 0;
                cursor.visible   = true;
                cursor.blinkOn   = true;
            }
            else
            {
                cursor.visible = false;
            }
        }
        else
        {
            cursor.visible = false;
        }
    }
    else
    {
        cursor.visible = false;
    }
}

int Screen::getBlockTextLength (int blockIndex) const noexcept
{
    if (blockIndex >= 0 and blockIndex < static_cast<int> (entries.size()))
    {
        const auto& entry { entries.at (static_cast<size_t> (blockIndex)) };

        if (entry.block != nullptr)
            return entry.block->getTextLength();
    }

    return 0;
}

int Screen::getBlockLineCount (int blockIndex) const noexcept
{
    if (blockIndex >= 0 and blockIndex < static_cast<int> (entries.size()))
    {
        const auto& entry { entries.at (static_cast<size_t> (blockIndex)) };

        if (entry.block != nullptr)
            return entry.block->getLineCount();
    }

    return 1;
}

int Screen::getBlockLineForChar (int blockIndex, int charIndex) const noexcept
{
    if (blockIndex >= 0 and blockIndex < static_cast<int> (entries.size()))
    {
        const auto& entry { entries.at (static_cast<size_t> (blockIndex)) };

        if (entry.block != nullptr)
            return entry.block->getLineForChar (charIndex);
    }

    return 0;
}

juce::Range<int> Screen::getBlockLineCharRange (int blockIndex, int lineIndex) const noexcept
{
    if (blockIndex >= 0 and blockIndex < static_cast<int> (entries.size()))
    {
        const auto& entry { entries.at (static_cast<size_t> (blockIndex)) };

        if (entry.block != nullptr)
            return entry.block->getLineCharRange (lineIndex);
    }

    return { 0, 0 };
}

int Screen::getBlockCharForLine (int blockIndex, int lineIndex, float targetX) const noexcept
{
    if (blockIndex >= 0 and blockIndex < static_cast<int> (entries.size()))
    {
        const auto& entry { entries.at (static_cast<size_t> (blockIndex)) };

        if (entry.block != nullptr)
            return entry.block->getCharForLine (lineIndex, targetX);
    }

    return 0;
}

float Screen::getBlockCharX (int blockIndex, int charIndex) const noexcept
{
    if (blockIndex >= 0 and blockIndex < static_cast<int> (entries.size()))
    {
        const auto& entry { entries.at (static_cast<size_t> (blockIndex)) };

        if (entry.block != nullptr)
            return entry.block->getCharX (charIndex);
    }

    return 0.0f;
}

float Screen::getCursorPixelY() const noexcept
{
    if (cursor.visible)
        return cursor.bounds.getY();

    return -1.0f;
}

juce::Rectangle<float> Screen::getCursorBounds() const noexcept
{
    return cursor.bounds;
}

void Screen::hideCursor() noexcept
{
    cursor.visible = false;
}

Screen::HitResult Screen::hitTestAt (float x, float y) const noexcept
{
    for (int i { 0 }; i < static_cast<int> (entries.size()); ++i)
    {
        const auto& entry { entries.at (static_cast<size_t> (i)) };

        if (entry.block != nullptr
            and y >= static_cast<float> (entry.y)
            and y < static_cast<float> (entry.y + entry.height))
        {
            const float localY { y - static_cast<float> (entry.y) };
            const int charIdx { entry.block->hitTest (x, localY) };

            if (charIdx >= 0)
                return { i, charIdx };
        }
    }

    return { -1, -1 };
}

void Screen::setStateTree (juce::ValueTree tree) noexcept
{
    stateTree = tree;
}

void Screen::mouseDown (const juce::MouseEvent& event)
{
    {
        auto* parent { getParentComponent() };

        if (parent != nullptr)
        {
            auto* pane { parent->getParentComponent() };

            if (pane != nullptr)
                pane->grabKeyboardFocus();
        }
    }

    clickCount = event.getNumberOfClicks();
    const auto hit { hitTestAt (static_cast<float> (event.x), static_cast<float> (event.y)) };

    if (hit.block >= 0 and stateTree.isValid())
    {
        if (clickCount == 3)
        {
            const int lineIndex { getBlockLineForChar (hit.block, hit.character) };
            const auto lineRange { getBlockLineCharRange (hit.block, lineIndex) };

            AppState::getContext()->setSelectionType (static_cast<int> (SelectionType::visualLine));
            stateTree.setProperty (App::ID::selAnchorBlock, hit.block, nullptr);
            stateTree.setProperty (App::ID::selAnchorChar, lineRange.getStart(), nullptr);
            stateTree.setProperty (App::ID::selCursorBlock, hit.block, nullptr);
            stateTree.setProperty (App::ID::selCursorChar, lineRange.getEnd() - 1, nullptr);
            dragActive = false;
        }
        else if (clickCount == 2)
        {
            const int textLen { getBlockTextLength (hit.block) };

            if (textLen > 0 and hit.block < static_cast<int> (entries.size()))
            {
                const auto& entry { entries.at (static_cast<size_t> (hit.block)) };

                if (entry.block != nullptr)
                {
                    const juce::String text { entry.block->getText() };
                    int wordStart { hit.character };
                    int wordEnd { hit.character };

                    while (wordStart > 0 and text[wordStart - 1] > ' ')
                        --wordStart;

                    while (wordEnd < textLen - 1 and text[wordEnd + 1] > ' ')
                        ++wordEnd;

                    AppState::getContext()->setSelectionType (static_cast<int> (SelectionType::visual));
                    stateTree.setProperty (App::ID::selAnchorBlock, hit.block, nullptr);
                    stateTree.setProperty (App::ID::selAnchorChar, wordStart, nullptr);
                    stateTree.setProperty (App::ID::selCursorBlock, hit.block, nullptr);
                    stateTree.setProperty (App::ID::selCursorChar, wordEnd, nullptr);
                }
            }

            dragActive = false;
        }
        else
        {
            AppState::getContext()->setSelectionType (static_cast<int> (SelectionType::none));
            AppState::getContext()->setModalType (static_cast<int> (ModalType::none));
            dragAnchor = hit;
            dragActive = false;
            hideCursor();
        }

        repaint();
    }
}

void Screen::mouseDrag (const juce::MouseEvent& event)
{
    if (dragAnchor.block >= 0 and clickCount == 1 and stateTree.isValid())
    {
        const auto hit { hitTestAt (static_cast<float> (event.x), static_cast<float> (event.y)) };

        if (hit.block >= 0)
        {
            const bool crossedThreshold { hit.block != dragAnchor.block
                                          or hit.character != dragAnchor.character };

            if (crossedThreshold and not dragActive)
            {
                AppState::getContext()->setSelectionType (static_cast<int> (SelectionType::visual));
                stateTree.setProperty (App::ID::selAnchorBlock, dragAnchor.block, nullptr);
                stateTree.setProperty (App::ID::selAnchorChar, dragAnchor.character, nullptr);
                dragActive = true;
            }

            if (dragActive)
            {
                stateTree.setProperty (App::ID::selCursorBlock, hit.block, nullptr);
                stateTree.setProperty (App::ID::selCursorChar, hit.character, nullptr);
                repaint();
            }
        }
    }
}

void Screen::mouseUp (const juce::MouseEvent& event)
{
    juce::ignoreUnused (event);
    dragActive = false;
}

int Screen::getFirstVisibleBlock (int viewportY) const noexcept
{
    for (int i { 0 }; i < static_cast<int> (entries.size()); ++i)
    {
        const auto& entry { entries.at (static_cast<size_t> (i)) };

        if (entry.y + entry.height > viewportY and entry.block != nullptr)
            return i;
    }

    return 0;
}

juce::String Screen::extractText (int startBlock, int startChar, int endBlock, int endChar) const
{
    juce::String result;
    const int blockCount { static_cast<int> (entries.size()) };

    for (int i { juce::jmax (0, startBlock) }; i <= juce::jmin (endBlock, blockCount - 1); ++i)
    {
        const auto& entry { entries.at (static_cast<size_t> (i)) };

        if (entry.block != nullptr)
        {
            const juce::String blockText { entry.block->getText() };
            const int textLen { blockText.length() };

            int from { 0 };
            int to   { textLen };

            if (i == startBlock)
                from = juce::jlimit (0, textLen, startChar);

            if (i == endBlock)
                to = juce::jlimit (0, textLen, endChar + 1);

            if (to > from)
                result += blockText.substring (from, to);

            if (i < endBlock)
                result += "\n";
        }
    }

    return result;
}

void Screen::updateLayout()
{
    const int width { getWidth() };

    if (width > 0)
    {
        layoutWidth = width;
        contentHeight = 0;

        const int blockGap { static_cast<int> (bodyFontSize * lineHeight) };
        const int lastIndex { static_cast<int> (entries.size()) - 1 };

        for (int i { 0 }; i < static_cast<int> (entries.size()); ++i)
        {
            auto& entry { entries.at (static_cast<size_t> (i)) };

            if (entry.block != nullptr)
            {
                auto* textBlock { dynamic_cast<TextBlock*> (entry.block.get()) };
                auto* tableBlock { dynamic_cast<TableBlock*> (entry.block.get()) };

                if (textBlock != nullptr)
                    textBlock->layout (width);
                else if (tableBlock != nullptr)
                    tableBlock->layout (width);

                entry.y = contentHeight;
                entry.height = entry.block->getPreferredHeight (width);
            }
            else
            {
                entry.y = contentHeight;
                // Keep estimated height for non-built blocks
            }

            contentHeight += entry.height;

            if (i < lastIndex)
                contentHeight += blockGap;
        }

        setSize (width, juce::jmax (1, contentHeight));
    }
}

void Screen::paint (juce::Graphics& g)
{
    const auto clipBounds { g.getClipBounds() };

    for (int i { 0 }; i < static_cast<int> (entries.size()); ++i)
    {
        const auto& entry { entries.at (static_cast<size_t> (i)) };

        if (entry.block != nullptr and entry.y + entry.height > clipBounds.getY() and entry.y < clipBounds.getBottom())
        {
            const juce::Rectangle<int> area { 0, entry.y, getWidth(), entry.height };
            entry.block->paint (g, area);

            if (entry.type == jreng::Markdown::BlockType::Mermaid)
            {
                auto* mermaid { dynamic_cast<MermaidBlock*> (entry.block.get()) };

                if (mermaid != nullptr and not mermaid->hasResult())
                    paintMermaidSpinner (g, area);
            }
        }
    }

    const auto selType { static_cast<SelectionType> (AppState::getContext()->getSelectionType()) };

    if (selType != SelectionType::none)
    {
        const auto selColour { Whelmed::Config::getContext()->getColour (Whelmed::Config::Key::selectionColour) };
        g.setColour (selColour);

        const bool anchorFirst { selAnchorBlock < selCursorBlock
            or (selAnchorBlock == selCursorBlock and selAnchorChar <= selCursorChar) };
        const int startBlock { anchorFirst ? selAnchorBlock : selCursorBlock };
        const int startChar  { anchorFirst ? selAnchorChar  : selCursorChar };
        const int endBlock   { anchorFirst ? selCursorBlock : selAnchorBlock };
        const int endChar    { anchorFirst ? selCursorChar  : selAnchorChar };

        for (int i { startBlock }; i <= endBlock and i < static_cast<int> (entries.size()); ++i)
        {
            const auto& entry { entries.at (static_cast<size_t> (i)) };

            if (entry.block != nullptr and entry.y + entry.height > clipBounds.getY() and entry.y < clipBounds.getBottom())
            {
                int blockStart { 0 };
                int blockEnd   { entry.block->getTextLength() };

                if (selType == SelectionType::visual)
                {
                    if (i == startBlock)
                        blockStart = startChar;

                    if (i == endBlock)
                        blockEnd = endChar + 1;
                }

                if (blockEnd > blockStart)
                {
                    auto rects { entry.block->getSelectionRects (blockStart, blockEnd) };
                    rects.offsetAll (0.0f, static_cast<float> (entry.y));
                    g.fillRectList (rects);
                }
            }
        }
    }

    // Cursor rendering
    if (cursor.visible and cursor.blinkOn)
    {
        const auto cursorBounds { cursor.bounds };

        if (cursorBounds.getY() + cursorBounds.getHeight() > static_cast<float> (clipBounds.getY())
            and cursorBounds.getY() < static_cast<float> (clipBounds.getBottom()))
        {
            g.setColour (cursor.colour);

            static constexpr float barWidth { 2.0f };
            g.fillRect (cursorBounds.getX(), cursorBounds.getY(),
                        barWidth, cursorBounds.getHeight());
        }
    }
}

void Screen::resized()
{
    if (getWidth() > 0 and getWidth() != layoutWidth)
        updateLayout();
}

void Screen::clear()
{
    entries.clear();
    builtCount = 0;
    contentHeight = 0;
    layoutWidth = 0;
}

int Screen::getBlockCount() const noexcept { return static_cast<int> (entries.size()); }

int Screen::getBuiltCount() const noexcept { return builtCount; }

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
            updateLayout();

        if (not hasPendingMermaids())
            stopTimer();
    }
}

void Screen::timerCallback()
{
    spinnerFrame = (spinnerFrame + 1) % 10;
    repaint();
}

bool Screen::hasPendingMermaids() const noexcept
{
    bool pending { false };

    for (const auto& entry : entries)
    {
        if (entry.type == jreng::Markdown::BlockType::Mermaid and entry.block != nullptr)
        {
            auto* mermaid { dynamic_cast<MermaidBlock*> (entry.block.get()) };

            if (mermaid != nullptr and not mermaid->hasResult())
                pending = true;
        }
    }

    return pending;
}

void Screen::paintMermaidSpinner (juce::Graphics& g, juce::Rectangle<int> area) const
{
    static constexpr std::array<juce::juce_wchar, 10> frames {
        0x280B,// ⠋
        0x2819,// ⠙
        0x2839,// ⠹
        0x2838,// ⠸
        0x283C,// ⠼
        0x2834,// ⠴
        0x2826,// ⠦
        0x2827,// ⠧
        0x2807,// ⠇
        0x280F // ⠏
    };

    const auto* loaderCfg { ::Config::getContext() };

    const auto spinnerColour { loaderCfg->getColour (::Config::Key::statusBarSpinnerColour) };
    const auto textColour { loaderCfg->getColour (::Config::Key::coloursStatusBarLabelFg) };

    g.setFont (juce::FontOptions()
                   .withName (loaderCfg->getString (::Config::Key::statusBarFontFamily))
                   .withPointHeight (loaderCfg->getFloat (::Config::Key::statusBarFontSize))
                   .withStyle (loaderCfg->getString (::Config::Key::statusBarFontStyle)));

    const juce::String spinnerChar { juce::String::charToString (frames.at ((size_t) spinnerFrame)) };
    const juce::String labelText { " Loading Diagram" };
    const juce::String fullText { spinnerChar + labelText };

    juce::GlyphArrangement gaFull;
    gaFull.addLineOfText (g.getCurrentFont(), fullText, 0.0f, 0.0f);
    const float fullWidth { gaFull.getBoundingBox (0, -1, false).getWidth() };

    juce::GlyphArrangement gaSpinner;
    gaSpinner.addLineOfText (g.getCurrentFont(), spinnerChar, 0.0f, 0.0f);
    const float spinnerCharWidth { gaSpinner.getBoundingBox (0, -1, false).getWidth() };

    const float centreX { static_cast<float> (area.getCentreX()) };
    const float centreY { static_cast<float> (area.getCentreY()) };
    const float originX { centreX - fullWidth * 0.5f };

    g.setColour (spinnerColour);
    g.drawSingleLineText (spinnerChar, static_cast<int> (originX), static_cast<int> (centreY), juce::Justification::left);

    g.setColour (textColour);
    g.drawSingleLineText (labelText, static_cast<int> (originX + spinnerCharWidth), static_cast<int> (centreY), juce::Justification::left);
}

/**_____________________________END OF NAMESPACE______________________________*/
} // namespace Whelmed
