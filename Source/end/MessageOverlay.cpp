#include "end/MessageOverlay.h"

static void drawSolidAxisLine (juce::Graphics& g, juce::Line<float> axis, bool)
{
    g.drawLine (axis);
}

static void drawDashAxisLine (juce::Graphics& g, juce::Line<float> axis, bool)
{
    g.drawDashedLine (axis, dashLengths, static_cast<int> (std::size (dashLengths)));
}

static void drawBracketAxisLine (juce::Graphics& g, juce::Line<float> axis, bool splitVertical)
{
    const auto capStart { axis.getPointAlongLine (textPadding) };
    const auto capEnd { axis.getPointAlongLine (axis.getLength() - textPadding) };

    g.drawLine ({ capStart, capEnd });

    if (splitVertical)
    {
        g.drawLine ({ capStart.x - bracketEndcapLength * 0.5f, capStart.y, capStart.x + bracketEndcapLength * 0.5f, capStart.y });
        g.drawLine ({ capEnd.x - bracketEndcapLength * 0.5f, capEnd.y, capEnd.x + bracketEndcapLength * 0.5f, capEnd.y });
    }
    else
    {
        g.drawLine ({ capStart.x, capStart.y - bracketEndcapLength * 0.5f, capStart.x, capStart.y + bracketEndcapLength * 0.5f });
        g.drawLine ({ capEnd.x, capEnd.y - bracketEndcapLength * 0.5f, capEnd.x, capEnd.y + bracketEndcapLength * 0.5f });
    }
}

void drawMessageOverlay (juce::Graphics& g,
                         juce::Component& overlay,
                         juce::Rectangle<int> bounds,
                         const juce::String& message,
                         int splitLine,
                         bool splitVertical)
{
    static const auto axisLines {
        []
        {
            jam::Function::Map<int, void> axisLines;
            axisLines.add<juce::Graphics&, const juce::Line<float>&, const bool&> (map::OverlayAxisLine::solid, &drawSolidAxisLine);
            axisLines.add<juce::Graphics&, const juce::Line<float>&, const bool&> (map::OverlayAxisLine::dash, &drawDashAxisLine);
            axisLines.add<juce::Graphics&, const juce::Line<float>&, const bool&> (map::OverlayAxisLine::bracket, &drawBracketAxisLine);
            return axisLines;
        }()
    };

    const auto family { ConfigModel::getInstance()->getValue (Id::toType (Id::overlay), Id::fontFamily).toString() };
    const auto size { static_cast<float> (ConfigModel::getInstance()->getValue (Id::toType (Id::overlay), Id::textFontSize)) };
    const juce::Font font { juce::FontOptions (family, size, juce::Font::plain) };

    const auto background { overlay.findColour (juce::Label::backgroundColourId).withAlpha (backgroundAlpha) };
    const auto foreground { overlay.findColour (juce::Label::textColourId) };

    const auto lineStyle { map::OverlayAxisLine::getInstance()->get (
        ConfigModel::getInstance()->getValue (Id::toType (Id::pane), Id::splitLine).toString()) };

    g.setColour (background);
    g.fillRect (bounds);
    g.setFont (font);
    g.setColour (foreground);

    if (splitLine >= 0)
    {
        const bool vertical { splitVertical };

        if (splitVertical)
        {
            const auto x { static_cast<float> (splitLine) };
            const auto top { static_cast<float> (bounds.getY()) };
            const auto bottom { static_cast<float> (bounds.getBottom()) };
            const juce::Line<float> axis { x, top, x, bottom };

            axisLines.get (lineStyle, g, axis, vertical);
        }
        else
        {
            const auto y { static_cast<float> (splitLine) };
            const auto left { static_cast<float> (bounds.getX()) };
            const auto right { static_cast<float> (bounds.getRight()) };
            const juce::Line<float> axis { left, y, right, y };

            axisLines.get (lineStyle, g, axis, vertical);
        }
    }

    if (splitLine >= 0 and message.contains (separator))
    {
        const auto first { message.upToFirstOccurrenceOf (separator, false, false) };
        const auto second { message.fromFirstOccurrenceOf (separator, false, false) };

        const auto region1 { splitVertical ? bounds.withRight (splitLine) : bounds.withBottom (splitLine) };
        const auto region2 { splitVertical ? bounds.withLeft (splitLine) : bounds.withTop (splitLine) };

        g.drawFittedText (first, region1.reduced (textPadding), juce::Justification::centred, maxLines);
        g.drawFittedText (second, region2.reduced (textPadding), juce::Justification::centred, maxLines);
    }
    else
    {
        g.drawFittedText (message, bounds.reduced (textPadding), juce::Justification::centred, maxLines);
    }
}
