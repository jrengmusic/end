namespace jreng
{
/*____________________________________________________________________________*/

template<typename ValueType>
juce::Line<ValueType> centredLine (const juce::Point<ValueType>& centre,
                                   ValueType inRadius,
                                   ValueType outRadius,
                                   ValueType angle)
{
    return juce::Line<ValueType> (centre.getPointOnCircumference (inRadius, angle),
                                  centre.getPointOnCircumference (outRadius, angle));
}

namespace Graphics
{
/*____________________________________________________________________________*/

struct Rotary
{
    struct Angles : public std::vector<float>
    {
        Angles (int div, float arc, float rotation);

        static Points<float> getPoints (const juce::Rectangle<float>& area, int div, float rotationDegree = 0.0f);

        static Lines<float> getLines (const juce::Rectangle<float>& area, int div, bool shouldBeClosed = true, float rotationDegree = 0.0f);

        static juce::Path getLinePath (const juce::Rectangle<float>& area, int div, bool shouldBeClosed = true, float rotationDegree = 0.0f, float lineThickness = 2.0f);

        static void drawLines (juce::Graphics& g, juce::Colour colour, const juce::Rectangle<float>& area, int div, bool shouldBeClosed = true, float rotationDegree = 0.0f, float lineThickness = 2.0f, float opacity = 1.0f);

        static juce::Path getNumbersPath (const juce::StringArray& numbers,
                                          const juce::Font& font,
                                          float length,
                                          float space,
                                          float baseline,
                                          bool shouldDrawInside,
                                          const juce::Rectangle<float>& area,
                                          int div,
                                          float arc,
                                          float rotation = 0.0f);

        //==============================================================================
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Angles)
    };

    struct SubAngles : public std::vector<float>
    {
        SubAngles (int div, int sub, float arc, float rotation);

        //==============================================================================
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SubAngles)
    };

    /*____________________________________________________________________________*/

    enum class SVGtype
    {
        fill,
        stroke,
    };

    class Base
    {
    public:
        Base (const juce::Point<float>& centrePoint,
              float diameter,
              float arcDegree = 150.0f,
              float rotationDegree = 0.0f,
              const SVGtype shouldWriteSVGAs = SVGtype::stroke);

        Base (const juce::Rectangle<float>& areaToDraw,
              float normalisedValue = 1.0f,
              float arcDegree = 150.0f,
              float rotationDegree = 0.0f,
              const SVGtype shouldWriteSVGAs = SVGtype::stroke);

        virtual ~Base() {}

        virtual void draw (juce::Graphics& g) = 0;
        void drawNumbers (juce::Graphics& g,
                          const juce::StringArray& numbers,
                          int div,
                          float baseline,
                          float position,
                          bool shouldDrawInside);

        juce::String getSVG (const juce::Colour& colour,
                             const juce::Colour& numberColour,
                             float stroke,
                             float subStroke) const noexcept;

    protected:
        float getRadius (const juce::Rectangle<float>& area, float offset = 0) const noexcept;

        juce::Point<float> centre;
        //        int div;
        float radius;
        float arc;
        float rotation;
        float length;
        juce::Path path;
        juce::Path subPath;
        juce::Path numbersPath;
        float cornerDistance;

    private:
        const SVGtype svgType;

        std::tuple<juce::Path, double> getMarker() const noexcept;
    };

    enum class Mode
    {
        Arc = 1,
        Line,
        ArcLine,
        Dot,
        InvLine,
        InvArcLine,
        Poly,
        ArcDiv,
        LineSub,
        ArcLineSub,
        DotSub,
        InvLineSub,
        InvArcLineSub,
        PolySub,
        InvPolySub = 17,
    };

    /*____________________________________________________________________________*/

    class Arc : public Base
    {
    public:
        Arc (const juce::Point<float>& centrePoint,
             float diameter,
             float lineWidth = 2.0f,
             float arcDegree = 150.f,
             float rotationDegree = 0.0f);

        Arc (const juce::Rectangle<float>& areaToDraw,
             float normalisedValue = 1.0f,
             float lineWidth = 2.0f,
             float arcDegree = 150.f,
             float rotationDegree = 0.0f);

        void draw (juce::Graphics& g) override;

        //==============================================================================
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Arc)
    };

    /*____________________________________________________________________________*/

    class Dash : public Arc
    {
    public:
        Dash (const juce::Point<float>& centrePoint,
              float diameter,
              float lineWidth = 2.0f,
              int division = 9,
              float gap = 0.25f,
              float arcDegree = 150.0f,
              float rotationDegree = 0.0f);

        Dash (const juce::Rectangle<float>& areaToDraw,
              float normalisedValue = 1.0f,
              float lineWidth = 2.0f,
              int division = 9,
              float gap = 0.25f,
              float arcDegree = 150.0f,
              float rotationDegree = 0.0f);

        void draw (juce::Graphics& g) override;

    private:
        int div;
        void calculateAngles (int division);

        float offset;
        std::vector<float> dashes;
        //==============================================================================
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Dash)
    };

    /*____________________________________________________________________________*/
    enum class ShouldDraw
    {
        withoutArc,
        withArc
    };

    class Line : public Base
    {
    public:
        Line (const juce::Point<float>& centrePoint,
              float diameter,
              int division = 9,
              float lineLength = 10.0f,
              float strokeWidth = 2.0f,
              ShouldDraw shouldDrawArc = ShouldDraw::withoutArc,
              bool isInside = true,
              float arcDegree = 150.0f,
              float rotationDegree = 0.0f);

        Line (const juce::Rectangle<float>& areaToDraw,
              float normalisedValue = 1.0f,
              int division = 9,
              float lineLength = 10.0f,
              float strokeWidth = 2.0f,
              ShouldDraw shouldDrawArc = ShouldDraw::withoutArc,
              bool isInside = true,
              float arcDegree = 150.0f,
              float rotationDegree = 0.0f);

        void draw (juce::Graphics& g) override;

    protected:
        int div;
        ShouldDraw drawArc;
        bool arcPosition;
        float stroke;
        //==============================================================================
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Line)
    };

    /*____________________________________________________________________________*/
    class LineSub : public Line
    {
    public:
        LineSub (const juce::Point<float>& centrePoint,
                 float diameter,
                 int division = 9,
                 int subDivision = 1,
                 float lineLength = 10.0f,
                 float strokeWidth = 2.0f,
                 float subLineLength = 0.75f,
                 float subStrokeWidth = 0.75f,
                 ShouldDraw shouldDrawArc = ShouldDraw::withoutArc,
                 bool isInside = true,
                 float arcDegree = 150.0f,
                 float rotationDegree = 0.0f);

        LineSub (const juce::Rectangle<float>& areaToDraw,
                 float normalisedValue = 1.0f,
                 int division = 9,
                 int subDivision = 1,
                 float lineLength = 10.0f,
                 float strokeWidth = 2.0f,
                 float subLineLength = 0.75f,
                 float subStrokeWidth = 0.75f,
                 ShouldDraw shouldDrawArc = ShouldDraw::withoutArc,
                 bool isInside = true,
                 float arcDegree = 150.0f,
                 float rotationDegree = 0.0f);

        void draw (juce::Graphics& g) override;

    private:
        int sub;
        float subLength;
        float subStroke;
        //==============================================================================
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LineSub)
    };

    /*____________________________________________________________________________*/
    class Dot : public Base
    {
    public:
        Dot (const juce::Point<float>& centrePoint,
             float diameter = 1.0f,
             int division = 9,
             float lineLength = 10.0f,
             float dotSize = 4.0f,
             float arcDegree = 150.0f,
             float rotationDegree = 0.0f);

        Dot (const juce::Rectangle<float>& areaToDraw,
             float normalisedValue = 1.0f,
             int division = 9,
             float lineLength = 10.0f,
             float dotSize = 4.0f,
             float arcDegree = 150.0f,
             float rotationDegree = 0.0f);

        void draw (juce::Graphics& g) override;

    protected:
        int div;
        float size;
        //==============================================================================
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Dot)
    };

    /*____________________________________________________________________________*/
    class DotSub : public Dot
    {
    public:
        DotSub (const juce::Point<float>& centrePoint,
                float diameter,
                int division = 9,
                int subDivision = 1,
                float lineLength = 10.0f,
                float dotSize = 4.0f,
                float subDotSize = 0.75f,
                float arcDegree = 150.0f,
                float rotationDegree = 0.0f);

        DotSub (const juce::Rectangle<float>& areaToDraw,
                float normalisedValue = 1.0f,
                int division = 9,
                int subDivision = 1,
                float lineLength = 10.0f,
                float dotSize = 4.0f,
                float subDotSize = 0.75f,
                float arcDegree = 150.0f,
                float rotationDegree = 0.0f);

        void draw (juce::Graphics& g) override;

    private:
        int sub;
        float subSize;
        //==============================================================================
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DotSub)
    };

    /*____________________________________________________________________________*/
    class Polygon : public Base
    {
    public:
        Polygon (const juce::Point<float>& centrePoint,
                 float diameter,
                 int division = 9,
                 float lineLength = 10.0f,
                 int polySide = 3,
                 float dotSize = 4.0f,
                 float polyAngleDegree = 0.0f,
                 float arcDegree = 150.0f,
                 float rotationDegree = 0.0f);

        Polygon (const juce::Rectangle<float>& areaToDraw,
                 float normalisedValue = 1.0f,
                 int division = 9,
                 float lineLength = 10.0f,
                 int polySide = 3,
                 float dotSize = 4.0f,
                 float polyAngleDegree = 0.0f,
                 float arcDegree = 150.0f,
                 float rotationDegree = 0.0f);

        void draw (juce::Graphics& g) override;

    protected:
        int div;
        int side;
        float size;
        float angle;
        //==============================================================================
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Polygon)
    };

    /*____________________________________________________________________________*/
    class PolygonSub : public Polygon
    {
    public:
        PolygonSub (const juce::Point<float>& centrePoint,
                    float diameter,
                    int division = 9,
                    int subDivision = 1,
                    float lineLength = 10.0f,
                    int polySide = 3,
                    float dotSize = 4.0f,
                    float subDotSize = 0.75f,
                    bool isInside = true,
                    float polyAngleDegree = 0.0f,
                    float arcDegree = 150.0f,
                    float rotationDegree = 0.0f);

        PolygonSub (const juce::Rectangle<float>& areaToDraw,
                    float normalisedValue = 1.0f,
                    int division = 9,
                    int subDivision = 1,
                    float lineLength = 10.0f,
                    int polySide = 3,
                    float dotSize = 4.0f,
                    float subDotSize = 0.75f,
                    bool isInside = true,
                    float polyAngleDegree = 0.0f,
                    float arcDegree = 150.0f,
                    float rotationDegree = 0.0f);

        void draw (juce::Graphics& g) override;

    private:
        int sub;
        float subSize;
        bool subPosition;
        //==============================================================================
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PolygonSub)
    };
};
/**_____________________________END OF NAMESPACE______________________________*/
} /** namespace Graphics */
/**_____________________________END OF NAMESPACE______________________________*/
} /** namespace jreng */
