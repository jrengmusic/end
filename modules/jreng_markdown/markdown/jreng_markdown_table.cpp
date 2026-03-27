namespace jreng::Markdown
{ /*____________________________________________________________________________*/

bool lineHasUnescapedPipe (const juce::String& line)
{
    bool inCode { false };
    int codeFenceLen { 0 };
    bool result { false };

    for (int i { 0 }; i < line.length() and not result; ++i)
    {
        auto c { line[i] };

        if (c == '`')
        {
            int fenceLen { 1 };
            while (i + fenceLen < line.length() and line[i + fenceLen] == '`')
                ++fenceLen;

            if (not inCode)
            {
                inCode = true;
                codeFenceLen = fenceLen;
                i += fenceLen - 1;
            }
            else if (fenceLen == codeFenceLen)
            {
                inCode = false;
                codeFenceLen = 0;
                i += fenceLen - 1;
            }
        }
        else if (c == '|' and not inCode)
        {
            int backslashCount { 0 };
            int j { i - 1 };
            while (j >= 0 and line[j] == '\\')
            {
                ++backslashCount;
                --j;
            }

            if (backslashCount % 2 == 0)
                result = true;
        }
    }

    return result;
}

juce::StringArray splitTableRow (const juce::String& line)
{
    juce::StringArray cells;
    juce::String trimmed { line.trim() };

    if (trimmed.startsWith ("|"))
        trimmed = trimmed.substring (1).trim();

    if (trimmed.endsWith ("|"))
        trimmed = trimmed.substring (0, trimmed.length() - 1).trim();

    bool inCode { false };
    int codeFenceLen { 0 };
    int cellStart { 0 };

    for (int i { 0 }; i < trimmed.length(); ++i)
    {
        auto c { trimmed[i] };

        if (c == '`')
        {
            int fenceLen { 1 };
            while (i + fenceLen < trimmed.length() and trimmed[i + fenceLen] == '`')
                ++fenceLen;

            if (not inCode)
            {
                inCode = true;
                codeFenceLen = fenceLen;
                i += fenceLen - 1;
            }
            else if (fenceLen == codeFenceLen)
            {
                inCode = false;
                codeFenceLen = 0;
                i += fenceLen - 1;
            }
        }
        else if (c == '|' and not inCode)
        {
            int backslashCount { 0 };
            int j { i - 1 };
            while (j >= 0 and trimmed[j] == '\\')
            {
                ++backslashCount;
                --j;
            }

            if (backslashCount % 2 == 0)
            {
                juce::String cellText { trimmed.substring (cellStart, i).trim() };
                cells.add (cellText);
                cellStart = i + 1;
            }
        }
    }

    if (cellStart < trimmed.length())
    {
        juce::String cellText { trimmed.substring (cellStart).trim() };
        cells.add (cellText);
    }

    return cells;
}

std::vector<Alignment> parseAlignmentRow (const juce::String& line)
{
    juce::StringArray cells { splitTableRow (line) };
    std::vector<Alignment> aligns;

    bool isValidRow { true };

    for (int i { 0 }; i < cells.size() and isValidRow; ++i)
    {
        juce::String cell { cells.getReference (i).trim() };

        bool leadingColon { cell.startsWith (":") };
        bool trailingColon { cell.endsWith (":") };

        juce::String middle { cell };
        if (leadingColon)
            middle = middle.substring (1);
        if (trailingColon)
            middle = middle.substring (0, middle.length() - 1);
        middle = middle.trim();

        if (middle.isEmpty() or not middle.containsOnly ("-"))
        {
            isValidRow = false;
        }
        else
        {
            Alignment align { Alignment::None };
            if (leadingColon and trailingColon)
                align = Alignment::Center;
            else if (leadingColon)
                align = Alignment::Left;
            else if (trailingColon)
                align = Alignment::Right;

            aligns.push_back (align);
        }
    }

    if (not isValidRow)
        aligns.clear();

    return aligns;
}

Tables parseTablesImpl (const juce::StringArray& lines)
{
    Tables result;
    int i { 0 };

    while (i + 1 < lines.size())
    {
        if (not lineHasUnescapedPipe (lines.getReference (i)))
        {
            ++i;
        }
        else
        {
            juce::StringArray headerCells { splitTableRow (lines.getReference (i)) };
            if (headerCells.isEmpty())
            {
                ++i;
            }
            else
            {
                std::vector<Alignment> aligns { parseAlignmentRow (lines.getReference (i + 1)) };
                if (aligns.size() == 0)
                {
                    ++i;
                }
                else
                {
                    int colCount { juce::jmax (headerCells.size(), jreng::toInt (aligns.size())) };

                    std::vector<juce::StringArray> bodyRawCells;
                    int j { i + 2 };
                    bool hasMoreRows { true };
                    while (j < lines.size() and lineHasUnescapedPipe (lines.getReference (j)) and hasMoreRows)
                    {
                        juce::StringArray rowCells { splitTableRow (lines.getReference (j)) };
                        if (rowCells.size() == 0)
                        {
                            hasMoreRows = false;
                        }
                        else
                        {
                            colCount = juce::jmax (colCount, rowCells.size());
                            bodyRawCells.push_back (rowCells);
                            ++j;
                        }
                    }

                    Table table;
                    table.startLine = i;
                    table.lineCount = j - i;
                    table.headerRowCount = 1;
                    table.bodyRowCount = jreng::toInt (bodyRawCells.size());
                    table.columnCount = colCount;

                    while (headerCells.size() < colCount)
                        headerCells.add (juce::String());
                    while (aligns.size() < colCount)
                        aligns.push_back (Alignment::None);

                    for (int c { 0 }; c < colCount; ++c)
                    {
                        TableCell cell;
                        cell.row = 0;
                        cell.col = c;
                        cell.isHeader = true;
                        cell.align = aligns.at (static_cast<size_t> (c));
                        cell.text = headerCells.getReference (c);
                        auto spanResult { Parser::getInlineSpans (headerCells.getReference (c)) };
                        cell.tokens = std::move (spanResult.spans);
                        cell.tokenCount = spanResult.count;
                        table.cells.push_back (std::move (cell));
                    }

                    for (int r { 0 }; r < jreng::toInt (bodyRawCells.size()); ++r)
                    {
                        juce::StringArray& rowCells { bodyRawCells.at (static_cast<size_t> (r)) };
                        while (rowCells.size() < colCount)
                            rowCells.add (juce::String());

                        for (int c { 0 }; c < colCount; ++c)
                        {
                            TableCell cell;
                            cell.row = r + 1;
                            cell.col = c;
                            cell.isHeader = false;
                            cell.align = aligns.at (static_cast<size_t> (c));
                            cell.text = rowCells.getReference (c);
                            auto spanResult { Parser::getInlineSpans (rowCells.getReference (c)) };
                            cell.tokens = std::move (spanResult.spans);
                            cell.tokenCount = spanResult.count;
                            table.cells.push_back (std::move (cell));
                        }
                    }

                    result.push_back (std::move (table));
                    i = j;
                }
            }
        }
    }

    return result;
}

Tables parseTables (const juce::String& markdown)
{
    auto lines { juce::StringArray::fromLines (markdown) };
    return parseTablesImpl (lines);
}

/**_____________________________END OF NAMESPACE______________________________*/
}// namespace jreng::Markdown
