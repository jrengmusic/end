#include "TextBlock.h"

namespace Whelmed
{ /*____________________________________________________________________________*/

TextBlock::TextBlock (juce::AttributedString attributedText,
                      juce::Colour background)
    : source (std::move (attributedText))
    , backgroundColour (background)
{
}

int TextBlock::getPreferredHeight (int /*width*/) const noexcept
{
    return cachedHeight;
}

void TextBlock::paint (juce::Graphics& g, juce::Rectangle<int> area) const
{
    if (backgroundColour != juce::Colours::transparentBlack)
    {
        g.setColour (backgroundColour);
        g.fillRect (area);
    }

    textLayout.draw (g, area.toFloat());
}

void TextBlock::layout (int width)
{
    if (width != cachedWidth and width > 0)
    {
        textLayout = juce::TextLayout();
        textLayout.createLayout (source, static_cast<float> (width));
        cachedHeight = juce::jmax (1, static_cast<int> (std::ceil (textLayout.getHeight())));
        cachedWidth = width;
    }
}

/**_____________________________END OF NAMESPACE______________________________*/
} // namespace Whelmed
