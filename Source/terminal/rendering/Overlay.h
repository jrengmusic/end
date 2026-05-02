/**
 * @file Overlay.h
 * @brief Lean image overlay component: paints a single image (or animated frames)
 *        with an optional border.
 *
 * `Terminal::Overlay` is a `jam::gl::Component` child of `Terminal::Display`.
 * It owns no GL resources — jam handles all GPU state.  Display tells Overlay
 * what to paint via the set* API; Overlay paints on demand.
 *
 * @see Terminal::Display
 */
#pragma once
#include <JuceHeader.h>
#include <vector>

namespace Terminal
{ /*____________________________________________________________________________*/

class Overlay : public jam::gl::Component,
                private juce::Timer
{
public:
    Overlay();
    ~Overlay() override;

    // =========================================================================
    // Display tells Overlay what to paint
    // =========================================================================

    /** @brief Sets a single static image.  Stops any running animation. */
    void setImage (juce::Image newImage) noexcept;

    /** @brief Sets an animated frame sequence.  Starts the timer with the first delay. */
    void setFrames (std::vector<juce::Image> newFrames, std::vector<int> delaysMs) noexcept;

    void setBorderColour (juce::Colour colour) noexcept;
    void setPadding (int pixels) noexcept;
    void setShowBorder (bool show) noexcept;

private:
    // =========================================================================
    // jam::gl::Component
    // =========================================================================

    void paintGL (jam::gl::Graphics& g) noexcept override;
    void paint (juce::Graphics& g) override;

    // =========================================================================
    // juce::Timer
    // =========================================================================

    void timerCallback() override;

    // =========================================================================
    // Constants
    // =========================================================================

    static constexpr float borderCornerRadius    { 8.0f };
    static constexpr float borderStrokeWidth     { 2.0f };
    static constexpr int   fallbackFrameDelayMs  { 100 };

    // =========================================================================
    // Data
    // =========================================================================

    juce::Image              image;
    std::vector<juce::Image> frames;
    std::vector<int>         frameDelays;
    int                      currentFrameIndex { 0 };
    juce::Colour             borderColour      { juce::Colours::white.withAlpha (0.3f) };
    int                      padding           { 0 };
    bool                     showBorder        { true };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Overlay)
};

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Terminal
