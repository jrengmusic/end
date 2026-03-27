/**
 * @file SpinnerOverlay.h
 * @brief Braille spinner overlay shown during markdown file parsing.
 *
 * Displays an animated braille spinner with "Loading..." text, centered
 * over the whelmed pane. Animated fade-in / fade-out via toggleFade.
 * Timer drives frame advancement at spinnerIntervalMs.
 *
 * @note All methods MESSAGE THREAD.
 * @see Whelmed::Component
 * @see MessageOverlay (pattern reference)
 */

#pragma once
#include <JuceHeader.h>
#include "config/Config.h"

namespace Whelmed
{ /*____________________________________________________________________________*/

class SpinnerOverlay
    : public juce::Component
    , private juce::Timer
{
public:
    SpinnerOverlay()
    {
        setOpaque (false);
        setInterceptsMouseClicks (false, false);
    }

    void show()
    {
        frameIndex = 0;
        jreng::Animator::toggleFade (this, true, fadeInMs);
        startTimer (spinnerIntervalMs);
    }

    void hide()
    {
        stopTimer();
        jreng::Animator::toggleFade (this, false);
    }

    void paint (juce::Graphics& g) override
    {
        const auto* cfg { Whelmed::Config::getContext() };
        const auto bgColour { cfg->getColour (Whelmed::Config::Key::background) };
        const auto fgColour { cfg->getColour (Whelmed::Config::Key::bodyColour) };

        g.fillAll (bgColour.withAlpha (backgroundAlpha));

        const juce::String spinnerText { juce::String::charToString (frames[frameIndex]) + "  Loading..." };

        g.setFont (juce::FontOptions()
                       .withName (cfg->getString (Whelmed::Config::Key::fontFamily))
                       .withPointHeight (cfg->getFloat (Whelmed::Config::Key::fontSize)));
        g.setColour (fgColour);
        g.drawText (spinnerText, getLocalBounds(), juce::Justification::centred, false);
    }

private:
    void timerCallback() override
    {
        frameIndex = (frameIndex + 1) % frameCount;
        repaint();
    }

    int frameIndex { 0 };

    // Braille spinner rotation: 10 frames covering a full cycle
    static constexpr int frameCount { 10 };
    static constexpr juce::juce_wchar frames[frameCount]
    {
        0x280B,  // ⠋
        0x2819,  // ⠙
        0x2839,  // ⠹
        0x2838,  // ⠸
        0x283C,  // ⠼
        0x2834,  // ⠴
        0x2826,  // ⠦
        0x2827,  // ⠧
        0x2807,  // ⠇
        0x280F   // ⠏
    };

    static constexpr float backgroundAlpha { 0.8f };
    static constexpr int fadeInMs { 60 };
    static constexpr int spinnerIntervalMs { 80 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpinnerOverlay)
};

/**_____________________________END OF NAMESPACE______________________________*/
}// namespace Whelmed
