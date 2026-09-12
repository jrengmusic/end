#include "end/MessageOverlay.h"

// Background fill alpha [0, 1]; applied on top of the window content.
static constexpr float backgroundAlpha { 0.8f };

// Padding in pixels applied to the text bounds.
static constexpr int textPadding { 20 };

// Maximum number of text lines rendered by drawFittedText().
static constexpr int maxLines { 20 };

// Length in pixels of a bracket-style endcap, centred on the axis line.
static constexpr float bracketEndcapLength { 8.0f };

// Dash/gap lengths in pixels for the dash-style split axis line.
static constexpr float dashLengths[] { 6.0f, 4.0f };

// Split token dividing a two-region overlay message.
static const juce::String separator { " | " };

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
                         const bool splitVertical)
{
    static const auto axisLines {
        []
        {
            jam::Function::Map<int, void> lines;
            lines.add<juce::Graphics&, const juce::Line<float>&, const bool&> (map::OverlayAxisLine::solid, &drawSolidAxisLine);
            lines.add<juce::Graphics&, const juce::Line<float>&, const bool&> (map::OverlayAxisLine::dash, &drawDashAxisLine);
            lines.add<juce::Graphics&, const juce::Line<float>&, const bool&> (map::OverlayAxisLine::bracket, &drawBracketAxisLine);
            return lines;
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
        if (splitVertical)
        {
            const auto x { static_cast<float> (splitLine) };
            const auto top { static_cast<float> (bounds.getY()) };
            const auto bottom { static_cast<float> (bounds.getBottom()) };
            const juce::Line<float> axis { x, top, x, bottom };

            axisLines.get (lineStyle, g, axis, splitVertical);
        }
        else
        {
            const auto y { static_cast<float> (splitLine) };
            const auto left { static_cast<float> (bounds.getX()) };
            const auto right { static_cast<float> (bounds.getRight()) };
            const juce::Line<float> axis { left, y, right, y };

            axisLines.get (lineStyle, g, axis, splitVertical);
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
