#pragma once
#include <JuceHeader.h>
#include "Block.h"
#include "TextBlock.h"
#include "MermaidBlock.h"
#include "TableBlock.h"
#include "MermaidSVGParser.h"

namespace Whelmed
{ /*____________________________________________________________________________*/

class Screen : public juce::Component,
               private juce::Timer
{
public:
    Screen();

    /** Loads a document. Builds blocks until viewportHeight is filled, returns batch count. */
    int load (const jreng::Markdown::ParsedDocument& doc, int viewportHeight);

    /** Builds a single block by index. Idempotent — skips if already built. */
    void build (int blockIndex, const jreng::Markdown::ParsedDocument& doc);

    /** Recomputes layout for all entries. */
    void updateLayout();

    void updateMermaidBlock (int blockIndex, MermaidParseResult&& result);
    void clear();

    int getBlockCount() const noexcept;
    int getBuiltCount() const noexcept;
    juce::Array<int> getMermaidBlockIndices() const;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;
    bool hasPendingMermaids() const noexcept;
    void paintMermaidSpinner (juce::Graphics& g, juce::Rectangle<int> area) const;

    struct BlockEntry
    {
        std::unique_ptr<Block> block;
        jreng::Markdown::BlockType type;
        int y { 0 };
        int height { 0 };
    };

    std::vector<BlockEntry> entries;
    int builtCount { 0 };
    int spinnerFrame { 0 };
    int contentHeight { 0 };
    int layoutWidth { 0 };
    float bodyFontSize { 16.0f };
    float lineHeight { 1.5f };

    juce::AttributedString buildAttributedString (const jreng::Markdown::ParsedDocument& doc, int blockIndex) const;

    std::unique_ptr<Block> createBlock (const jreng::Markdown::ParsedDocument& doc, int blockIndex);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Screen)
};

/**_____________________________END OF NAMESPACE______________________________*/
} // namespace Whelmed
