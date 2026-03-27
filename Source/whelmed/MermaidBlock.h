/**
 * @file MermaidBlock.h
 * @brief Renders a mermaid diagram from SVG parse result.
 *
 * Takes a pre-parsed MermaidParseResult and renders primitives
 * scaled to fit the available width. ViewBox determines aspect ratio.
 *
 * @note MESSAGE THREAD.
 * @see MermaidSVGParser
 */

#pragma once
#include <JuceHeader.h>
#include "Block.h"
#include "MermaidSVGParser.h"

namespace Whelmed
{ /*____________________________________________________________________________*/

class MermaidBlock : public Block
{
public:
    MermaidBlock();

    void setParseResult (MermaidParseResult&& result);
    bool hasResult() const noexcept;

    int getPreferredHeight (int width) const noexcept override;
    void paint (juce::Graphics& g, juce::Rectangle<int> area) const override;

private:
    static constexpr int kPlaceholderHeight { 200 };

    MermaidParseResult parseResult;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MermaidBlock)
};

/**_____________________________END OF NAMESPACE______________________________*/
} // namespace Whelmed
