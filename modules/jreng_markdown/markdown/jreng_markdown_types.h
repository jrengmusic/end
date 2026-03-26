namespace jreng::Markdown
{ /*____________________________________________________________________________*/

// ============================================================================
// Block Types
// ============================================================================

enum class BlockType
{
    Markdown,   ///< A block of standard markdown text.
    Mermaid,    ///< A fenced code block containing a mermaid diagram.
    CodeFence,  ///< A generic fenced code block (not mermaid).
    Table       ///< A GitHub-flavored markdown table.
};

struct Block
{
    BlockType type;
    juce::String content;
    juce::String language;  // Language tag from code fence opening (e.g. "cpp", "lua"). Empty for untagged fences.
};

using Blocks = std::vector<Block>;

// ============================================================================
// Line Classification
// ============================================================================

enum class LineType
{
    Header,
    ListItem,
    Paragraph,
    ThematicBreak,
    Blank
};

// ============================================================================
// Semantic Unit
// ============================================================================

struct BlockUnit
{
    LineType kind;
    uint8_t level;
    juce::String text;
    int lineNumberStart;
    int lineNumberEnd;
};

using BlockUnits = std::vector<BlockUnit>;

// ============================================================================
// Inline Styles
// ============================================================================

enum InlineStyle : uint16_t
{
    None   = 0,
    Bold   = 1u << 0,
    Italic = 1u << 1,
    Code   = 1u << 2,
    Link   = 1u << 3
};

inline InlineStyle operator| (InlineStyle a, InlineStyle b)
{ return static_cast<InlineStyle> (static_cast<uint16_t> (a) | static_cast<uint16_t> (b)); }

inline InlineStyle operator& (InlineStyle a, InlineStyle b)
{ return static_cast<InlineStyle> (static_cast<uint16_t> (a) & static_cast<uint16_t> (b)); }

inline InlineStyle operator^ (InlineStyle a, InlineStyle b)
{ return static_cast<InlineStyle> (static_cast<uint16_t> (a) ^ static_cast<uint16_t> (b)); }

inline InlineStyle operator~ (InlineStyle a)
{ return static_cast<InlineStyle> (~static_cast<uint16_t> (a)); }

inline InlineStyle& operator|= (InlineStyle& a, InlineStyle b) { a = a | b; return a; }
inline InlineStyle& operator&= (InlineStyle& a, InlineStyle b) { a = a & b; return a; }
inline InlineStyle& operator^= (InlineStyle& a, InlineStyle b) { a = a ^ b; return a; }

struct InlineSpan
{
    int startOffset;
    int endOffset;
    InlineStyle style;
    int linkIndex;
};

struct TextLink
{
    juce::String text;
    juce::String href;
};

using InlineSpans = std::vector<InlineSpan>;
using TextLinks = std::vector<TextLink>;

/**_____________________________END OF NAMESPACE______________________________*/
}// namespace jreng::Markdown
