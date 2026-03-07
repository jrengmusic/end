namespace jreng::Graphics
{
/*____________________________________________________________________________*/

class Logo
{
public:
    using Points = std::vector<std::vector<juce::Point<float>>>;
    using Colours = std::vector<juce::Colour>;

    Logo()
    {
    }

    void drawStroke (juce::Graphics& g,
                     const juce::Rectangle<float>& area,
                     const Colours& colourScheme = default3,
                     float lineThickness = 1.0f,
                     int index = 3,
                     float normalisedValue = 1.0f)
    {
        juce::DrawableComposite comp;

        if (index < lines.size())
        {
            addLinesPerimeter (comp, colourScheme, lineThickness, index, normalisedValue, index != 1);
        }
        else
        {
            for (int idx = 0; idx <= (index - 1); ++idx)
            {
                addLinesPerimeter (comp, colourScheme, lineThickness, idx, normalisedValue, idx != 1);
            }
        }

        comp.drawWithin (g, area, juce::RectanglePlacement::centred, 1.0f);
    }

    void addLinesPerimeter (juce::DrawableComposite& comp,
                            const Colours& colours,
                            float lineThickness,
                            int index,
                            float normalisedValue,
                            bool shouldBeCounterClockwise = false)
    {
        Perimeter shape { lines[index], normalisedValue, shouldBeCounterClockwise, lineThickness };
        auto drawable { new juce::DrawablePath (shape.getDrawablePath()) };
        drawable->setStrokeFill (colours.at (index));
        drawable->setStrokeType (juce::PathStrokeType (lineThickness));
        comp.addAndMakeVisible (drawable);
    }

    void drawFill (juce::Graphics& g,
                   const juce::Rectangle<float>& area,
                   float normalisedValue = 1.0f,
                   bool shouldBeHold = true,
                   bool shouldBeReversed = false)
    {
        juce::DrawableComposite comp;
        Owner<juce::Drawable> drawables;

        for (auto [points, c] : zip (shapes, colours8))
        {
            juce::Path path;
            path.startNewSubPath (points[0]);
            for (auto& p : points)
            {
                path.lineTo (p);
            }
            path.closeSubPath();

            auto drawable { std::make_unique<juce::DrawablePath>() };
            drawable->setPath (path);
            drawable->setFill (c);
            comp.addChildComponent (*drawable);
            drawables.add (std::move (drawable));
        }

        int index { shouldBeReversed
                        ? toInt (Value::map (normalisedValue, static_cast<float> (drawables.size() - 1), 0.0f))
                        : toInt (Value::map (normalisedValue, 0.0f, static_cast<float> (drawables.size() - 1))) };

        if (shouldBeHold)
            for (int idx = 0; idx <= index; idx++)
            {
                drawables.at (idx)->setVisible (true);
            }
        else
            drawables.at (index)->setVisible (true);

        comp.drawWithin (g, area, juce::RectanglePlacement::centred, 1.0f);
    }

    inline static const Colours default3 {
        juce::Colour { 0xfff0552b },// fuego
        juce::Colour { 0xffce3c28 },// luckyLobster
        juce::Colour { 0xffae2924 },// bloodRush
    };

private:
    Points lines {
        {
            { 12.1f, 0.0f },
            { 43.4f, 10.2f },
            { 43.4f, 100.0f },
            { 12.1f, 89.8f },
            { 12.1f, 0.0f },
        },
        {
            { 51.6f, 0.9f },
            { 82.9f, 11.1f },
            { 43.4f, 55.7f },
            { 12.1f, 45.5f },
            { 51.6f, 0.9f },
        },
        {
            { 87.9f, 87.6f },
            { 56.6f, 77.4f },
            { 27.0f, 28.7f },
            { 58.0f, 39.2f },
            { 87.9f, 87.6f },
        }
    };

    Points shapes {
        {
            { 43.389f, 34.251f },
            { 58.034f, 39.167f },
            { 43.391f, 55.698f },
        },
        {
            { 43.391f, 55.698f },
            { 58.034f, 39.167f },
            { 87.875f, 87.605f },
            { 56.611f, 77.402f },
        },
        {

            { 26.969f, 28.739f },
            { 43.389f, 34.251f },
            { 43.389f, 55.695f },
        },
        {

            { 43.389f, 10.203f },
            { 51.588f, 0.945f },
            { 82.852f, 11.149f },
            { 58.034f, 39.167f },
            { 43.389f, 34.251f },
        },
        {

            { 43.388f, 10.203f },
            { 43.389f, 34.251f },
            { 26.968f, 28.739f },
        },
        {

            { 26.969f, 28.739f },
            { 43.389f, 55.7f },
            { 12.125f, 45.497f },
        },
        {

            { 12.125f, 45.496f },
            { 43.389f, 55.7f },
            { 43.389f, 100.0f },
            { 12.125f, 89.796f },
        },
        {

            { 12.125f, 0.0f },
            { 43.388f, 10.203f },
            { 12.125f, 45.497f },
        },
    };

    Colours colours8 {
        juce::Colour { 0xffa82623 },
        juce::Colour { 0xffaf2824 },
        juce::Colour { 0xffd03d26 },
        juce::Colour { 0xffcf3c28 },
        juce::Colour { 0xffde482a },
        juce::Colour { 0xffdf4828 },
        juce::Colour { 0xffef552b },
        juce::Colour { 0xffef552b },
    };

    std::vector<juce::Rectangle<float>> bounds8 {
        { 43.389f, 34.251f, 14.645f, 21.447f },
        { 43.391f, 39.167f, 44.484f, 48.439f },
        { 26.969f, 28.739f, 16.42f, 26.956f },
        { 43.389f, 0.945f, 39.463f, 38.221f },
        { 26.968f, 10.203f, 16.42f, 24.048f },
        { 12.125f, 28.739f, 31.264f, 26.961f },
        { 12.125f, 45.496f, 31.264f, 54.504f },
        { 12.125f, 0.0f, 31.263f, 45.497f },
    };

    std::vector<juce::Rectangle<float>> bounds3 {
        { 12.1f, 0.0f, 31.3f, 100.0f },
        { 12.1f, 0.9f, 70.7f, 54.7f },
        { 27.0f, 28.7f, 60.9f, 58.9f },
    };

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Logo)
};

/**_____________________________END OF NAMESPACE______________________________*/
} /** namespace jreng::Graphics */
