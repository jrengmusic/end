/**
 * @file LookAndFeel.cpp
 * @brief Terminal::LookAndFeel implementation — minimal tab bar drawing.
 *
 * @see LookAndFeel.h
 * @see Config
 */

#include "LookAndFeel.h"

namespace Terminal
{

/**
 * @brief Constructs the LookAndFeel with no custom setup.
 *
 * All colour values are read from Config at paint time rather than
 * cached, so config reload applies immediately without requiring
 * a LookAndFeel rebuild.
 *
 * @note MESSAGE THREAD.
 */
LookAndFeel::LookAndFeel() = default;

/**
 * @brief Draws a single tab button — flat rectangle with text.
 *
 * Active tab: full-opacity foreground text with a cursor-coloured
 * indicator line at the bottom. Inactive tabs: reduced-opacity
 * foreground text. Hover state uses an intermediate opacity.
 *
 * @param button      The tab bar button being drawn.
 * @param g           Graphics context.
 * @param isMouseOver True if the mouse is hovering over this tab.
 * @param isMouseDown True if the mouse button is pressed on this tab.
 *
 * @note MESSAGE THREAD — called by JUCE tab bar painting.
 */
void LookAndFeel::drawTabButton (juce::TabBarButton& button, juce::Graphics& g,
                                  bool isMouseOver, bool isMouseDown)
{
    const auto* cfg { Config::getContext() };
    const auto bounds { button.getLocalBounds().toFloat() };
    const bool isActive { button.getToggleState() };

    const auto fgColour { cfg->getColour (Config::Key::coloursForeground) };
    const auto cursorColour { cfg->getColour (Config::Key::coloursCursor) };

    float textAlpha { inactiveAlpha };

    if (isActive)
    {
        textAlpha = activeAlpha;
    }
    else if (isMouseOver or isMouseDown)
    {
        textAlpha = hoverAlpha;
    }

    g.setFont (juce::FontOptions (cfg->getString (Config::Key::overlayFamily),
                                   fontSize, juce::Font::plain));
    g.setColour (fgColour.withAlpha (textAlpha));
    g.drawText (button.getButtonText(), bounds.toNearestInt(),
                juce::Justification::centred, false);

    if (isActive)
    {
        g.setColour (cursorColour);
        g.fillRect (bounds.getX(), bounds.getBottom() - activeIndicatorHeight,
                    bounds.getWidth(), activeIndicatorHeight);
    }
}

/**
 * @brief Draws the tab bar background — translucent fill matching window colour.
 *
 * @param bar  The tab button bar.
 * @param g    Graphics context.
 *
 * @note MESSAGE THREAD — called by JUCE tab bar painting.
 */
void LookAndFeel::drawTabbedButtonBarBackground (juce::TabbedButtonBar& bar,
                                                   juce::Graphics& g)
{
    const auto* cfg { Config::getContext() };
    const auto bgColour { cfg->getColour (Config::Key::windowColour) };
    g.fillAll (bgColour.withAlpha (barAlpha));
}

/**
 * @brief Calculates the best width for a tab button based on its text.
 *
 * Measures the button text using the overlay font at the configured size,
 * then adds horizontal padding on both sides.
 *
 * @param button    The tab bar button to measure.
 * @param tabDepth  The depth (height for horizontal bars) of the tab bar.
 * @return          The calculated width in pixels.
 *
 * @note MESSAGE THREAD — called by JUCE tab bar layout.
 */
int LookAndFeel::getTabButtonBestWidth (juce::TabBarButton& button, int tabDepth)
{
    const auto* cfg { Config::getContext() };
    const juce::Font font { juce::FontOptions (cfg->getString (Config::Key::overlayFamily),
                                                fontSize, juce::Font::plain) };
    const int textWidth { static_cast<int> (std::ceil (
        juce::TextLayout::getStringWidth (font, button.getButtonText()))) };

    return textWidth + horizontalPadding * 2;
}

}
