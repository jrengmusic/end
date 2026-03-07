namespace jreng::Graphics
{
/*____________________________________________________________________________*/

class Perimeter
{
public:
    Perimeter (const juce::Rectangle<float>& areaToDraw,
               float normalisedValue,
               int division,
               float lineThickness = 2.0f,
               float buttLength = 0.1f,
               float rotationDegree = 0.0f)
        : normal (normalisedValue)
        , stroke (lineThickness)

    {
        if (auto points { Rotary::Angles::getPoints (areaToDraw, division, rotationDegree) };
            points.size())
        {
            if (buttLength)
            {
                auto segmentLength { points.begin()->getDistanceFrom (*(points.begin() + 1)) };
                auto indent { buttLength * segmentLength };

                /** flip the indent direction if it's flipped 180 degree */
                indent = (rotationDegree == 180.0f ? -indent : indent);

                /** add extra two indent point at begin and end */
                auto begin { points.front().withX (points.front().getX() + indent) };
                points.insert (points.begin(), begin);

                auto end { points.back().withX (points.back().getX() - indent) };
                points.push_back (end);
            }

            /** calculate total perimeter length in a single flat line */
            for (auto p = points.begin() + 1; p < points.end(); ++p)
            {
                auto previous { *(p - 1) };
                auto distance { p->getDistanceFrom (previous) };
                perimeter += distance;

                /** and add each lines segment into a vector*/
                lines.push_back ({ previous, *p });
            }
        }
    }

    Perimeter (const std::vector<juce::Point<float>>& points,
               float normalisedValue = 0.0f,
               bool shouldBeCounterClockwise = false,
               float lineThickness = 2.0f,
               float rotationDegree = 0.0f)
        : normal (normalisedValue)
        , stroke (lineThickness)
    {
        /** calculate total perimeter length in a single flat line */
        for (auto p = points.begin() + 1; p < points.end(); ++p)
        {
            auto previous { *(p - 1) };
            auto distance { p->getDistanceFrom (previous) };
            perimeter += distance;

            /** and add each lines segment into a vector*/
            lines.push_back (shouldBeCounterClockwise ? juce::Line<float> (*p, previous)
                                                      : juce::Line<float> (previous, *p));
        }

        if (shouldBeCounterClockwise)
            std::reverse (lines.begin(), lines.end());
    }

    ~Perimeter()
    {
    }

    juce::Path getPath() noexcept
    {
        if (lines.size())
        {
            path.setUsingNonZeroWinding (true);

            /** map 0 to 1 normalised value to get the current position of the end
             of the lines and the current segment */

            float currentLength { Value::map (normal, 0.0f, perimeter) };

            /** currentSegment index start from -1 because the body of do...while loop is executed once before the condition is checked.*/
            int currentSegment { -1 };
            float segmentLength { 0.0f };

            do
            {
                if (currentLength > segmentLength)
                {
                    currentSegment++;
                    segmentLength += lines[currentSegment].getLength();

                    /** we clip minimum value of the distance to 0.0f, to ensure the line
                        don't exceed the segment length */
                    float distance { Value::clipMin (0.0f, segmentLength - currentLength) };
                    auto line { lines[currentSegment].withShortenedEnd (distance) };
                    currentPosition = line.getEnd();
                    path.addLineSegment (line, 1.0f);
                }

            }

            while (segmentLength < currentLength);
        }

        /** If you hit this assertion, there are no lines to draw */

        assert (lines.size() > 0);

        return path;
    }

    juce::DrawablePath getDrawablePath() noexcept
    {
        juce::DrawablePath drawable;
        drawable.setPath (getPath());
        return drawable;
    }

    void draw (juce::Graphics& g, juce::Colour colour, float opacity = 1.0f)
    {
        Drawable::stroke (g, getPath(), colour, stroke, opacity);
    }

    void draw (juce::Graphics& g, const juce::Rectangle<float>& area, juce::Colour colour, float opacity = 1.0f)
    {
        Drawable::stroke (g, area, getPath(), colour, stroke, opacity);
    }

    juce::Point<float> getCurrentPosition()
    {
        return currentPosition;
    }

private:
    juce::Path path;
    float normal;
    float stroke;
    std::vector<juce::Line<float>> lines;
    juce::Point<float> currentPosition;
    float perimeter { 0 };
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Perimeter)
};

/**_____________________________END OF NAMESPACE______________________________*/
} /** namespace jreng::Graphics */
