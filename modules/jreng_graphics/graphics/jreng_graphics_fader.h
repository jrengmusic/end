namespace jreng::Graphics
{
/*____________________________________________________________________________*/
struct Fader
{
    /**
        An object to draw fader lines. Create one by creating a LineBuilder object,
        provide its constructor values as an argument to Line object.
     */

    using Number = std::tuple<double, bool, bool, bool>;
    using Numbers = std::vector<Number>;

    struct LineBuilder
    {
        const juce::Rectangle<float> area;
        const juce::NormalisableRange<double> range;
        Numbers numbers;
        int position { Position::right };
        bool isWithPositiveSign { false };
        bool isWithNegativeSign { true };
        float textWidth { 60.0f };
        float textHeight { 20.0f };
        float kerning { 10.0f };
        float baseline { 0.0f };
        float space { 8.0f };
        float infSize { 75.0f };
        float width { 40.0f };
        float centerGap { 20.0f };
        float subWidth { 75.0f };
        float stroke { 1.0f };
        float subStroke { 75.0f };
        juce::Colour colourLine { juce::Colours::grey };
        juce::Colour colourNumber { juce::Colours::grey };
        juce::FontOptions font { "Arial", "Regular", 14.0f};

        enum Position
        {
            left,
            right,
        };
    };

    template<typename FloatType>
    class Line
    {
    public:
        Line (const LineBuilder& newLines)
            : lines (newLines)
        {
            path.setUsingNonZeroWinding (true);
            subPath.setUsingNonZeroWinding (true);
            numbersPath.setUsingNonZeroWinding (true);
        }

        void draw (juce::Graphics& g)
        {
            path.clear();
            subPath.clear();
            numbersPath.clear();
            
            float startY { lines.area.getY() };
            float endY { startY + lines.area.getHeight() };
            float endXLeft { lines.area.getCentreX() - lines.centerGap };
            float startXLeft { endXLeft - lines.width };
            float startXRight { lines.area.getCentreX() + lines.centerGap };
            float endXRight { startXRight + lines.width };

            float startSubXLeft { endXLeft - jreng::Value::fromPercent (lines.subWidth, lines.width) };
            float endSubXRight { startXRight + jreng::Value::fromPercent (lines.subWidth, lines.width) };

            float textX { lines.position ? endXRight + lines.space : startXLeft - lines.space - lines.textWidth };

            for (auto&& [dB, shouldShowRightLine, shouldShowLeftLine, shouldShowNumber] : lines.numbers)
            {
                if (dB >= lines.range.start and dB <= lines.range.end)
                {
                    float y { jreng::Value::map ((float) lines.range.convertTo0to1 (dB), 0.0f, 1.0f, endY, startY) };

                    if (shouldShowLeftLine)
                    {
                        if (shouldShowNumber)
                            path.addLineSegment (juce::Line<float> { startXLeft, y, endXLeft, y }, lines.stroke);
                        else
                            subPath.addLineSegment (juce::Line<float> { startSubXLeft, y, endXLeft, y }, jreng::Value::fromPercent (lines.subStroke, lines.stroke));
                    }

                    if (shouldShowRightLine)
                    {
                        if (shouldShowNumber)
                            path.addLineSegment (juce::Line<float> { startXRight, y, endXRight, y }, lines.stroke);
                        else
                            subPath.addLineSegment (juce::Line<float> { startXRight, y, endSubXRight, y }, jreng::Value::fromPercent (lines.subStroke, lines.stroke));
                    }

                    float baselineOffset { jreng::Value::map (lines.baseline, -100.0f, 100.0f, lines.textHeight, -lines.textHeight) - (lines.textHeight / 2) };

                    juce::Rectangle<float> textArea { textX, y + baselineOffset, lines.textWidth, lines.textHeight };
                    
                    if (shouldShowNumber)
                    {
                        juce::Font font { lines.font };
                        font.setExtraKerningFactor (jreng::Value::fromPercent (lines.kerning, 1.0f));
                        
                        g.setFont (font);
                        juce::GlyphArrangement glyph;

                        auto justification { lines.position ? juce::Justification::centredLeft : juce::Justification::centredRight };

                        if (dB > -100.0)
                        {
                            juce::String textNumber { dB };

                            if (not lines.isWithNegativeSign)
                                textNumber = juce::String (dB).replace ("-", "");

                            if (dB > 0.0 and lines.isWithPositiveSign)
                                textNumber = "+" + textNumber;

                            glyph.addJustifiedText (font, textNumber, textArea.getX(), textArea.getCentreY() + (lines.textHeight / 4), textArea.getWidth(), justification);
                        }
                        else
                        {
                            juce::Rectangle<float> signArea;
                            juce::Rectangle<float> infArea;

                            float infWidth { textArea.getWidth() - lines.textHeight };
                            float signWidth { lines.textHeight / 2 };
                            float scale { jreng::Value::fromPercent (lines.infSize, 1.0f) };

                            if (lines.position)
                            {
                                if (lines.isWithNegativeSign)
                                    signArea = textArea.removeFromLeft (signWidth);

                                infArea = textArea.removeFromLeft (infWidth * scale);
                            }
                            else
                            {
                                infArea = textArea.removeFromRight (infWidth * scale);

                                if (lines.isWithNegativeSign)
                                    signArea = textArea.removeFromRight (signWidth);
                            }

                            auto inf { SVG::getPath (inf_data, infArea) };
                            numbersPath.addPath (inf);

                            glyph.addJustifiedText (font, "-", signArea.getX(), signArea.getCentreY() + (lines.textHeight / 4), signArea.getWidth(), juce::Justification::centredLeft);
                        }

                        glyph.createPath (numbersPath);
                    }
                }

                g.setColour (lines.colourLine);
                g.strokePath (path, juce::PathStrokeType (lines.stroke));
                g.strokePath (subPath, juce::PathStrokeType (jreng::Value::fromPercent (lines.subStroke, lines.stroke)));

                g.setColour (lines.colourNumber);
                g.fillPath (numbersPath);
            }
        }

        juce::String getSVG() const noexcept
        {
            const auto& [marker, markerStroke] { getMarker() };

            juce::StringArray marks {
                SVG::Format::stroke (path, lines.colourLine, lines.stroke, "lines"),
                SVG::Format::fill (numbersPath, lines.colourNumber, "numbers"),
                SVG::Format::stroke (marker, juce::Colours::magenta, markerStroke, "guide"),
            };

            if (subPath.toString().isNotEmpty())
                marks.add (SVG::Format::stroke (subPath, lines.colourLine, jreng::Value::fromPercent (lines.subStroke, lines.stroke), "sublines"));

            return SVG::Format::group (marks.joinIntoString ("\n\t"), "fader_marks");
        }

        std::tuple<juce::Path, double> getMarker() const noexcept
        {
            juce::Path marker;

            const float line { 10.0f };
            auto width { 2 * (line + lines.width + lines.textWidth + lines.centerGap) };
            auto height {  2 * line + lines.area.getHeight()};
            auto area { lines.area.withSizeKeepingCentre (width, height) };

            /** Centre line */
            marker.startNewSubPath ({ lines.area.getCentreX(), lines.area.getY() });
            marker.lineTo ({ lines.area.getCentreX(), lines.area.getBottom() });
            marker.closeSubPath();

            /** Top left */
            auto topLeft { area.getTopLeft() };
            marker.startNewSubPath (topLeft);
            marker.lineTo (topLeft.getX() + line, topLeft.getY());
            marker.startNewSubPath (topLeft);
            marker.lineTo (topLeft.getX(), topLeft.getY() + line);
            marker.closeSubPath();

            /** Top right */
            auto topRight { area.getTopRight() };
            marker.startNewSubPath (topRight);
            marker.lineTo (topRight.getX() - line, topRight.getY());
            marker.startNewSubPath (topRight);
            marker.lineTo (topRight.getX(), topRight.getY() + line);
            marker.closeSubPath();

            /** Bottom left */
            auto bottomLeft { area.getBottomLeft() };
            marker.startNewSubPath (bottomLeft);
            marker.lineTo (bottomLeft.getX() + line, bottomLeft.getY());
            marker.startNewSubPath (bottomLeft);
            marker.lineTo (bottomLeft.getX(), bottomLeft.getY() - line);
            marker.closeSubPath();

            /** Bottom right */
            auto bottomRight { area.getBottomRight() };
            marker.startNewSubPath (bottomRight);
            marker.lineTo (bottomRight.getX() - line, bottomRight.getY());
            marker.startNewSubPath (bottomRight);
            marker.lineTo (bottomRight.getX(), bottomRight.getY() - line);
            marker.closeSubPath();

            const double stroke { 0.3 };
            return { marker, stroke };
        }

    private:
        LineBuilder lines;

        juce::Path path;
        juce::Path subPath;
        juce::Path numbersPath;

        const juce::String inf_data {
            R"KuassaSVG(
      <svg width="100px" height="50px" version="1.1" xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" xml:space="preserve" xmlns:serif="http://www.serif.com/" style="fill-rule:evenodd;clip-rule:evenodd;stroke-linejoin:round;stroke-miterlimit:2;">
          <path d="M65.116,16.732L41.141,40.02C33.165,47.412 20.795,47.258 13.006,39.668C13.006,39.668 13.006,39.668 13.006,39.668C9.05,35.813 6.818,30.524 6.818,25C6.818,19.476 9.05,14.187 13.006,10.332C13.006,10.332 13.006,10.332 13.006,10.332C20.795,2.742 33.165,2.588 41.141,9.98L48.233,16.554L42.29,23.331L34.884,16.732C30.503,12.672 23.708,12.757 19.43,16.925C19.43,16.925 19.43,16.925 19.43,16.925C17.252,19.047 16.024,21.959 16.024,25C16.024,28.041 17.252,30.953 19.43,33.075C19.43,33.075 19.43,33.075 19.43,33.075C23.708,37.243 30.503,37.328 34.884,33.268L58.859,9.98C66.835,2.588 79.205,2.742 86.994,10.332C86.994,10.332 86.994,10.332 86.994,10.332C90.95,14.187 93.182,19.476 93.182,25C93.182,30.524 90.95,35.813 86.994,39.668C86.994,39.668 86.994,39.668 86.994,39.668C80.335,46.157 70.328,47.211 62.58,42.761C61.265,42.006 60.016,41.092 58.859,40.02L51.767,33.446L57.71,26.669L65.116,33.268C69.497,37.328 76.292,37.243 80.57,33.075C80.57,33.075 80.57,33.075 80.57,33.075C82.748,30.953 83.976,28.041 83.976,25C83.976,21.959 82.748,19.047 80.57,16.925C80.57,16.925 80.57,16.925 80.57,16.925C76.292,12.757 69.497,12.672 65.116,16.732Z" style="fill:rgb(0,200,216);"/>
      </svg>
      )KuassaSVG"

        };

        //==============================================================================
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Line)
    };
};
/**_____________________________END OF NAMESPACE______________________________*/
} /** namespace jrengGraphics */
