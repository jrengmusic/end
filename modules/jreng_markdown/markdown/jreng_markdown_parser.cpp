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
    if (endIndex > st.segmentStart and st.doc != nullptr)
    {
        if (st.doc->spanCount < st.doc->spanCapacity)
        {
            InlineSpan span {};
            span.startOffset = st.segmentStart;
            span.endOffset = endIndex;
            span.style = st.currentStyle;
            span.uriOffset = 0;
            span.uriLength = 0;

            st.doc->spans[st.doc->spanCount] = span;
            ++st.doc->spanCount;
        }

        st.segmentStart = endIndex;
    }
}

int Parser::appendText (ParsedDocument& doc, const juce::String& text)
{
    int offset { doc.textSize };
    auto utf8 { text.toUTF8() };
    int numBytes { static_cast<int> (utf8.sizeInBytes()) - 1 };  // exclude null terminator

    if (doc.textSize + numBytes <= doc.textCapacity)
    {
        std::memcpy (doc.text + doc.textSize, utf8.getAddress(), static_cast<size_t> (numBytes));
        doc.textSize += numBytes;
    }

    return offset;
}

void Parser::appendBlock (ParsedDocument& doc, const Block& block)
{
    if (doc.blockCount < doc.blockCapacity)
    {
        doc.blocks[doc.blockCount] = block;
        ++doc.blockCount;
    }
}

void Parser::tokenizeSpans (ParsedDocument& doc, const juce::String& text)
{
    TokenizerState st;
    st.doc = &doc;
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

                    int uriOffset { appendText (doc, url) };
                    auto uriUtf8 { url.toUTF8() };
                    int uriLength { static_cast<int> (uriUtf8.sizeInBytes()) - 1 };

                    for (int spanIdx { st.linkTextStartSpanIndex }; spanIdx < doc.spanCount; ++spanIdx)
                    {
                        doc.spans[spanIdx].uriOffset = uriOffset;
                        doc.spans[spanIdx].uriLength = uriLength;
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
                    st.linkTextStartSpanIndex = doc.spanCount;
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
}

void Parser::emitMarkdownBlock (ParsedDocument& doc, const juce::String& text, int level)
{
    int contentOffset { appendText (doc, text) };
    auto utf8 { text.toUTF8() };
    int contentLength { static_cast<int> (utf8.sizeInBytes()) - 1 };

    int spanOffset { doc.spanCount };
    tokenizeSpans (doc, text);
    int spanCount { doc.spanCount - spanOffset };

    Block block {};
    block.type = BlockType::Markdown;
    block.contentOffset = contentOffset;
    block.contentLength = contentLength;
    block.languageOffset = 0;
    block.languageLength = 0;
    block.spanOffset = spanOffset;
    block.spanCount = spanCount;
    block.level = level;

    appendBlock (doc, block);
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

void Parser::processRange (ParsedDocument& doc,
                           const juce::StringArray& lines,
                           int startLine,
                           int endLine)
{
    if (startLine <= endLine)
    {
        juce::StringArray pendingParagraph;

        auto flushParagraph = [&doc, &pendingParagraph]()
        {
            if (pendingParagraph.size() > 0)
            {
                juce::String text { pendingParagraph.joinIntoString ("\n") };

                if (text.trim().isNotEmpty())
                    emitMarkdownBlock (doc, text, 0);

                pendingParagraph.clear();
            }
        };

        int i { startLine - 1 };  // 0-based index

        while (i <= endLine - 1)
        {
            const auto& line { lines[i] };
            auto trimmed { line.trim() };

            if (trimmed.startsWith ("```"))
            {
                flushParagraph();

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

                juce::String fenceContent { fenceLines.joinIntoString ("\n") };

                int contentOffset { appendText (doc, fenceContent) };
                auto contentUtf8 { fenceContent.toUTF8() };
                int contentLength { static_cast<int> (contentUtf8.sizeInBytes()) - 1 };

                int languageOffset { appendText (doc, language) };
                auto languageUtf8 { language.toUTF8() };
                int languageLength { static_cast<int> (languageUtf8.sizeInBytes()) - 1 };

                Block block {};
                block.type = BlockType::CodeFence;
                block.contentOffset = contentOffset;
                block.contentLength = contentLength;
                block.languageOffset = languageOffset;
                block.languageLength = languageLength;
                block.spanOffset = 0;
                block.spanCount = 0;
                block.level = 0;

                appendBlock (doc, block);
            }
            else
            {
                auto trimmedLine { line.trim() };

                // Table detection: line with pipe followed by separator row
                bool isTable { false };

                if (i + 1 <= endLine - 1 and trimmedLine.contains ("|"))
                {
                    auto nextTrimmed { lines[i + 1].trim() };

                    if (nextTrimmed.contains ("|") and nextTrimmed.contains ("---"))
                        isTable = true;
                }

                if (isTable)
                {
                    flushParagraph();

                    juce::StringArray tableLines;

                    while (i <= endLine - 1 and lines[i].trim().contains ("|"))
                    {
                        tableLines.add (lines[i]);
                        ++i;
                    }

                    juce::String tableContent { tableLines.joinIntoString ("\n") };

                    int contentOffset { appendText (doc, tableContent) };
                    auto utf8 { tableContent.toUTF8() };
                    int contentLength { static_cast<int> (utf8.sizeInBytes()) - 1 };

                    Block block {};
                    block.type = BlockType::Table;
                    block.contentOffset = contentOffset;
                    block.contentLength = contentLength;

                    appendBlock (doc, block);
                }
                else
                {
                    auto [kind, level, contentStart] { classifyLine (line) };

                    if (kind == LineType::Blank)
                    {
                        flushParagraph();
                        ++i;
                    }
                    else if (kind == LineType::Paragraph)
                    {
                        juce::String content { line.substring (contentStart).trim() };
                        pendingParagraph.add (content);
                        ++i;
                    }
                    else
                    {
                        flushParagraph();

                        juce::String content { line.substring (contentStart).trim() };

                        if (kind == LineType::ThematicBreak)
                        {
                            emitMarkdownBlock (doc, "---", 0);
                        }
                        else if (kind == LineType::Header)
                        {
                            emitMarkdownBlock (doc, content, static_cast<int> (level));
                        }
                        else if (kind == LineType::ListItem)
                        {
                            emitMarkdownBlock (doc, content, 0);
                        }

                        ++i;
                    }
                }
            }
        }

        flushParagraph();
    }
}

ParsedDocument Parser::parse (const juce::String& markdown)
{
    ParsedDocument doc;

    int inputSize { markdown.length() };
    doc.textCapacity = juce::jmax (1024, inputSize * 4);    // UTF-8 worst case + URIs
    doc.blockCapacity = juce::jmax (64, inputSize / 50);
    doc.spanCapacity = juce::jmax (64, inputSize / 20);

    doc.text.allocate (static_cast<size_t> (doc.textCapacity), true);
    doc.blocks.allocate (static_cast<size_t> (doc.blockCapacity), true);
    doc.spans.allocate (static_cast<size_t> (doc.spanCapacity), true);

    if (markdown.isNotEmpty())
    {
        auto mermaidBlocks { jreng::Mermaid::extractBlocks (markdown) };

        juce::StringArray lines;
        lines.addLines (markdown);

        int previousEndLine { 0 };

        for (const auto& mermaidBlock : mermaidBlocks)
        {
            processRange (doc, lines, previousEndLine + 1, mermaidBlock.lineStart - 1);

            if (mermaidBlock.code.trim().isNotEmpty())
            {
                int contentOffset { appendText (doc, mermaidBlock.code) };
                auto utf8 { mermaidBlock.code.toUTF8() };
                int contentLength { static_cast<int> (utf8.sizeInBytes()) - 1 };

                Block block {};
                block.type = BlockType::Mermaid;
                block.contentOffset = contentOffset;
                block.contentLength = contentLength;
                appendBlock (doc, block);
            }

            previousEndLine = mermaidBlock.lineEnd;
        }

        processRange (doc, lines, previousEndLine + 1, lines.size());
    }

    return doc;
}

Parser::InlineSpanResult Parser::getInlineSpans (const juce::String& text)
{
    ParsedDocument doc;

    int inputSize { juce::jmax (256, text.length()) };
    doc.textCapacity = inputSize * 4;
    doc.spanCapacity = inputSize / 10;

    doc.text.allocate (static_cast<size_t> (doc.textCapacity), true);
    doc.blocks.allocate (1, true);
    doc.spans.allocate (static_cast<size_t> (doc.spanCapacity), true);

    tokenizeSpans (doc, text);

    InlineSpanResult result;
    result.count = doc.spanCount;

    if (doc.spanCount > 0)
    {
        result.spans.allocate (static_cast<size_t> (doc.spanCount), false);
        std::memcpy (result.spans, doc.spans, static_cast<size_t> (doc.spanCount) * sizeof (InlineSpan));
    }

    return result;
}

/**_____________________________END OF NAMESPACE______________________________*/
}// namespace jreng::Markdown
