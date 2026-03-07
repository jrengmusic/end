#pragma once

namespace jreng
{
/*____________________________________________________________________________*/

namespace Graphics
{
/*____________________________________________________________________________*/



/*____________________________________________________________________________*/
inline static void drawRect (juce::Graphics& g,
                             const juce::Rectangle<int>& area,
                             juce::Colour colour = juce::Colours::magenta)
{
    g.setColour (colour);
    g.drawRect (area);
}

inline static void fillRect (juce::Graphics& g,
                             const juce::Rectangle<int>& area,
                             juce::Colour colour = juce::Colours::magenta)
{
    g.setColour (colour);
    g.fillRect (area);
}

inline static void drawRoundedRectangle (juce::Graphics& g, const juce::Rectangle<float>& area, juce::Colour colour = juce::Colours::magenta, float cornerSize = 4.0f, float lineThickness = 2.0f)
{
    g.setColour (colour);
    g.drawRoundedRectangle (area.toFloat().reduced (0.5f * lineThickness), cornerSize, lineThickness);
}

inline static void fillRoundedRectangle (juce::Graphics& g, const juce::Rectangle<float>& area, juce::Colour colour = juce::Colours::magenta, float cornerSize = 4.0f)
{
    g.setColour (colour);
    g.fillRoundedRectangle (area.toFloat(), cornerSize);
}

inline static void drawRoundedRectangle (juce::Graphics& g, const juce::Rectangle<int>& area, juce::Colour colour = juce::Colours::magenta, float cornerSize = 4.0f, float lineThickness = 2.0f)
{
    drawRoundedRectangle (g, area.toFloat(), colour, cornerSize, lineThickness);
}

inline static void fillRoundedRectangle (juce::Graphics& g, const juce::Rectangle<int>& area, juce::Colour colour = juce::Colours::magenta, float cornerSize = 4.0f)
{
    fillRoundedRectangle (g, area.toFloat(), colour, cornerSize);
}

inline static void drawBounds (juce::Graphics& g, juce::Component* component, juce::Colour colour = juce::Colours::magenta)
{
    if (component)
        Graphics::drawRect (g, component->getLocalBounds(), colour);
}

inline static void drawChildrenBounds (juce::Graphics& g, juce::Component* parent, float opacity = 1.0f)
{
    if (parent)
    {
        auto& children { parent->getChildren() };

        std::for_each (children.begin(), children.end(), [&] (auto& child)
                       {
                           if (child)
                           {
                               float hue { Value::normalise<float> (children.indexOf (child), 0, children.size()) };
                               float saturation { 1.0f };
                               float brightness (1.0f);
                               float alpha { 0xff };
                               juce::Colour colour { hue, saturation, brightness, alpha };
                               Graphics::drawRect (g, child->getBounds(), colour.withMultipliedAlpha (opacity));
                           }
                       });
    }
}

inline static void writeChildrenBoundsToSVGFile (juce::Component* parent, const juce::File& fileToSave)
{
    juce::StringArray lines;

    for (auto& child : parent->getChildren())
    {
        lines.add (SVG::Format::rect (child->getBounds(), child->getComponentID()));
    }

    auto svg { SVG::File::getStringToWrite (parent->getWidth(),
                                            parent->getHeight(),
                                            lines.joinIntoString ("\n")) };

    fileToSave.replaceWithText (svg);
}

inline static void writeChildrenBoundsToDesktop (juce::Component* parent, const juce::String& filename = { "bounds.svg" })
{
    writeChildrenBoundsToSVGFile (parent, File::getDesktopDirectory().getChildFile (filename));
}

/**_____________________________END OF NAMESPACE______________________________*/
}// namespace Graphics

template<typename ValueType>
using Points = std::vector<juce::Point<ValueType>>;

template<typename ValueType>
using Lines = std::vector<juce::Line<ValueType>>;

template<typename ValueType>
struct Rectangle
{
    static const Lines<ValueType> getLines (const juce::Rectangle<ValueType>& r, bool isCounterClockwise = false)
    {
        Lines<ValueType> lines {
            { r.getTopLeft(), r.getTopRight() },
            { r.getTopRight(), r.getBottomRight() },
            { r.getBottomRight(), r.getBottomLeft() },
            { r.getBottomLeft(), r.getTopLeft() },
        };

        if (isCounterClockwise)
            std::reverse (lines.begin(), lines.end());

        return lines;
    }
};

/*____________________________________________________________________________*/
struct Drawable
{
    static const void draw (juce::Graphics& g,
                            const juce::Path& path,
                            juce::Colour fillColour,
                            juce::Colour strokeColour,
                            float lineThickness = 1.0f,
                            float opacity = 1.0f)
    {
        auto joint { juce::PathStrokeType::curved };
        auto endCap { juce::PathStrokeType::rounded };
        auto strokeType { juce::PathStrokeType (lineThickness, joint, endCap) };

        juce::DrawablePath d;
        d.setPath (path);
        d.setFill (fillColour);
        d.setStrokeType (strokeType);
        d.setStrokeFill (strokeColour);
        d.draw (g, opacity);
    }

    static const void drawWithin (juce::Graphics& g,
                                  const juce::Rectangle<float>& area,
                                  const juce::Path& path,
                                  juce::Colour fillColour,
                                  juce::Colour strokeColour,
                                  float lineThickness = 1.0f,
                                  float opacity = 1.0f)
    {
        auto joint { juce::PathStrokeType::curved };
        auto endCap { juce::PathStrokeType::rounded };
        auto strokeType { juce::PathStrokeType (lineThickness, joint, endCap) };

        juce::DrawablePath d;
        d.setPath (path);
        d.setFill (fillColour);
        d.setStrokeType (strokeType);
        d.setStrokeFill (strokeColour);
        d.drawWithin (g, area, juce::RectanglePlacement::centred, opacity);
    }

    static const void stroke (juce::Graphics& g,
                              const juce::Path& path,
                              juce::Colour strokeColour,
                              float lineThickness = 1.0f,
                              float opacity = 1.0f)
    {
        draw (g, path, juce::Colour(), strokeColour, lineThickness, opacity);
    }

    static const void stroke (juce::Graphics& g,
                              const juce::Rectangle<float>& area,
                              const juce::Path& path,
                              juce::Colour strokeColour,
                              float lineThickness = 1.0f,
                              float opacity = 1.0f)
    {
        drawWithin (g, area, path, juce::Colour(), strokeColour, lineThickness, opacity);
    }

    static const void fill (juce::Graphics& g,
                            const juce::Path& path,
                            juce::Colour fillColour,
                            float lineThickness = 1.0f,
                            float opacity = 1.0f)
    {
        draw (g, path, fillColour, juce::Colour(), lineThickness, opacity);
    }
};

/**_____________________________END OF NAMESPACE______________________________*/
}// namespace jreng
