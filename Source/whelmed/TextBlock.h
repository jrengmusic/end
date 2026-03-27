#pragma once
#include <JuceHeader.h>
#include "Block.h"

namespace Whelmed
{ /*____________________________________________________________________________*/

class TextBlock : public Block
{
public:
    explicit TextBlock (juce::AttributedString attributedText,
                        juce::Colour background = juce::Colours::transparentBlack);

    int getPreferredHeight (int width) const noexcept override;
    void paint (juce::Graphics& g, juce::Rectangle<int> area) const override;

    /** Re-creates the internal layout for a new width. Call when viewport width changes. */
    void layout (int width);

private:
    juce::AttributedString source;
    juce::TextLayout textLayout;
    juce::Colour backgroundColour;
    int cachedHeight { 0 };
    int cachedWidth { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TextBlock)
};

/**_____________________________END OF NAMESPACE______________________________*/
} // namespace Whelmed
