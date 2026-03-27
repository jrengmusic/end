/**
 * @file SpinnerOverlay.h
 * @brief Braille spinner progress bar shown during markdown file parsing.
 *
 * Displays a thin bar at the bottom of the parent component. Fills left-to-right
 * as blocks complete. Text reads: "⠋ Building Blocks (5/194)".
 * Frame advancement is driven by ValueTree events via update().
 *
 * @note All methods MESSAGE THREAD.
 * @see Whelmed::Component
 */

#pragma once
#include <JuceHeader.h>
#include "config/Config.h"

namespace Whelmed
{ /*____________________________________________________________________________*/

class SpinnerOverlay
    : public juce::Component
{
public:
    SpinnerOverlay()
    {
        setOpaque (false);
        setInterceptsMouseClicks (false, false);
    }

    void show (int total)
    {
        builtCount = 0;
        totalCount = total;
        frameIndex = 0;
        setVisible (true);
    }

    void update (int built, int total)
    {
        builtCount = built;
        totalCount = total;
        frameIndex = (frameIndex + 1) % frameCount;
        repaint();
    }

    void hide()
    {
        setVisible (false);
    }

    void paint (juce::Graphics& g) override
    {
        const auto* cfg { Whelmed::Config::getContext() };
        const auto bgColour      { cfg->getColour (Whelmed::Config::Key::progressBackground) };
        const auto fgColour      { cfg->getColour (Whelmed::Config::Key::progressForeground) };
        const auto textColour    { cfg->getColour (Whelmed::Config::Key::progressTextColour) };
        const auto spinnerColour { cfg->getColour (Whelmed::Config::Key::progressSpinnerColour) };

        // Background bar
        g.setColour (bgColour);
        g.fillRect (getLocalBounds());

        // Progress fill
        if (totalCount > 0)
        {
            const float ratio { static_cast<float> (builtCount) / static_cast<float> (totalCount) };
            const int fillWidth { static_cast<int> (ratio * static_cast<float> (getWidth())) };
            g.setColour (fgColour);
            g.fillRect (0, 0, fillWidth, getHeight());
        }

        // Spinner + text
        const juce::String progressText { totalCount > 0
            ? " (" + juce::String (builtCount) + "/" + juce::String (totalCount) + ")"
            : juce::String() };

        g.setFont (juce::FontOptions()
                       .withName (cfg->getString (Whelmed::Config::Key::fontFamily))
                       .withPointHeight (cfg->getFloat (Whelmed::Config::Key::fontSize)));

        const auto textArea { getLocalBounds().reduced (8, 0) };
        const juce::String spinner { juce::String::charToString (frames.at ((size_t) frameIndex)) + " " };

        g.setColour (spinnerColour);
        g.drawText (spinner, textArea, juce::Justification::centredLeft, false);

        juce::GlyphArrangement ga;
        ga.addLineOfText (g.getCurrentFont(), spinner, 0.0f, 0.0f);
        const int spinnerWidth { static_cast<int> (std::ceil (ga.getBoundingBox (0, -1, false).getWidth())) };
        const auto labelArea { textArea.withTrimmedLeft (spinnerWidth) };

        g.setColour (textColour);
        g.drawText ("Building Blocks" + progressText, labelArea, juce::Justification::centredLeft, false);
    }

private:
    int frameIndex  { 0 };
    int builtCount  { 0 };
    int totalCount  { 0 };

    // Braille spinner rotation: 10 frames covering a full cycle
    static constexpr int frameCount { 10 };
    static constexpr std::array<juce::juce_wchar, frameCount> frames
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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpinnerOverlay)
};

/**_____________________________END OF NAMESPACE______________________________*/
} // namespace Whelmed
