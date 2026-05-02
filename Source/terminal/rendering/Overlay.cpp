/**
 * @file Overlay.cpp
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
    triggerRepaint();
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
        triggerRepaint();

        if (not frameDelays.empty())
            startTimer (frameDelays.at (0));
    }
}

void Overlay::setBorderColour (juce::Colour colour) noexcept
{
    borderColour = colour;
    triggerRepaint();
}

void Overlay::setPadding (int pixels) noexcept
{
    padding = pixels;
    triggerRepaint();
}

void Overlay::setShowBorder (bool show) noexcept
{
    showBorder = show;
    triggerRepaint();
}

//==============================================================================
// jam::gl::Component — GPU path
//==============================================================================

void Overlay::paintGL (jam::gl::Graphics& g) noexcept
{
    const auto bounds     { getLocalBounds().toFloat() };
    const auto imageArea  { bounds.reduced (static_cast<float> (padding)) };

    if (image.isValid())
    {
        const auto sourceRect { image.getBounds().toFloat() };
        const auto targetArea { juce::RectanglePlacement { juce::RectanglePlacement::centred }
                                    .appliedTo (sourceRect, imageArea) };

        g.drawImage (image, targetArea);
    }

    if (showBorder)
    {
        juce::Path border;
        border.addRoundedRectangle (bounds, borderCornerRadius);

        g.setColour (borderColour);
        g.strokePath (border, juce::PathStrokeType { borderStrokeWidth });
    }
}

//==============================================================================
// jam::gl::Component — CPU path
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
        g.drawRoundedRectangle (bounds.toFloat(), borderCornerRadius, borderStrokeWidth);
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
        triggerRepaint();

        const int nextDelay { frameDelays.empty()
                                  ? fallbackFrameDelayMs
                                  : frameDelays.at (static_cast<std::size_t> (currentFrameIndex)
                                                    % frameDelays.size()) };
        startTimer (nextDelay);
    }
}

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Terminal
