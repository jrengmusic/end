#include "MermaidBlock.h"

namespace Whelmed
{ /*____________________________________________________________________________*/

MermaidBlock::MermaidBlock()
{
    setOpaque (false);
}

void MermaidBlock::setParseResult (MermaidParseResult&& result)
{
    parseResult = std::move (result);
    repaint();
}

void MermaidBlock::paint (juce::Graphics& g)
{
    if (not parseResult.ok)
        return;

    g.saveState();
    g.addTransform (juce::AffineTransform::translation (offsetX, offsetY)
                        .followedBy (juce::AffineTransform::scale (scale)));

    for (const auto& prim : parseResult.primitives)
    {
        if (prim.hasFill)
        {
            g.setColour (prim.fillColour);
            g.fillPath (prim.path);
        }

        if (prim.hasStroke)
        {
            g.setColour (prim.strokeColour);
            g.strokePath (prim.path, juce::PathStrokeType (prim.strokeWidth));
        }
    }

    for (const auto& text : parseResult.texts)
    {
        g.setColour (text.colour);
        g.setFont (text.fontSize);
        g.drawText (text.text, text.bounds, text.justification, true);
    }

    g.restoreState();
}

void MermaidBlock::resized()
{
    if (parseResult.ok and parseResult.viewBox.getWidth() > 0.0f)
    {
        float vbWidth  { parseResult.viewBox.getWidth() };
        float vbHeight { parseResult.viewBox.getHeight() };

        scale = static_cast<float> (getWidth()) / vbWidth;
        offsetX = -parseResult.viewBox.getX() * scale;
        offsetY = -parseResult.viewBox.getY() * scale;

        preferredHeight = juce::jmax (1, static_cast<int> (std::ceil (vbHeight * scale)));
    }
}

int MermaidBlock::getPreferredHeight() const noexcept
{
    return preferredHeight;
}

/**_____________________________END OF NAMESPACE______________________________*/
}// namespace Whelmed
