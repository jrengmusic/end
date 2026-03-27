#pragma once
#include <JuceHeader.h>
#include "Block.h"
#include "TextBlock.h"
#include "MermaidBlock.h"
#include "TableBlock.h"
#include "MermaidSVGParser.h"

namespace Whelmed
{ /*____________________________________________________________________________*/

/**
    Unified rendering surface for the Whelmed document viewer.

    Analogous to Terminal::Screen — the single Component that renders all content.
    Owns all Block data objects and paints them in one pass with viewport culling.
    Lives inside a juce::Viewport as its viewed component.
*/
class Screen : public juce::Component
{
public:
    Screen();

    /** Builds block data from a parsed markdown document. Applies styles immediately.
        Calls layout(width) internally if width is known. */
    void buildFromDocument (const jreng::Markdown::ParsedDocument& doc,
                            const jreng::Markdown::DocConfig& config);

    /** Recomputes layout for all blocks at the given width. Sets component size. */
    void relayout (int width);

    /** Updates a mermaid block's parse result and triggers relayout. */
    void updateMermaidBlock (int blockIndex, MermaidParseResult&& result);

    /** Clears all blocks. */
    void clear();

    /** Returns the number of blocks. */
    int getBlockCount() const noexcept;

    /** Returns block indices that are mermaid type. */
    juce::Array<int> getMermaidBlockIndices() const;

    // juce::Component
    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    struct BlockEntry
    {
        std::unique_ptr<Block> block;
        jreng::Markdown::BlockType type;
        int y { 0 };
        int height { 0 };
    };

    std::vector<BlockEntry> entries;
    int contentHeight { 0 };
    int layoutWidth { 0 };

    juce::AttributedString buildAttributedString (const jreng::Markdown::ParsedDocument& doc,
                                                   int blockIndex,
                                                   const jreng::Markdown::DocConfig& config) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Screen)
};

/**_____________________________END OF NAMESPACE______________________________*/
} // namespace Whelmed
