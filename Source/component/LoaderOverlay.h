/**
 * @file LoaderOverlay.h
 * @brief Full-screen spinner overlay shown during connection / loading operations.
 *
 * Covers the entire parent component. Text reads: "⠋ [message] (5/194)".
 * Frame advancement is driven exclusively by a 10 Hz juce::Timer when the overlay is visible.
 *
 * Font and colours match MessageOverlay (overlayFamily / overlaySize / overlayColour).
 *
 * @note All methods MESSAGE THREAD.
 * @see MessageOverlay
 */

#pragma once
#include <JuceHeader.h>
#include "../config/Config.h"

class LoaderOverlay : public juce::Component
                    , private juce::Timer
{
public:
    LoaderOverlay()
    {
        setOpaque (false);
        setInterceptsMouseClicks (false, false);
    }

    void show (int total, const juce::String& message = "Building Blocks")
    {
        builtCount = 0;
        totalCount = total;
        frameIndex = 0;
        displayMessage = message;
        setVisible (true);
        startTimerHz (10);
    }

    void update (int built, int total)
    {
        builtCount = built;
        totalCount = total;
        repaint();
    }

    void hide()
    {
        stopTimer();
        setVisible (false);
    }

    void paint (juce::Graphics& g) override
    {
        const auto* cfg { Config::getContext() };

        const auto bgColour      { cfg->getColour (Config::Key::windowColour) };
        const auto fgColour      { cfg->getColour (Config::Key::overlayColour) };

        // Full-screen background — match MessageOverlay alpha
        g.setColour (bgColour.withAlpha (0.8f));
        g.fillRect (getLocalBounds());

        // Progress fill strip at bottom when total is known
        if (totalCount > 0)
        {
            const float ratio { static_cast<float> (builtCount) / static_cast<float> (totalCount) };
            const int fillWidth { static_cast<int> (ratio * static_cast<float> (getWidth())) };
            g.setColour (fgColour.withAlpha (0.3f));
            g.fillRect (0, getHeight() - 2, fillWidth, 2);
        }

        // Spinner + text
        const juce::String progressText { totalCount > 0
                                              ? " (" + juce::String (builtCount) + "/" + juce::String (totalCount) + ")"
                                              : juce::String() };

        g.setFont (juce::FontOptions()
                       .withName (cfg->getString (Config::Key::overlayFamily))
                       .withPointHeight (cfg->getFloat (Config::Key::overlaySize)));

        const juce::String spinnerChar { juce::String::charToString (frames.at ((size_t) frameIndex)) };
        const juce::String labelText { " " + displayMessage + progressText };
        const juce::String fullText { spinnerChar + labelText };

        // Measure full combined string to find centered origin
        juce::GlyphArrangement gaFull;
        gaFull.addLineOfText (g.getCurrentFont(), fullText, 0.0f, 0.0f);
        const float fullWidth { gaFull.getBoundingBox (0, -1, false).getWidth() };

        const float centreX { static_cast<float> (getWidth()) * 0.5f };
        const float originX { centreX - fullWidth * 0.5f };
        const float originY { static_cast<float> (getHeight()) * 0.5f };

        // Measure spinner char alone for offset
        juce::GlyphArrangement gaSpinner;
        gaSpinner.addLineOfText (g.getCurrentFont(), spinnerChar, 0.0f, 0.0f);
        const float spinnerCharWidth { gaSpinner.getBoundingBox (0, -1, false).getWidth() };

        // Draw spinner char
        g.setColour (fgColour);
        g.drawSingleLineText (spinnerChar, static_cast<int> (originX), static_cast<int> (originY), juce::Justification::left);

        // Draw label text offset by spinner width
        g.setColour (fgColour);
        g.drawSingleLineText (labelText, static_cast<int> (originX + spinnerCharWidth), static_cast<int> (originY), juce::Justification::left);
    }

private:
    void timerCallback() override
    {
        frameIndex = (frameIndex + 1) % frameCount;
        repaint();
    }

    int frameIndex { 0 };
    int builtCount { 0 };
    int totalCount { 0 };
    juce::String displayMessage { "Building Blocks" };

    // Braille spinner rotation: 10 frames covering a full cycle
    static constexpr int frameCount { 10 };
    static constexpr std::array<juce::juce_wchar, frameCount> frames {
        0x280B, // ⠋
        0x2819, // ⠙
        0x2839, // ⠹
        0x2838, // ⠸
        0x283C, // ⠼
        0x2834, // ⠴
        0x2826, // ⠦
        0x2827, // ⠧
        0x2807, // ⠇
        0x280F  // ⠏
    };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LoaderOverlay)
};
