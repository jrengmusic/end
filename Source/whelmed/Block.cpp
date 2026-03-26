#include "Block.h"

namespace Whelmed
{ /*____________________________________________________________________________*/

Block::Block (const juce::AttributedString& attributedString_)
    : attributedString { attributedString_ }
{
}

void Block::paint (juce::Graphics& g)
{
    layout.draw (g, getLocalBounds().toFloat());
}

void Block::resized()
{
    rebuildLayout();
}

int Block::getPreferredHeight() const noexcept
{
    return preferredHeight;
}

void Block::rebuildLayout()
{
    const float maxWidth { static_cast<float> (getWidth()) };

    if (maxWidth > 0.0f)
    {
        layout.createLayout (attributedString, maxWidth);
        preferredHeight = juce::jmax (1, static_cast<int> (std::ceil (layout.getHeight())));
    }
}

/**_____________________________END OF NAMESPACE______________________________*/
}// namespace Whelmed
