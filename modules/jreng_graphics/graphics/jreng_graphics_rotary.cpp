namespace jreng::Graphics
{
/*____________________________________________________________________________*/

Rotary::Angles::Angles (int div, float arc, float rotation)
{
    for (float i = 0.0f; i < static_cast<float> (div); ++i)
    {
        const float angle { Value::map (i, 0.0f, static_cast<float> (div - 1), -arc, arc) };
        push_back (rotation + angle);
    }
}

Points<float> Rotary::Angles::getPoints (const juce::Rectangle<float>& area, int div, float rotationDegree)
{
    std::vector<juce::Point<float>> points;

    /** if the shape is rectangle */
    if (div == 4)
    {
        points.push_back (area.getBottomLeft());
        points.push_back (area.getTopLeft());
        points.push_back (area.getTopRight());
        points.push_back (area.getBottomRight());

        return points;
    }

    auto centre { area.getCentre() };
    auto radius { area.getAspectRatio() > 1.0 ? area.getHeight() / 2 : area.getWidth() / 2 };
    auto arc { juce::degreesToRadians (180.0f - 0.5f * (360.f / (float) div)) };

    Angles angles (div, arc, juce::degreesToRadians (rotationDegree));

    for (auto& angle : angles)
    {
        points.push_back (centre.getPointOnCircumference (radius, angle));
    }

    return points;
}

Lines<float> Rotary::Angles::getLines (const juce::Rectangle<float>& area,
                                       int div,
                                       bool shouldBeClosed,
                                       float rotationDegree)
{
    std::vector<juce::Line<float>> lines;

    if (auto points { Angles::getPoints (area, div, rotationDegree) };
        points.size())
    {
        for (auto p = points.begin() + 1; p < points.end(); ++p)
        {
            auto previous { (p - 1) };
            lines.push_back ({ *previous, *p });
        }

        if (shouldBeClosed)
            lines.push_back ({ points.back(), points.front() });
    }

    return lines;
}

juce::Path Rotary::Angles::getLinePath (const juce::Rectangle<float>& area,
                                        int div,
                                        bool shouldBeClosed,
                                        float rotationDegree,
                                        float lineThickness)
{
    juce::Path path;

    if (auto lines { Angles::getLines (area, div, shouldBeClosed, rotationDegree) };
        lines.size())
    {
        for (auto& line : lines)
            path.addLineSegment (line, lineThickness);
    }

    return path;
}

void Rotary::Angles::drawLines (juce::Graphics& g,
                                juce::Colour colour,
                                const juce::Rectangle<float>& area,
                                int div,
                                bool shouldBeClosed,
                                float rotationDegree,
                                float lineThickness,
                                float opacity)
{
    juce::Path path { getLinePath (area, div, shouldBeClosed, rotationDegree, lineThickness) };

    Drawable::fill (g, path, colour, lineThickness, opacity);
}

juce::Path Rotary::Angles::getNumbersPath (const juce::StringArray& numbers,
                                           const juce::FontOptions& fontOptions,
                                           float length,
                                           float space,
                                           float baseline,
                                           bool shouldDrawInside,
                                           const juce::Rectangle<float>& area,
                                           int div,
                                           float arc,
                                           float rotation)
{
    juce::Path path;
    juce::GlyphArrangement glyph;
    const juce::Font font { fontOptions };

    auto angles { Angles { div, arc, rotation } };

    for (int index { 0 }; index < numbers.size(); ++index)
    {
        juce::StringArray nsv;
        nsv.addTokens (numbers[index].unquoted(), ",", "\"");

        enum
        {
            number,
            isShowing,
            isUsingCustomText,
            customText,
        };

        if (bool isShow { static_cast<bool> (nsv[isShowing].getIntValue()) })
        {
            juce::String text { static_cast<bool> (nsv[isUsingCustomText].getIntValue()) ? nsv[customText] : nsv[number] };
            float height { font.getHeight() };
            float width { juce::TextLayout::getStringWidth (font, text) };
            float halfHeight { 0.5f * height };
            float halfWidth { 0.5f * width };
            float offsetStart { shouldDrawInside ? 0.0f : font.getExtraKerningFactor() * height };
            float offsetEnd { shouldDrawInside ? font.getExtraKerningFactor() * height : 0.0f };
            float kerningOffset { Value::map (std::sin (angles[index]), -1.0f, 1.0f, offsetStart, offsetEnd) };
            float xOffset { Value::map (std::sin (angles[index]), -1.0f, 1.0f, -halfWidth, halfWidth) };
            float yOffset { Value::map (std::cos (angles[index]), -1.0f, 1.0f, halfHeight, -halfHeight) };
            float inRadius { area.getWidth() * 0.5f - space };
            float outRadius { length + space };
            auto line { jreng::centredLine (area.getCentre(), inRadius, outRadius, angles[index]) };
            auto end { shouldDrawInside ? line.getStart() : line.getEnd() };

            auto justification { juce::Justification::left };
            juce::Rectangle<float> r { 0.0f, 0.0f, width, height };

            if (shouldDrawInside)
            {
                xOffset *= -1.0f;
                yOffset *= -1.0f;
            }

            juce::Point<float> centre { end.getX() + xOffset, end.getY() + yOffset };

            r.setCentre (centre);

            glyph.addFittedText (font, text, r.getX() + kerningOffset, r.getY() - (baseline * height), r.getWidth(), r.getHeight(), justification, 1);
        }
    }

    glyph.createPath (path);

    return path;
}

Rotary::SubAngles::SubAngles (int div, int sub, float arc, float rotation)
{
    int index { 0 };

    const int subDiv { sub + 1 };

    for (float i = 0.0f; i < static_cast<float> (div); i += 1 / static_cast<float> (subDiv), ++index)
    {
        const float subAngle {
            rotation + Value::map (i, 0.0f, static_cast<float> (div - 1), -arc, arc)
        };

        if (not (index % subDiv == 0))
            push_back (subAngle);
    }

    pop_back();
}

/*____________________________________________________________________________*/

Rotary::Base::Base (const juce::Point<float>& centrePoint,
                    float diameter,
                    float arcDegree,
                    float rotationDegree,
                    const SVGtype shouldWriteSVGAs)
    : centre (centrePoint)
    , radius (0.5f * diameter)
    , arc (juce::degreesToRadians (arcDegree))
    , rotation (juce::degreesToRadians (rotationDegree))
    , svgType (shouldWriteSVGAs)
{
}

Rotary::Base::Base (const juce::Rectangle<float>& areaToDraw,
                    float normalisedValue,
                    float arcDegree,
                    float rotationDegree,
                    const SVGtype shouldWriteSVGAs)
    : svgType (shouldWriteSVGAs)
{
    centre = areaToDraw.getCentre();
    arc = juce::degreesToRadians (Value::map (normalisedValue, 0.0f, arcDegree));
    rotation = juce::degreesToRadians (Value::map (normalisedValue, -arcDegree, 0.0f));
}

juce::String Rotary::Base::getSVG (const juce::Colour& colour,
                                   const juce::Colour& numberColour,
                                   float stroke,
                                   float subStroke) const noexcept
{
    const auto& [marker, markerStroke] { getMarker() };
    using Format = jreng::SVG::Format;

    juce::String svg;
    const juce::String marks { "marks" };
    const juce::String numbers { "numbers" };
    const juce::String guide { "guide" };

    switch (svgType)
    {
        case SVGtype::fill:
            svg = Format::fill (path, colour, marks)
                  + Format::fill (subPath, colour, marks)
                  + Format::fill (numbersPath, numberColour, numbers)
                  + Format::stroke (marker, juce::Colours::magenta, markerStroke, guide);
            break;

        default:
            svg = Format::stroke (path, colour, stroke, marks)
                  + Format::stroke (subPath, colour, subStroke, marks)
                  + Format::fill (numbersPath, numberColour, numbers)
                  + Format::stroke (marker, juce::Colours::magenta, markerStroke, guide);
            break;
    }

    return Format::group (svg);
}

float Rotary::Base::getRadius (const juce::Rectangle<float>& area, float offset) const noexcept
{
    auto diameter { area.getAspectRatio() > 1.0 ? area.getHeight() : area.getWidth() };
    return (diameter / 2) - offset;
}

std::tuple<juce::Path, double> Rotary::Base::getMarker() const noexcept
{
    juce::Path marker;

    const float line { 10.0f };

    /** Centre cross */
    marker.startNewSubPath (centre);
    marker.lineTo (centre.getX() + line, centre.getY());
    marker.startNewSubPath (centre);
    marker.lineTo (centre.getX() - line, centre.getY());
    marker.startNewSubPath (centre);
    marker.lineTo (centre.getX(), centre.getY() + line);
    marker.startNewSubPath (centre);
    marker.lineTo (centre.getX(), centre.getY() - line);

    /** Top left */
    auto topLeft { centre + juce::Point<float> (-cornerDistance, -cornerDistance) };
    marker.startNewSubPath (topLeft);
    marker.lineTo (topLeft.getX() + line, topLeft.getY());
    marker.startNewSubPath (topLeft);
    marker.lineTo (topLeft.getX(), topLeft.getY() + line);

    /** Top right */
    auto topRight { centre + juce::Point<float> (cornerDistance, -cornerDistance) };
    marker.startNewSubPath (topRight);
    marker.lineTo (topRight.getX() - line, topRight.getY());
    marker.startNewSubPath (topRight);
    marker.lineTo (topRight.getX(), topRight.getY() + line);

    /** Bottom left */
    auto bottomLeft { centre + juce::Point<float> (-cornerDistance, cornerDistance) };
    marker.startNewSubPath (bottomLeft);
    marker.lineTo (bottomLeft.getX() + line, bottomLeft.getY());
    marker.startNewSubPath (bottomLeft);
    marker.lineTo (bottomLeft.getX(), bottomLeft.getY() - line);

    /** Bottom right */
    auto bottomRight { centre + juce::Point<float> (cornerDistance, cornerDistance) };
    marker.startNewSubPath (bottomRight);
    marker.lineTo (bottomRight.getX() - line, bottomRight.getY());
    marker.startNewSubPath (bottomRight);
    marker.lineTo (bottomRight.getX(), bottomRight.getY() - line);

    const double stroke { 0.3 };
    return { marker, stroke };
}

void Rotary::Base::drawNumbers (juce::Graphics& g,
                                const juce::StringArray& numbers,
                                int div,
                                float space,
                                float baseline,
                                bool shouldDrawInside)
{
    juce::Rectangle<float> area { centre.getX() - radius,
                                  centre.getY() - radius,
                                  2 * radius,
                                  2 * radius };

    const juce::Font currentFont { g.getCurrentFont() };
    const juce::FontOptions currentFontOptions { currentFont.getTypefaceName(),
                                                 currentFont.getHeight(),
                                                 currentFont.getStyleFlags() };
    numbersPath = Angles::getNumbersPath (numbers, currentFontOptions, length, space, baseline, shouldDrawInside, area, div, arc, rotation);

    cornerDistance = radius + space + (2 * currentFont.getHeight());

    g.fillPath (numbersPath);
}

/*____________________________________________________________________________*/

Rotary::Arc::Arc (const juce::Point<float>& centrePoint,
                  float diameter,
                  float lineWidth,
                  float arcDegree,
                  float rotationDegree)
    : Base (centrePoint, diameter - 2 * lineWidth, arcDegree, rotationDegree, SVGtype::fill)
{
    length = radius + lineWidth;
    //    cornerDistance = length;
}

Rotary::Arc::Arc (const juce::Rectangle<float>& areaToDraw,
                  float normalisedValue,
                  float lineWidth,
                  float arcDegree,
                  float rotationDegree)
    : Base (areaToDraw, normalisedValue, arcDegree, rotationDegree, SVGtype::fill)
{
    radius = getRadius (areaToDraw, lineWidth);

    length = radius + lineWidth;

    //    cornerDistance = length;
}

void Rotary::Arc::draw (juce::Graphics& g)
{
    path.clear();
    
    path.addPieSegment (centre.getX() - length,
                        centre.getY() - length,
                        2 * length,
                        2 * length,
                        -(arc - rotation),
                        (arc + rotation),
                        radius / length);

    g.fillPath (path);
}

/*____________________________________________________________________________*/

Rotary::Dash::Dash (const juce::Point<float>& centrePoint,
                    float diameter,
                    float lineWidth,
                    int division,
                    float gap,
                    float arcDegree,
                    float rotationDegree)
    : Arc (centrePoint, diameter, lineWidth, arcDegree, rotationDegree)
    , offset (0.5f * (arc / division) * gap)
{
    calculateAngles (division);
}

Rotary::Dash::Dash (const juce::Rectangle<float>& areaToDraw,
                    float normalisedValue,
                    float lineWidth,
                    int division,
                    float gap,
                    float arcDegree,
                    float rotationDegree)
    : Arc (areaToDraw, normalisedValue, lineWidth, arcDegree, rotationDegree)
    , offset (0.5f * (arc / division) * gap)
{
    calculateAngles (division);
}

void Rotary::Dash::draw (juce::Graphics& g)
{
    path.setUsingNonZeroWinding (true);
    path.clear();
    
    for (int index = 0; index < dashes.size() - 1; ++index)
    {
        float begin { dashes[index] + offset };
        float end { dashes[index + 1] - offset };
        path.addPieSegment (centre.getX() - length,
                            centre.getY() - length,
                            2 * length,
                            2 * length,
                            begin,
                            end,
                            radius / length);
    }

    g.fillPath (path);
}

void Rotary::Dash::calculateAngles (int division)
{
    const float segment { arc + offset };

    for (float i = 0.0f; i <= division; ++i)
    {
        dashes.push_back (rotation + Value::map (i, 0.0f, static_cast<float> (division), -segment, segment));
    }
}

/*____________________________________________________________________________*/
Rotary::Line::Line (const juce::Point<float>& centrePoint,
                    float diameter,
                    int division,
                    float lineLength,
                    float strokeWidth,
                    ShouldDraw shouldDrawArc,
                    bool isInside,
                    float arcDegree,
                    float rotationDegree)
    : Base (centrePoint, diameter - 2 * lineLength, arcDegree, rotationDegree, SVGtype::stroke)
    , div (division)
    , drawArc (shouldDrawArc)
    , arcPosition (isInside)
    , stroke (strokeWidth)
{
    length = radius + lineLength;
    //    cornerDistance = length;
}

Rotary::Line::Line (const juce::Rectangle<float>& areaToDraw,
                    float normalisedValue,
                    int division,
                    float lineLength,
                    float strokeWidth,
                    ShouldDraw shouldDrawArc,
                    bool isInside,
                    float arcDegree,
                    float rotationDegree)
    : Base (areaToDraw, normalisedValue, arcDegree, rotationDegree, SVGtype::stroke)
    , div (division)
    , drawArc (shouldDrawArc)
    , arcPosition (isInside)
    , stroke (strokeWidth)
{
    radius = getRadius (areaToDraw, lineLength);
    length = radius + lineLength;
    //    cornerDistance = length;
}

void Rotary::Line::draw (juce::Graphics& g)
{
    path.clear();
    
    switch (drawArc)
    {
        case ShouldDraw::withArc:
            path.addCentredArc (centre.getX(),
                                centre.getY(),
                                arcPosition ? radius : length,
                                arcPosition ? radius : length,
                                rotation,
                                -arc,
                                arc,
                                true);

        default:
            for (auto& angle : Angles { div, arc, rotation })
            {
                path.addLineSegment (jreng::centredLine (centre, radius, length, angle), 0.0f);
            }
            break;
    }

    g.strokePath (path, juce::PathStrokeType (stroke, juce::PathStrokeType::JointStyle::mitered, juce::PathStrokeType::EndCapStyle::square));
}

/*____________________________________________________________________________*/
Rotary::LineSub::LineSub (const juce::Point<float>& centrePoint,
                          float diameter,
                          int division,
                          int subDivision,
                          float lineLength,
                          float strokeWidth,
                          float subLineLength,
                          float subStrokeWidth,
                          ShouldDraw shouldDrawArc,
                          bool isInside,
                          float arcDegree,
                          float rotationDegree)
    : Line (centrePoint, diameter, division, lineLength, strokeWidth, shouldDrawArc, isInside, arcDegree, rotationDegree)
    , sub (subDivision)
    , subLength (subLineLength * lineLength)
    , subStroke (subStrokeWidth * stroke)
{
}

Rotary::LineSub::LineSub (const juce::Rectangle<float>& areaToDraw,
                          float normalisedValue,
                          int division,
                          int subDivision,
                          float lineLength,
                          float strokeWidth,
                          float subLineLength,
                          float subStrokeWidth,
                          ShouldDraw shouldDrawArc,
                          bool isInside,
                          float arcDegree,
                          float rotationDegree)
    : Line (areaToDraw, normalisedValue, division, lineLength, strokeWidth, shouldDrawArc, isInside, arcDegree, rotationDegree)
    , sub (subDivision)
    , subLength (subLineLength * lineLength)
    , subStroke (subStrokeWidth * stroke)
{
}

void Rotary::LineSub::draw (juce::Graphics& g)
{
    subPath.clear();
    
    for (auto& angle : SubAngles { div, sub, arc, rotation })
    {
        if (arcPosition)
            subPath.addLineSegment (jreng::centredLine (centre, radius, radius + subLength, angle), 0.0f);
        else
            subPath.addLineSegment (jreng::centredLine (centre, length - subLength, length, angle), 0.0f);
    }

    Line::draw (g);
    g.strokePath (subPath, juce::PathStrokeType (subStroke, juce::PathStrokeType::JointStyle::mitered, juce::PathStrokeType::EndCapStyle::square));
}

/*____________________________________________________________________________*/
Rotary::Dot::Dot (const juce::Point<float>& centrePoint,
                  float diameter,
                  int division,
                  float lineLength,
                  float dotSize,
                  float arcDegree,
                  float rotationDegree)
    : Base (centrePoint, diameter - dotSize, arcDegree, rotationDegree, SVGtype::fill)
    , div (division)
    , size (0.5f * dotSize)
{
    length = radius + lineLength - dotSize;
    //    cornerDistance = radius + size;
}

Rotary::Dot::Dot (const juce::Rectangle<float>& areaToDraw,
                  float normalisedValue,
                  int division,
                  float lineLength,
                  float dotSize,
                  float arcDegree,
                  float rotationDegree)
    : Base (areaToDraw, normalisedValue, arcDegree, rotationDegree, SVGtype::fill)
    , size (0.5f * dotSize)
{
    div = division;
    radius = getRadius (areaToDraw, dotSize * 0.5f);
    length = radius + lineLength;
    //    cornerDistance = radius + size;
}

void Rotary::Dot::draw (juce::Graphics& g)
{
    path.clear();
    
    for (auto& angle : Angles { div, arc, rotation })
    {
        const auto dotCentre {
            centre.getPointOnCircumference (radius, angle).translated (-size, -size)
        };

        path.addEllipse (dotCentre.getX(), dotCentre.getY(), 2 * size, 2 * size);
    }

    g.fillPath (path);
}

/*____________________________________________________________________________*/
Rotary::DotSub::DotSub (const juce::Point<float>& centrePoint,
                        float diameter,
                        int division,
                        int subDivision,
                        float lineLength,
                        float dotSize,
                        float subDotSize,
                        float arcDegree,
                        float rotationDegree)
    : Dot (centrePoint, diameter, division, lineLength, dotSize, arcDegree, rotationDegree)
    , sub (subDivision)
    , subSize (0.5f * subDotSize * dotSize)
{
    div = division;
}

Rotary::DotSub::DotSub (const juce::Rectangle<float>& areaToDraw,
                        float normalisedValue,
                        int division,
                        int subDivision,
                        float lineLength,
                        float dotSize,
                        float subDotSize,
                        float arcDegree,
                        float rotationDegree)
    : Dot (areaToDraw, normalisedValue, division, lineLength, dotSize, arcDegree, rotationDegree)
    , sub (subDivision)
    , subSize (0.5f * subDotSize * dotSize)
{
    div = division;
}

void Rotary::DotSub::draw (juce::Graphics& g)
{
    subPath.clear();
    
    for (auto& angle : SubAngles { div, sub, arc, rotation })
    {
        const auto dotCentre {
            centre.getPointOnCircumference (radius, angle)
                .translated (-subSize, -subSize)

        };

        subPath.addEllipse (dotCentre.getX(), dotCentre.getY(), 2 * subSize, 2 * subSize);
    }

    Dot::draw (g);
    g.fillPath (subPath);
}

/*____________________________________________________________________________*/
Rotary::Polygon::Polygon (const juce::Point<float>& centrePoint,
                          float diameter,
                          int division,
                          float lineLength,
                          int polySide,
                          float dotSize,
                          float polyAngleDegree,
                          float arcDegree,
                          float rotationDegree)
    : Base (centrePoint, diameter - dotSize, arcDegree, rotationDegree, SVGtype::fill)
    , side (polySide)
    , size (dotSize)
    , angle (juce::degreesToRadians (polyAngleDegree))
{
    div = division;
    length = radius + lineLength;
    //    cornerDistance = radius + (0.5f * size);
}

Rotary::Polygon::Polygon (const juce::Rectangle<float>& areaToDraw,
                          float normalisedValue,
                          int division,
                          float lineLength,
                          int polySide,
                          float dotSize,
                          float polyAngleDegree,
                          float arcDegree,
                          float rotationDegree)
    : Base (areaToDraw, normalisedValue, arcDegree, rotationDegree, SVGtype::fill)
    , side (polySide)
    , size (dotSize)
    , angle (juce::degreesToRadians (polyAngleDegree))
{
    div = division;
    radius = getRadius (areaToDraw, dotSize * 0.5f);
    //    cornerDistance = radius + (0.5f * size);
}

void Rotary::Polygon::draw (juce::Graphics& g)
{
    path.clear();
    
    for (auto& a : Angles { div, arc, rotation })
    {
        path.addPolygon (centre.getPointOnCircumference (radius, a), side, size, angle + a + juce::MathConstants<float>::pi);
    }

    g.fillPath (path);
}

/*____________________________________________________________________________*/
Rotary::PolygonSub::PolygonSub (const juce::Point<float>& centrePoint,
                                float diameter,
                                int division,
                                int subDivision,
                                float lineLength,
                                int polySide,
                                float dotSize,
                                float subDotSize,
                                bool isInside,
                                float polyAngleDegree,
                                float arcDegree,
                                float rotationDegree)
    : Polygon (centrePoint, diameter, division, lineLength, polySide, dotSize, polyAngleDegree, arcDegree, rotationDegree)
    , sub (subDivision)
    , subSize (subDotSize * dotSize)
    , subPosition (isInside)
{
    div = division;
}

Rotary::PolygonSub::PolygonSub (const juce::Rectangle<float>& areaToDraw,
                                float normalisedValue,
                                int division,
                                int subDivision,
                                float lineLength,
                                int polySide,
                                float dotSize,
                                float subDotSize,
                                bool isInside,
                                float polyAngleDegree,
                                float arcDegree,
                                float rotationDegree)
    : Polygon (areaToDraw, normalisedValue, division, polySide, dotSize, polyAngleDegree, arcDegree, rotationDegree)
    , sub (subDivision)
    , subSize (subDotSize * dotSize)
    , subPosition (isInside)
{
    div = division;
}

void Rotary::PolygonSub::draw (juce::Graphics& g)
{
    subPath.clear();
    
    for (auto& a : SubAngles { div, sub, arc, rotation })
    {
        auto polyCentre { centre.getPointOnCircumference (subPosition ? (radius - size + subSize) : (radius + size - subSize), a) };
        subPath.addPolygon (polyCentre, side, subSize, angle + a + juce::MathConstants<float>::pi);
    }

    Polygon::draw (g);
    g.fillPath (subPath);
}

/**_____________________________END OF NAMESPACE______________________________*/
} /** namespace jrengGraphics */
