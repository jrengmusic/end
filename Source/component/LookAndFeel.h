/**
 * @file LookAndFeel.h
 * @brief Custom LookAndFeel for the Terminal tab bar.
 *
 * Overrides JUCE's default tab drawing to produce a minimal, translucent
 * tab bar that matches the terminal's visual style. Colours and font are
 * pulled from Config at paint time so that hot-reload applies immediately.
 *
 * @see Terminal::Tabs
 * @see Config::Key::coloursForeground
 * @see Config::Key::coloursCursor
 * @see Config::Key::windowColour
 */

#pragma once
#include <JuceHeader.h>
#include "../config/Config.h"

namespace Terminal
{

/**
 * @class LookAndFeel
 * @brief Minimal tab bar appearance for the terminal emulator.
 *
 * Draws flat rectangular tabs with a translucent bar background.
 * Active tab is highlighted with the cursor colour; inactive tabs
 * use the foreground colour at reduced opacity.
 *
 * All colours are read from Config at paint time (no caching) so that
 * config reload is reflected immediately.
 *
 * @note MESSAGE THREAD — all methods called by JUCE painting system.
 */
class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    LookAndFeel();

    void drawTabButton (juce::TabBarButton& button, juce::Graphics& g,
                        bool isMouseOver, bool isMouseDown) override;

    void drawTabbedButtonBarBackground (juce::TabbedButtonBar& bar,
                                        juce::Graphics& g) override;

    int getTabButtonBestWidth (juce::TabBarButton& button, int tabDepth) override;

private:
    static constexpr float barAlpha { 0.85f };
    static constexpr float activeAlpha { 1.0f };
    static constexpr float inactiveAlpha { 0.5f };
    static constexpr float hoverAlpha { 0.7f };
    static constexpr int horizontalPadding { 24 };
    static constexpr float activeIndicatorHeight { 2.0f };
    static constexpr float fontSize { 13.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LookAndFeel)
};

}
