namespace jreng::Graphics
{
/*____________________________________________________________________________*/

template <typename ShadowType>
struct Shadow
{
    struct Shape
    {
        ShadowType& parent;
        float angle;
        float radius;
        float distance;

        //==============================================================================
        juce::Path getPath() const noexcept
        {
            auto& origin { parent.origin };

            float ellipseWidth { radius * origin.getWidth() };
            float ellipseHeight { radius * origin.getHeight() };

            juce::Rectangle<float> ellipse { ellipseWidth, ellipseHeight };
            ellipse.setCentre (getCircumference().getEnd());

            auto leftCircum { centredLine (ellipse.getCentre(), 0.0f, ellipseWidth * 0.5f, juce::degreesToRadians (180.0f - angle)) };
            auto rightCircum { centredLine (ellipse.getCentre(), 0.0f, ellipseWidth * 0.5f, juce::degreesToRadians (360.0f - angle)) };

            juce::Path p;
            p.startNewSubPath ({ origin.getX(), origin.getCentreY() });
            p.lineTo ({ origin.getRight(), origin.getCentreY() });
            p.lineTo (rightCircum.getEnd());
            p.lineTo (leftCircum.getEnd());
            p.closeSubPath();
            p.addEllipse (ellipse);

            return p;
        }

        juce::Line<float> getCircumference() const noexcept
        {
            return centredLine (parent.origin.getCentre(), 0.0f, distance * parent.getHeight(), juce::degreesToRadians (90.0f - angle));
        }

        //==============================================================================
    } shape;

    struct Gradient
    {
        float opacity;
        float startAlpha;
        float endAlpha;
        float start;
        float end;
        juce::Colour colour { juce::Colours::black };
        bool isRadial { true };

        //==============================================================================
        juce::ColourGradient getGradient (const Shape& shape) const noexcept
        {
            auto line { shape.getCircumference() };
            auto startPoint { line.getPointAlongLineProportionally (start) };
            auto endPoint { line.getPointAlongLineProportionally (end) };
            auto startColour { colour.withAlpha (startAlpha * opacity) };
            auto endColour { colour.withAlpha (endAlpha * opacity) };

            return juce::ColourGradient (startColour, startPoint, endColour, endPoint, isRadial);
        }

        //==============================================================================
    } gradient;

    jreng::CachedBlur blur;
    juce::Image image;
};

/**_____________________________END OF NAMESPACE______________________________*/
} /** namespace jreng::Graphics */
