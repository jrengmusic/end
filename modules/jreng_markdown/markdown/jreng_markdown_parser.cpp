namespace jreng::Markdown
{ /*____________________________________________________________________________*/

int Parser::countConsecutive (const juce::String& s, int start, char target)
{
    int count { 0 };
    while (start + count < s.length() and s[start + count] == target)
        ++count;
    return count;
}

void Parser::flushSegment (TokenizerState& st, int endIndex)
{
    if (endIndex > st.segmentStart)
    {
        InlineSpan span;
        span.startOffset = st.segmentStart;
        span.endOffset = endIndex;
        span.style = st.currentStyle;
        span.linkIndex = -1;

        st.spans->push_back (span);
        st.segmentStart = endIndex;
    }
}

//==============================================================================
Blocks Parser::getBlocks (const juce::String& markdown)
{
    Blocks result;

    if (markdown.isNotEmpty())
    {
        auto mermaidBlocks { jreng::Mermaid::extractBlocks (markdown) };

        juce::StringArray lines;
        lines.addLines (markdown);

        // Scans a range of lines (1-based, inclusive) and emits Markdown and CodeFence
        // blocks into result, detecting generic ``` fences within that range.
        auto scanRange = [&result, &lines] (int startLine, int endLine)
        {
            if (startLine > endLine)
                return;

            juce::StringArray pendingMarkdown;
            int i { startLine - 1 };  // 0-based index

            auto flushPending = [&result, &pendingMarkdown]()
            {
                if (pendingMarkdown.size() > 0)
                {
                    juce::String text { pendingMarkdown.joinIntoString ("\n") };

                    if (text.trim().isNotEmpty())
                    {
                        Block block;
                        block.type = BlockType::Markdown;
                        block.content = text;
                        result.push_back (block);
                    }

                    pendingMarkdown.clear();
                }
            };

            while (i <= endLine - 1)
            {
                const auto& line { lines[i] };
                auto trimmed { line.trim() };

                if (trimmed.startsWith ("```"))
                {
                    flushPending();

                    juce::String language { trimmed.substring (3).trim().toLowerCase() };
                    juce::StringArray fenceLines;
                    ++i;

                    while (i <= endLine - 1)
                    {
                        auto closingTrimmed { lines[i].trim() };

                        if (closingTrimmed.startsWith ("```"))
                            break;

                        fenceLines.add (lines[i]);
                        ++i;
                    }

                    // Skip closing fence line
                    if (i <= endLine - 1)
                        ++i;

                    Block block;
                    block.type = BlockType::CodeFence;
                    block.content = fenceLines.joinIntoString ("\n");
                    block.language = language;
                    result.push_back (block);
                }
                else
                {
                    pendingMarkdown.add (line);
                    ++i;
                }
            }

            flushPending();
        };

        int previousEndLine { 0 };

        for (const auto& mermaidBlock : mermaidBlocks)
        {
            scanRange (previousEndLine + 1, mermaidBlock.lineStart - 1);

            if (mermaidBlock.code.trim().isNotEmpty())
            {
                Block block;
                block.type = BlockType::Mermaid;
                block.content = mermaidBlock.code;
                result.push_back (block);
            }

            previousEndLine = mermaidBlock.lineEnd;
        }

        scanRange (previousEndLine + 1, lines.size());
    }

    return result;
}

std::tuple<LineType, uint8_t, int> Parser::classifyLine (const juce::String& line)
{
    auto trimmed { line.trim() };
    LineType resultKind { LineType::Paragraph };
    uint8_t resultLevel { 0 };
    int resultOffset { 0 };

    if (trimmed.isEmpty())
    {
        resultKind = LineType::Blank;
    }
    else if (trimmed == "---" or trimmed == "***" or trimmed == "___")
    {
        resultKind = LineType::ThematicBreak;
    }
    else
    {
        int leadingSpaces { line.length() - line.trimStart().length() };
        auto content { line.trimStart() };

        if (content.startsWith ("#"))
        {
            auto afterHashes { content.trimCharactersAtStart ("#") };
            int hashCount { content.length() - afterHashes.length() };

            if (hashCount >= 1 and hashCount <= 6 and afterHashes.isNotEmpty()
                and juce::CharacterFunctions::isWhitespace (afterHashes[0]))
            {
                resultKind = LineType::Header;
                resultLevel = static_cast<uint8_t> (hashCount);
                resultOffset = leadingSpaces + hashCount + 1;
            }
            else
            {
                resultOffset = leadingSpaces;
            }
        }
        else if (content.length() >= 2 and (content[0] == '-' or content[0] == '*' or content[0] == '+')
                 and juce::CharacterFunctions::isWhitespace (content[1]))
        {
            resultKind = LineType::ListItem;
            resultLevel = static_cast<uint8_t> ((leadingSpaces / 2) + 1);
            resultOffset = leadingSpaces + 2;
        }
        else if (content.isNotEmpty() and juce::CharacterFunctions::isDigit (content[0]))
        {
            int dotPos { content.indexOfChar ('.') };
            if (dotPos > 0 and dotPos < content.length() - 1
                and content.substring (0, dotPos).containsOnly ("0123456789")
                and juce::CharacterFunctions::isWhitespace (content[dotPos + 1]))
            {
                resultKind = LineType::ListItem;
                resultLevel = static_cast<uint8_t> ((leadingSpaces / 2) + 1);
                resultOffset = leadingSpaces + dotPos + 2;
            }
            else
            {
                resultOffset = leadingSpaces;
            }
        }
        else
        {
            resultOffset = leadingSpaces;
        }
    }

    return { resultKind, resultLevel, resultOffset };
}

BlockUnits Parser::getUnits (const juce::String& blockContent)
{
    BlockUnits result;

    auto lines { juce::StringArray::fromLines (blockContent) };

    BlockUnit currentParagraph;
    bool inParagraph { false };

    for (int i { 0 }; i < lines.size(); ++i)
    {
        const auto& line { lines[i] };
        auto [kind, level, contentStart] { classifyLine (line) };

        if (kind == LineType::Blank)
        {
            if (inParagraph)
            {
                result.push_back (currentParagraph);
                inParagraph = false;
            }
        }
        else
        {
            juce::String content { line.substring (contentStart).trim() };

            if (kind == LineType::Paragraph)
            {
                if (inParagraph)
                {
                    currentParagraph.text = currentParagraph.text + "\n" + content;
                }
                else
                {
                    currentParagraph = { kind, 0, content, i + 1, i + 1 };
                    inParagraph = true;
                }
            }
            else
            {
                if (inParagraph)
                {
                    result.push_back (currentParagraph);
                    inParagraph = false;
                }

                BlockUnit unit;
                unit.kind = kind;
                unit.level = level;
                unit.text = content;
                unit.lineNumberStart = i + 1;
                unit.lineNumberEnd = i + 1;
                result.push_back (unit);
            }
        }
    }

    if (inParagraph)
        result.push_back (currentParagraph);

    return result;
}

std::pair<InlineSpans, TextLinks> Parser::inlineSpans (const juce::String& text)
{
    InlineSpans spans;
    TextLinks links;

    TokenizerState st;
    st.spans = &spans;
    st.links = &links;
    st.segmentStart = 0;

    for (int i { 0 }; i < text.length(); ++i)
    {
        auto c { text[i] };

        switch (st.mode)
        {
            case InlineMode::CodeSpan:
            {
                if (c == '`')
                {
                    int fenceLen { countConsecutive (text, i, '`') };

                    if (fenceLen == st.codeFenceLen)
                    {
                        flushSegment (st, i);
                        st.mode = InlineMode::Normal;
                        st.currentStyle &= ~Code;
                        i += fenceLen - 1;
                        st.segmentStart = i + 1;
                    }
                }
                break;
            }

            case InlineMode::LinkDest:
            {
                if (c == ')')
                {
                    juce::String url { text.substring (st.linkDestStart, i).trim() };

                    TextLink link;
                    link.text = text.substring (st.openBracketPos + 1, i - 1);
                    link.href = url;
                    links.push_back (link);

                    int linkIndex { static_cast<int> (links.size()) - 1 };
                    for (int tokenIdx { st.linkTextStartTokenIndex }; tokenIdx < static_cast<int> (spans.size());
                         ++tokenIdx)
                    {
                        spans.at (tokenIdx).linkIndex = linkIndex;
                    }

                    st.mode = InlineMode::Normal;
                    st.currentStyle &= ~Link;
                    st.segmentStart = i + 1;
                }
                break;
            }

            case InlineMode::Normal:
            case InlineMode::LinkText:
            {
                if (c == '`')
                {
                    flushSegment (st, i);

                    int fenceLen { countConsecutive (text, i, '`') };

                    if (st.mode == InlineMode::Normal)
                    {
                        st.mode = InlineMode::CodeSpan;
                        st.codeFenceLen = fenceLen;
                        st.currentStyle |= Code;
                        i += fenceLen - 1;
                        st.segmentStart = i + 1;
                    }
                    else
                    {
                        st.segmentStart = i + fenceLen;
                        i += fenceLen - 1;
                    }
                }
                else if (c == '*' or c == '_')
                {
                    flushSegment (st, i);

                    int runLen { countConsecutive (text, i, c) };

                    if (runLen >= 2)
                        st.currentStyle ^= Bold;
                    if (runLen >= 1)
                        st.currentStyle ^= Italic;

                    i += runLen - 1;
                    st.segmentStart = i + 1;
                }
                else if (c == '[' and st.mode == InlineMode::Normal)
                {
                    flushSegment (st, i);
                    st.mode = InlineMode::LinkText;
                    st.openBracketPos = i;
                    st.linkTextStartTokenIndex = static_cast<int> (spans.size());
                    st.currentStyle |= Link;
                    st.segmentStart = i + 1;
                }
                else if (c == ']' and st.mode == InlineMode::LinkText)
                {
                    flushSegment (st, i);

                    int j { i + 1 };
                    while (j < text.length() and juce::CharacterFunctions::isWhitespace (text[j]))
                        ++j;

                    if (j < text.length() and text[j] == '(')
                    {
                        st.mode = InlineMode::LinkDest;
                        st.linkDestStart = j + 1;
                        i = j;
                        st.segmentStart = j + 1;
                    }
                    else
                    {
                        st.mode = InlineMode::Normal;
                        st.currentStyle &= ~Link;
                        st.segmentStart = i + 1;
                    }
                }
                break;
            }
        }
    }

    flushSegment (st, text.length());

    return { spans, links };
}

juce::AttributedString Parser::toAttributedString (const Block& block, const FontConfig& fontConfig)
{
    juce::AttributedString result;

    auto units { getUnits (block.content) };

    LineType previousKind { LineType::Blank };

    for (const auto& unit : units)
    {
        if (previousKind != LineType::Blank and unit.kind != previousKind)
            result.append ("\n", juce::FontOptions().withName (fontConfig.bodyFamily).withPointHeight (fontConfig.bodySize), fontConfig.bodyColour);

        previousKind = unit.kind;

        float fontSize { fontConfig.bodySize };

        if (unit.kind == LineType::Header)
        {
            switch (unit.level)
            {
                case 1:
                    fontSize = fontConfig.h1Size;
                    break;
                case 2:
                    fontSize = fontConfig.h2Size;
                    break;
                case 3:
                    fontSize = fontConfig.h3Size;
                    break;
                case 4:
                    fontSize = fontConfig.h4Size;
                    break;
                case 5:
                    fontSize = fontConfig.h5Size;
                    break;
                case 6:
                    fontSize = fontConfig.h6Size;
                    break;
                default:
                    break;
            }
        }

        auto [spans, links] { inlineSpans (unit.text) };

        if (spans.empty())
        {
            juce::Colour unitColour { fontConfig.bodyColour };

            if (unit.kind == LineType::Header)
            {
                switch (unit.level)
                {
                    case 1: unitColour = fontConfig.h1Colour; break;
                    case 2: unitColour = fontConfig.h2Colour; break;
                    case 3: unitColour = fontConfig.h3Colour; break;
                    case 4: unitColour = fontConfig.h4Colour; break;
                    case 5: unitColour = fontConfig.h5Colour; break;
                    case 6: unitColour = fontConfig.h6Colour; break;
                    default: break;
                }
            }

            result.append (unit.text + "\n", juce::FontOptions().withName (fontConfig.bodyFamily).withPointHeight (fontSize), unitColour);
        }
        else
        {
            for (const auto& span : spans)
            {
                juce::String spanText { unit.text.substring (span.startOffset, span.endOffset) };

                bool isBold { (span.style & Bold) != None };
                bool isItalic { (span.style & Italic) != None };
                bool isCode { (span.style & Code) != None };
                bool isLink { (span.style & Link) != None };

                float size { isCode ? fontConfig.codeSize : fontSize };
                const juce::String& family { isCode ? fontConfig.codeFamily : fontConfig.bodyFamily };

                juce::String style { "Regular" };

                if (isBold and isItalic)
                    style = "Bold Italic";
                else if (isBold)
                    style = "Bold";
                else if (isItalic)
                    style = "Italic";

                juce::Colour colour { fontConfig.bodyColour };

                if (isCode)
                {
                    colour = fontConfig.codeColour;
                }
                else if (isLink)
                {
                    colour = fontConfig.linkColour;
                }
                else if (unit.kind == LineType::Header)
                {
                    switch (unit.level)
                    {
                        case 1: colour = fontConfig.h1Colour; break;
                        case 2: colour = fontConfig.h2Colour; break;
                        case 3: colour = fontConfig.h3Colour; break;
                        case 4: colour = fontConfig.h4Colour; break;
                        case 5: colour = fontConfig.h5Colour; break;
                        case 6: colour = fontConfig.h6Colour; break;
                        default: break;
                    }
                }

                result.append (spanText, juce::FontOptions().withName (family).withPointHeight (size).withStyle (style), colour);
            }

            result.append ("\n", juce::FontOptions().withName (fontConfig.bodyFamily).withPointHeight (fontSize), fontConfig.bodyColour);
        }
    }

    return result;
}

/**_____________________________END OF NAMESPACE______________________________*/
}// namespace jreng::Markdown
