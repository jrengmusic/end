namespace jreng::Mermaid
{ /*____________________________________________________________________________*/

Blocks extractBlocks (const juce::String& markdown)
{
    Blocks blocks;

    if (markdown.isNotEmpty())
    {
        juce::StringArray lines;
        lines.addLines (markdown);

        int currentLine { 0 };

        while (currentLine < lines.size())
        {
            const auto& line = lines.getReference (currentLine);

            if (Fence::isStart (line))
            {
                Block block;
                block.lineStart = currentLine + 1;

                const int codeStartIndex { currentLine + 1 };
                int codeEndIndex { codeStartIndex };
                bool foundEndMarker { false };

                ++currentLine;

                while (currentLine < lines.size() and not foundEndMarker)
                {
                    const auto& codeLine = lines.getReference (currentLine);

                    if (Fence::isEnd (codeLine))
                    {
                        block.lineEnd = currentLine + 1;
                        foundEndMarker = true;
                        ++currentLine;
                    }
                    else
                    {
                        ++currentLine;
                        ++codeEndIndex;
                    }
                }

                if (foundEndMarker and codeEndIndex > codeStartIndex)
                {
                    const int numLines { codeEndIndex - codeStartIndex };
                    block.code = lines.joinIntoString ("\n", codeStartIndex, numLines);
                    blocks.push_back (block);
                }
            }
            else
            {
                ++currentLine;
            }
        }
    }

    return blocks;
}

/**_____________________________END OF NAMESPACE______________________________*/
}// namespace jreng::Mermaid
