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
#include "MermaidSVGParser.h"

namespace Whelmed
{ /*____________________________________________________________________________*/

class MermaidBlock : public juce::Component
{
public:
    MermaidBlock();

    void setParseResult (MermaidParseResult&& result);

    void paint (juce::Graphics& g) override;
    void resized() override;

    int getPreferredHeight() const noexcept;

private:
    MermaidParseResult parseResult;
    int preferredHeight { 0 };
    float scale { 1.0f };
    float offsetX { 0.0f };
    float offsetY { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MermaidBlock)
};

/**_____________________________END OF NAMESPACE______________________________*/
}// namespace Whelmed
