namespace jreng::Markdown
{ /*____________________________________________________________________________*/

struct DocConfig
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

    static ParsedDocument parse (const juce::String& markdown);

    struct InlineSpanResult
    {
        juce::HeapBlock<InlineSpan> spans;
        int count { 0 };
    };

    static InlineSpanResult getInlineSpans (const juce::String& text);

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
        int linkTextStartSpanIndex { 0 };
        int linkDestStart { 0 };

        ParsedDocument* doc { nullptr };
        int segmentStart { 0 };
    };

    // ========================================================================
    // Internal Helpers
    // ========================================================================

    static std::tuple<LineType, uint8_t, int> classifyLine (const juce::String& line);
    static int countConsecutive (const juce::String& s, int start, char target);
    static void flushSegment (TokenizerState& st, int endIndex);
    static void tokenizeSpans (ParsedDocument& doc, const juce::String& text);
    static int appendText (ParsedDocument& doc, const juce::String& text);
    static void appendBlock (ParsedDocument& doc, const Block& block);
    static void emitMarkdownBlock (ParsedDocument& doc, const juce::String& text, int level);
    static void processRange (ParsedDocument& doc,
                              const juce::StringArray& lines,
                              int startLine,
                              int endLine);
};

/**_____________________________END OF NAMESPACE______________________________*/
}// namespace jreng::Markdown
