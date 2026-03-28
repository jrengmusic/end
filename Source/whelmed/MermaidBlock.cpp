#include "MermaidBlock.h"

namespace Whelmed
{ /*____________________________________________________________________________*/

MermaidBlock::MermaidBlock() = default;

void MermaidBlock::setParseResult (MermaidParseResult&& result)
{
    parseResult = std::move (result);
}

bool MermaidBlock::hasResult() const noexcept
{
    return parseResult.ok;
}

int MermaidBlock::getPreferredHeight (int width) const noexcept
{
    int height { kPlaceholderHeight };

    if (parseResult.ok and parseResult.viewBox.getWidth() > 0.0f and width > 0)
    {
        const float displayWidth { static_cast<float> (width) };
        height = juce::jmax (1, static_cast<int> (std::ceil (
            (displayWidth / parseResult.viewBox.getWidth()) * parseResult.viewBox.getHeight())));
    }

    return height;
}

void MermaidBlock::paint (juce::Graphics& g, juce::Rectangle<int> area) const
{
    if (parseResult.ok and parseResult.viewBox.getWidth() > 0.0f)
    {
        const float vbWidth { parseResult.viewBox.getWidth() };
        const float scale   { static_cast<float> (area.getWidth()) / vbWidth };

        g.saveState();
        g.addTransform (juce::AffineTransform::translation (-parseResult.viewBox.getX(),
                                                              -parseResult.viewBox.getY())
                            .followedBy (juce::AffineTransform::scale (scale))
                            .followedBy (juce::AffineTransform::translation (
                                static_cast<float> (area.getX()),
                                static_cast<float> (area.getY()))));

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
}

/**_____________________________END OF NAMESPACE______________________________*/
} // namespace Whelmed
