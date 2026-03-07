namespace jreng::Graphics
{
/*____________________________________________________________________________*/

class Vignette : public juce::Component
{
public:
    Vignette() = default;
    ~Vignette() = default;

    void paint (juce::Graphics& g) override
    {
        const float cornerRadius { 16.0f };// Adjust to match your component's corner radius
        auto rect = getLocalBounds().toFloat();

        // Create rounded rectangle path and clip to it
        juce::Path roundedPath;
        roundedPath.addRoundedRectangle (rect, cornerRadius);
        g.reduceClipRegion (roundedPath);
        
        auto innerRect = rect.reduced (edgeDistance);
        auto transparent { juce::Colour() };
        auto colour { findColour (juce::ResizableWindow::backgroundColourId).withSaturation (0.5f) };

        EdgeGradient edges[] = {
            // Top edge
            { { rect.getCentreX(), rect.getY() },
              { innerRect.getCentreX(), innerRect.getY() },
              { rect.getX(), rect.getY(), rect.getWidth(), edgeDistance } },

            // Bottom edge
            { { rect.getCentreX(), rect.getBottom() },
              { innerRect.getCentreX(), innerRect.getBottom() },
              { rect.getX(), innerRect.getBottom(), rect.getWidth(), edgeDistance } },

            // Left edge
            { { rect.getX(), rect.getCentreY() },
              { innerRect.getX(), innerRect.getCentreY() },
              { rect.getX(), rect.getY(), edgeDistance, rect.getHeight() } },

            // Right edge
            { { rect.getRight(), rect.getCentreY() },
              { innerRect.getRight(), innerRect.getCentreY() },
              { innerRect.getRight(), rect.getY(), edgeDistance, rect.getHeight() } }
        };

        for (const auto& edge : edges)
        {
            juce::ColourGradient gradient (colour, edge.outerPoint, transparent, edge.innerPoint, false);
            g.setGradientFill (gradient);
            g.fillRect (edge.fillArea);
        }
    }

private:
    // Define edge gradients: outer point, inner point, fill rect
    struct EdgeGradient
    {
        juce::Point<float> outerPoint;
        juce::Point<float> innerPoint;
        juce::Rectangle<float> fillArea;
    };

    float edgeDistance { 40.0f };
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Vignette)
};

/**_____________________________END OF NAMESPACE______________________________*/
} /** namespace jreng::Graphics */
