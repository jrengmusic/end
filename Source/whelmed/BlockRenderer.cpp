#include <JuceHeader.h>
#include "BlockRenderer.h"

namespace Whelmed
{ /*____________________________________________________________________________*/

BlockRenderer::BlockRenderer (const jreng::Markdown::Block& block,
                              jreng::Typeface& typeface)
    : attributedString { jreng::Markdown::Parser::toAttributedString (block) }
    , typeface { typeface }
{
}

void BlockRenderer::paintGL (jreng::GLGraphics& g) noexcept
{
    layout.draw (g, getLocalBounds().toFloat());
}

void BlockRenderer::paint (juce::Graphics& g)
{
    layout.draw (g, getLocalBounds().toFloat());
}

void BlockRenderer::resized()
{
    rebuildLayout();
}

int BlockRenderer::getPreferredHeight() const noexcept
{
    return preferredHeight;
}

void BlockRenderer::rebuildLayout()
{
    const float maxWidth { static_cast<float> (getWidth()) };

    if (maxWidth > 0.0f)
    {
        layout.createLayout (attributedString, typeface, maxWidth);
        preferredHeight = juce::jmax (1, static_cast<int> (std::ceil (layout.getHeight())));
    }
}

/**_____________________________END OF NAMESPACE______________________________*/
}// namespace Whelmed
