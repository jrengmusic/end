namespace jreng::Markdown
{ /*____________________________________________________________________________*/

struct FontConfig
{
    juce::String bodyFamily;
    float bodySize { 14.0f };
    juce::String codeFamily;
    float codeSize { 12.0f };
    float h1Size { 28.0f };
    float h2Size { 24.0f };
    float h3Size { 20.0f };
    float h4Size { 18.0f };
    float h5Size { 16.0f };
    float h6Size { 14.0f };
    juce::Colour bodyColour;
    juce::Colour codeColour;
    juce::Colour linkColour;
    juce::Colour h1Colour;
    juce::Colour h2Colour;
    juce::Colour h3Colour;
    juce::Colour h4Colour;
    juce::Colour h5Colour;
    juce::Colour h6Colour;
};

struct Parser
{
    // ========================================================================
    // Public API
    // ========================================================================

    static Blocks getBlocks (const juce::String& markdown);

    static BlockUnits getUnits (const juce::String& blockContent);

    static std::pair<InlineSpans, TextLinks> inlineSpans (const juce::String& text);

    static std::tuple<LineType, uint8_t, int> classifyLine (const juce::String& line);

    static juce::AttributedString toAttributedString (const Block& block, const FontConfig& fontConfig);

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
