namespace jreng::Graphics
{
/*____________________________________________________________________________*/

struct Segment
{
    static void draw (juce::Graphics& g,
                      const juce::Rectangle<float>& areaToDraw,
                      int numSegmentsPerSide = 3,
                      float gap = 0.2f,
                      juce::Colour segmentColour = juce::Colours::magenta,
                      juce::Colour alternateSegmentColour = juce::Colours::yellow,
                      float lineThickness = 2.0f,
                      float opacity = 1.0f)
    {
        auto area { areaToDraw.reduced (lineThickness) };

        Points<float> points;

        for (auto& l : Rectangle<float>::getLines (area))
        {
            auto length { l.getLength() };
            auto segmentLength { length / (numSegmentsPerSide + 1) };
            auto offset { segmentLength * (0.5f * gap) };

            points.push_back (l.getStart());
            points.push_back (l.getPointAlongLine (offset));
            for (auto segment = segmentLength; segment < length; segment += segmentLength)
            {
                points.push_back (l.getPointAlongLine (segment - offset));
                points.push_back (l.getPointAlongLine (segment + offset));
            }
            points.push_back (l.getPointAlongLine (length - offset));
            points.push_back (l.getEnd());
        }

        float r { 1.0f };
        g.setColour (segmentColour);
        for (auto& p : points)
        {
            g.fillRect (juce::Rectangle<float> { p.getX() - r, p.getY() - r, 2 * r, 2 * r });
        }

        //        float cornerSize { 10.f };
        //        juce::Point<float> horizontalControlPoint { area.getTopLeft() };

        //        juce::Path path;
        //        path.startNewSubPath (area.getX(), area.getY() + cornerSize);
        //        path.quadraticTo (horizontalControlPoint, { area.getX() + cornerSize, area.getY() });
        //
        //        Drawable::stroke (g, path, segmentColour, lineThickness, opacity);
    }
};

/**_____________________________END OF NAMESPACE______________________________*/
} /** namespace jreng::Graphics */
