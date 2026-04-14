#pragma once

namespace jreng
{

class GLVignette : public GLComponent
{
public:
    GLVignette() = default;
    ~GLVignette() override = default;

    void setEdgeDistance (float newDistance) noexcept
    {
        edgeDistance = newDistance;
    }

    float getEdgeDistance() const noexcept
    {
        return edgeDistance;
    }

    void paintGL (GLGraphics& g) noexcept override
    {
        const auto bounds { getLocalBounds().toFloat() };
        const auto c { findColour (juce::ResizableWindow::backgroundColourId) };

        const auto innerRect { bounds.reduced (edgeDistance) };
        const juce::Colour transparent {};

        g.fillLinearGradient ({ bounds.getX(), bounds.getY(), bounds.getWidth(), edgeDistance },
                              c, transparent,
                              { bounds.getCentreX(), bounds.getY() },
                              { innerRect.getCentreX(), innerRect.getY() });

        g.fillLinearGradient ({ bounds.getX(), innerRect.getBottom(), bounds.getWidth(), edgeDistance },
                              c, transparent,
                              { bounds.getCentreX(), bounds.getBottom() },
                              { innerRect.getCentreX(), innerRect.getBottom() });

        g.fillLinearGradient ({ bounds.getX(), bounds.getY(), edgeDistance, bounds.getHeight() },
                              c, transparent,
                              { bounds.getX(), bounds.getCentreY() },
                              { innerRect.getX(), innerRect.getCentreY() });

        g.fillLinearGradient ({ innerRect.getRight(), bounds.getY(), edgeDistance, bounds.getHeight() },
                              c, transparent,
                              { bounds.getRight(), bounds.getCentreY() },
                              { innerRect.getRight(), innerRect.getCentreY() });

    }

private:
    static constexpr float defaultEdgeDistance { 40.0f };
    float edgeDistance { defaultEdgeDistance };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GLVignette)
};

} // namespace jreng
