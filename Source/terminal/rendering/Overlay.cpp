/**
 * @file Overlay.cpp
 * @brief Terminal::Overlay — image overlay component (jam::animation::Base).
 */
#include "Overlay.h"

namespace Terminal
{ /*____________________________________________________________________________*/

Overlay::Overlay()  = default;
Overlay::~Overlay() = default;

//==============================================================================
// Display tells Overlay what to paint
//==============================================================================

void Overlay::setImage (juce::Image newImage) noexcept
{
    stopTimer();
    frames.clear();
    frameDelays.clear();
    currentFrameIndex = 0;
    image = std::move (newImage);
    repaint();
}

void Overlay::setFrames (std::vector<juce::Image> newFrames, std::vector<int> delaysMs) noexcept
{
    stopTimer();
    frames      = std::move (newFrames);
    frameDelays = std::move (delaysMs);

    if (not frames.empty())
    {
        currentFrameIndex = 0;
        image = frames.at (0);
        repaint();

        if (not frameDelays.empty())
            startTimer (frameDelays.at (0));
    }
}

void Overlay::setBorderColour (juce::Colour colour) noexcept
{
    borderColour = colour;
    repaint();
}

void Overlay::setPadding (int pixels) noexcept
{
    padding = pixels;
    repaint();
}

void Overlay::setShowBorder (bool show) noexcept
{
    showBorder = show;
    repaint();
}

//==============================================================================
// juce::Component
//==============================================================================

void Overlay::paint (juce::Graphics& g)
{
    const auto bounds    { getLocalBounds() };
    const auto imageArea { bounds.reduced (padding) };

    if (image.isValid())
    {
        g.drawImageWithin (image,
                           imageArea.getX(), imageArea.getY(),
                           imageArea.getWidth(), imageArea.getHeight(),
                           juce::RectanglePlacement::centred);
    }

    if (showBorder)
    {
        g.setColour (borderColour);
        g.drawRoundedRectangle (bounds.toFloat().reduced (borderStrokeWidth * 0.5f), borderCornerRadius, borderStrokeWidth);
    }
}

//==============================================================================
// juce::Timer — animation
//==============================================================================

void Overlay::timerCallback()
{
    if (not frames.empty())
    {
        currentFrameIndex = (currentFrameIndex + 1) % static_cast<int> (frames.size());
        image = frames.at (currentFrameIndex);
        repaint();

        const int nextDelay { frameDelays.empty()
                                  ? fallbackFrameDelayMs
                                  : frameDelays.at (static_cast<std::size_t> (currentFrameIndex)
                                                    % frameDelays.size()) };
        startTimer (nextDelay);
    }
}

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Terminal
