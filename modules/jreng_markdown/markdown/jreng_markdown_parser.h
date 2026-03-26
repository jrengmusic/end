namespace jreng::Markdown
{ /*____________________________________________________________________________*/

struct Parser
{
    // ========================================================================
    // Public API
    // ========================================================================

    static Blocks getBlocks (const juce::String& markdown);

    static BlockUnits getUnits (const juce::String& blockContent);

    static std::pair<InlineSpans, TextLinks> inlineSpans (const juce::String& text);

    static std::tuple<LineType, uint8_t, int> classifyLine (const juce::String& line);

    static juce::AttributedString toAttributedString (const Block& block);

    //==============================================================================
private:
    // ========================================================================
    // Internal Types
    // ========================================================================

    enum class InlineMode
    {
        Normal,
        CodeSpan,
        LinkText,
        LinkDest
    };

    struct TokenizerState
    {
        InlineMode mode { InlineMode::Normal };
        InlineStyle currentStyle { None };

        int codeFenceLen { 0 };
        int codeStart { 0 };

        int openBracketPos { 0 };
        int linkTextStartTokenIndex { 0 };
        int linkDestStart { 0 };

        std::vector<InlineSpan>* spans { nullptr };
        std::vector<TextLink>* links { nullptr };
        int segmentStart { 0 };
    };

    // ========================================================================
    // Internal Helpers
    // ========================================================================

    static int countConsecutive (const juce::String& s, int start, char target);

    static void flushSegment (TokenizerState& st, int endIndex);
};

/**_____________________________END OF NAMESPACE______________________________*/
}// namespace jreng::Markdown
