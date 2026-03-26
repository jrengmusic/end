namespace jreng::Markdown
{ /*____________________________________________________________________________*/

static constexpr float defaultFontSize { 14.0f };
static constexpr float codeFontSize { 12.0f };
static constexpr float h1FontSize { 24.0f };
static constexpr float h2FontSize { 22.0f };
static constexpr float h3FontSize { 20.0f };
static constexpr float h4FontSize { 18.0f };
static constexpr float h5FontSize { 16.0f };
static constexpr float h6FontSize { 14.0f };
static const juce::Colour codeColour { 0xff8b0000u };
static const juce::Colour linkColour { 0xff0000ffu };

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

        auto addMarkdownBlock = [&result, &lines] (int startLine, int endLine)
        {
            if (startLine <= endLine)
            {
                const int startIndex { startLine - 1 };
                const int numLines { endLine - startLine + 1 };
                juce::String text { lines.joinIntoString ("\n", startIndex, numLines) };

                if (text.trim().isNotEmpty())
                {
                    Block block { BlockType::Markdown, text };
                    result.push_back (block);
                }
            }
        };

        int previousEndLine { 0 };

        for (const auto& mermaidBlock : mermaidBlocks)
        {
            addMarkdownBlock (previousEndLine + 1, mermaidBlock.lineStart - 1);

            if (mermaidBlock.code.trim().isNotEmpty())
            {
                Block block { BlockType::Mermaid, mermaidBlock.code };
                result.push_back (block);
            }

            previousEndLine = mermaidBlock.lineEnd;
        }

        if (previousEndLine < lines.size())
            addMarkdownBlock (previousEndLine + 1, lines.size());

        if (mermaidBlocks.empty())
        {
            result.clear();
            addMarkdownBlock (1, lines.size());
        }
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

juce::AttributedString Parser::toAttributedString (const Block& block)
{
    juce::AttributedString result;

    auto units { getUnits (block.content) };

    LineType previousKind { LineType::Blank };

    for (const auto& unit : units)
    {
        if (previousKind != LineType::Blank and unit.kind != previousKind)
            result.append ("\n", juce::FontOptions (defaultFontSize), juce::Colours::white);

        previousKind = unit.kind;

        float fontSize { defaultFontSize };

        if (unit.kind == LineType::Header)
        {
            switch (unit.level)
            {
                case 1:
                    fontSize = h1FontSize;
                    break;
                case 2:
                    fontSize = h2FontSize;
                    break;
                case 3:
                    fontSize = h3FontSize;
                    break;
                case 4:
                    fontSize = h4FontSize;
                    break;
                case 5:
                    fontSize = h5FontSize;
                    break;
                case 6:
                    fontSize = h6FontSize;
                    break;
                default:
                    break;
            }
        }

        auto [spans, links] { inlineSpans (unit.text) };

        if (spans.empty())
        {
            result.append (unit.text + "\n", juce::FontOptions (fontSize), juce::Colours::white);
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

                float size { isCode ? codeFontSize : fontSize };

                int styleFlags { juce::Font::plain };
                if (isBold)
                    styleFlags |= juce::Font::bold;
                if (isItalic)
                    styleFlags |= juce::Font::italic;

                juce::Colour colour { juce::Colours::white };
                if (isCode)
                    colour = codeColour;
                else if (isLink)
                    colour = linkColour;

                result.append (spanText, juce::FontOptions (size, styleFlags), colour);
            }

            result.append ("\n", juce::FontOptions (fontSize), juce::Colours::white);
        }
    }

    return result;
}

/**_____________________________END OF NAMESPACE______________________________*/
}// namespace jreng::Markdown
