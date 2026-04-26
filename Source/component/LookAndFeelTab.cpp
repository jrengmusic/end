/**
 * @file LookAndFeelTab.cpp
 * @brief Terminal::LookAndFeel implementation — tab button drawing, width, font, and extras button.
 *
 * @see LookAndFeel.h
 */

#include "LookAndFeel.h"

namespace Terminal
{ /*____________________________________________________________________________*/
/**
 * @brief Applies a rotation transform to @p g for vertical tab orientations.
 *
 * Rotates 90 degrees around the centre of @p area so that vertical tab
 * drawing can reuse the same horizontal path logic.
 *
 * @param g                Graphics context (transform is added in place).
 * @param area             Original tab area rectangle (in component coordinates).
 * @param isLeftOrientation True when the tab bar is on the left side.
 * @return                 The transformed draw area (width and height swapped).
 */
static juce::Rectangle<float> applyVerticalTabTransform (juce::Graphics& g,
                                                         const juce::Rectangle<float>& area,
                                                         bool isLeftOrientation)
{
    const auto centreX { area.getCentreX() };
    const auto centreY { area.getCentreY() };
    const auto angle { isLeftOrientation ? -juce::MathConstants<float>::halfPi
                                         : juce::MathConstants<float>::halfPi };

    g.addTransform (juce::AffineTransform::rotation (angle, centreX, centreY));

    return { area.getCentreX() - area.getHeight() * 0.5f,
             area.getCentreY() - area.getWidth() * 0.5f,
             area.getHeight(),
             area.getWidth() };
}

/**
 * @brief Core tab button drawing shared by both juce:: and jam:: overrides.
 *
 * Extracts all orientation and colour lookups from the calling override so
 * the drawing path is not duplicated.
 *
 * @param lf           The LookAndFeel instance used to retrieve colours and font.
 * @param g            Graphics context.
 * @param area         Active area of the tab button (in component coordinates).
 * @param isActive     True when this is the selected tab.
 * @param isVertical   True when the tab bar is vertical (left or right).
 * @param isLeftOrientation True when the bar is on the left side.
 * @param isMouseOver  True when the mouse hovers over this button.
 * @param isMouseDown  True when the mouse button is pressed.
 */
void LookAndFeel::drawTabButtonCore (LookAndFeel& lf,
                                      juce::Graphics& g,
                                      const juce::Rectangle<float>& area,
                                      bool isActive,
                                      bool isVertical,
                                      bool isLeftOrientation,
                                      bool isMouseOver,
                                      bool isMouseDown)
{
    const auto activeColour    { lf.findColour (LookAndFeel::tabActiveColourId) };
    const auto indicatorColour { lf.findColour (LookAndFeel::tabIndicatorColourId) };
    const auto inactiveColour  { lf.findColour (jam::TabbedButtonBar::tabTextColourId) };

    juce::Graphics::ScopedSaveState saveState (g);

    auto drawArea { area };

    if (isVertical)
        drawArea = applyVerticalTabTransform (g, area, isLeftOrientation);

    if (lf.hasSvgTabButton)
    {
        const auto& leftPath   { isActive ? lf.svgActiveLeft   : lf.svgInactiveLeft };
        const auto& centerPath { isActive ? lf.svgActiveCenter : lf.svgInactiveCenter };
        const auto& rightPath  { isActive ? lf.svgActiveRight  : lf.svgInactiveRight };

        // Compute 3-slice layout from original path bounds
        const auto leftBounds   { leftPath.getBounds() };
        const auto rightBounds  { rightPath.getBounds() };
        const float svgRowHeight { leftBounds.getHeight() };
        const float scaleFactor  { drawArea.getHeight() / svgRowHeight };

        const float leftWidth  { leftBounds.getWidth() * scaleFactor };
        const float rightWidth { rightBounds.getWidth() * scaleFactor };

        const auto leftTarget   { juce::Rectangle<float> (drawArea.getX(), drawArea.getY(),
                                                           leftWidth, drawArea.getHeight()) };
        const auto rightTarget  { juce::Rectangle<float> (drawArea.getRight() - rightWidth, drawArea.getY(),
                                                           rightWidth, drawArea.getHeight()) };
        const auto centerTarget { juce::Rectangle<float> (drawArea.getX() + leftWidth, drawArea.getY(),
                                                           drawArea.getWidth() - leftWidth - rightWidth,
                                                           drawArea.getHeight()) };

        // Choose colour: active = activeColour, inactive hovered = filled, inactive = stroked
        if (isActive)
            g.setColour (activeColour);
        else
            g.setColour (inactiveColour);

        // Draw each slice — transform from original bounds to target rect
        const auto leftTransform   { juce::RectanglePlacement (juce::RectanglePlacement::stretchToFit)
                                         .getTransformToFit (leftBounds, leftTarget) };
        const auto centerTransform { juce::RectanglePlacement (juce::RectanglePlacement::stretchToFit)
                                         .getTransformToFit (centerPath.getBounds(), centerTarget) };
        const auto rightTransform  { juce::RectanglePlacement (juce::RectanglePlacement::stretchToFit)
                                         .getTransformToFit (rightBounds, rightTarget) };

        if (isActive or isMouseOver)
        {
            g.fillPath (leftPath, leftTransform);
            g.fillPath (centerPath, centerTransform);
            g.fillPath (rightPath, rightTransform);
        }
        else
        {
            g.strokePath (leftPath,   juce::PathStrokeType (LookAndFeel::strokeWidth), leftTransform);
            g.strokePath (centerPath, juce::PathStrokeType (LookAndFeel::strokeWidth), centerTransform);
            g.strokePath (rightPath,  juce::PathStrokeType (LookAndFeel::strokeWidth), rightTransform);
        }
    }
    else
    {
        auto buttonArea { drawArea.reduced (0, LookAndFeel::buttonInset) };
        auto indicatorArea { buttonArea.removeFromLeft (isActive ? LookAndFeel::indicatorSize - LookAndFeel::skew : 0) };

        juce::Path path { LookAndFeel::getTabButtonShape (buttonArea) };

        if (isActive)
        {
            juce::Path indicator { LookAndFeel::getTabButtonIndicator (indicatorArea) };
            g.setColour (indicatorColour);
            g.fillPath (indicator);

            g.setColour (activeColour);
            g.fillPath (path);
        }
        else if (isMouseOver)
        {
            g.setColour (inactiveColour);
            g.fillPath (path);
        }
        else
        {
            g.setColour (inactiveColour);
            g.strokePath (path, juce::PathStrokeType (LookAndFeel::strokeWidth));
        }
    }
}

void LookAndFeel::drawTabButton (juce::TabBarButton& button, juce::Graphics& g, bool isMouseOver, bool isMouseDown)
{
    const auto area { button.getActiveArea().toFloat() };
    const bool isActive { button.getToggleState() };
    const auto orientation { button.getTabbedButtonBar().getOrientation() };
    const bool isVertical { orientation == juce::TabbedButtonBar::TabsAtLeft
                            or orientation == juce::TabbedButtonBar::TabsAtRight };
    const bool isLeftOrientation { orientation == juce::TabbedButtonBar::TabsAtLeft };

    drawTabButtonCore (*this, g, area, isActive, isVertical, isLeftOrientation,
                       isMouseOver, isMouseDown);
}

/**
 * @brief Draws the tab bar background — translucent fill matching window colour.
 *
 * @param bar  The tab button bar.
 * @param g    Graphics context.
 *
 * @note MESSAGE THREAD — called by JUCE tab bar painting.
 */
void LookAndFeel::drawTabbedButtonBarBackground (juce::TabbedButtonBar&, juce::Graphics&) {}

// jam::TabbedButtonBar::LookAndFeelMethods implementations
// ---------------------------------------------------------------------------

void LookAndFeel::drawTabButton (jam::TabBarButton& button, juce::Graphics& g,
                                  bool isMouseOver, bool isMouseDown)
{
    const auto area { button.getActiveArea().toFloat() };
    const bool isActive { button.getToggleState() };
    const auto orientation { button.getTabbedButtonBar().getOrientation() };
    const bool isVertical { orientation == jam::TabbedButtonBar::TabsAtLeft
                            or orientation == jam::TabbedButtonBar::TabsAtRight };
    const bool isLeftOrientation { orientation == jam::TabbedButtonBar::TabsAtLeft };

    drawTabButtonCore (*this, g, area, isActive, isVertical, isLeftOrientation,
                       isMouseOver, isMouseDown);
}

void LookAndFeel::drawTabbedButtonBarBackground (jam::TabbedButtonBar&, juce::Graphics&) {}

int LookAndFeel::getTabButtonBestWidth (jam::TabBarButton& button, int tabDepth)
{
    const auto font { getTabButtonFont (button, static_cast<float> (tabDepth)) };
    const float charWidth { juce::TextLayout::getStringWidth (font, "M") };
    const int minWidth { static_cast<int> (charWidth * minTabChars) + horizontalPadding * 2 + static_cast<int> (skew) };
    const int maxWidth { static_cast<int> (charWidth * maxTabChars) + horizontalPadding * 2 + static_cast<int> (skew) };
    const int textWidth { static_cast<int> (
        std::ceil (juce::TextLayout::getStringWidth (font, button.getButtonText()))) };
    const int totalWidth { textWidth + horizontalPadding * 2 + static_cast<int> (skew) };

    return juce::jlimit (minWidth, maxWidth, totalWidth);
}

juce::Font LookAndFeel::getTabButtonFont (jam::TabBarButton&, float)
{
    const auto* cfg { lua::Engine::getContext() };
    return juce::Font { juce::FontOptions()
                            .withName (cfg->display.tab.family)
                            .withPointHeight (cfg->display.tab.size) };
}

juce::Rectangle<int> LookAndFeel::getTabButtonExtraComponentBounds (const jam::TabBarButton&,
                                                                      juce::Rectangle<int>& textArea,
                                                                      juce::Component&)
{
    return textArea;
}

void LookAndFeel::drawTabButtonText (jam::TabBarButton&, juce::Graphics&, bool, bool) {}

void LookAndFeel::drawTabAreaBehindFrontButton (jam::TabbedButtonBar&, juce::Graphics&, int, int) {}

void LookAndFeel::createTabButtonShape (jam::TabBarButton& button, juce::Path& path,
                                         bool, bool)
{
    path = getTabButtonShape (button.getActiveArea().toFloat().reduced (0, buttonInset));
}

void LookAndFeel::fillTabButtonShape (jam::TabBarButton&, juce::Graphics&,
                                       const juce::Path&, bool, bool) {}

// ---------------------------------------------------------------------------

/**
 * @brief Calculates the best width for a tab button based on its text.
 *
 * Measures the button text using the tab font, then adds horizontal
 * padding on both sides.
 *
 * @param button    The tab bar button to measure.
 * @param tabDepth  The depth (height for horizontal bars) of the tab bar.
 * @return          The calculated width in pixels.
 *
 * @note MESSAGE THREAD — called by JUCE tab bar layout.
 */
int LookAndFeel::getTabButtonBestWidth (juce::TabBarButton& button, int tabDepth)
{
    const auto font { getTabButtonFont (button, static_cast<float> (tabDepth)) };
    const float charWidth { juce::TextLayout::getStringWidth (font, "M") };
    const int minWidth { static_cast<int> (charWidth * minTabChars) + horizontalPadding * 2 + static_cast<int> (skew) };
    const int maxWidth { static_cast<int> (charWidth * maxTabChars) + horizontalPadding * 2 + static_cast<int> (skew) };

    const int textWidth { static_cast<int> (
        std::ceil (juce::TextLayout::getStringWidth (font, button.getButtonText()))) };
    const int totalWidth { textWidth + horizontalPadding * 2 + static_cast<int> (skew) };

    return juce::jlimit (minWidth, maxWidth, totalWidth);
}

/**
 * @brief Returns the tab font at the configured point size.
 *
 * Single source of truth for the tab font.  JUCE calls this
 * internally for layout; `drawTabButton` and `getTabButtonBestWidth`
 * also use it.
 *
 * @param button  The tab bar button being queried.
 * @param height  The tab bar depth (height for horizontal bars).
 * @return The tab font at `lua::Engine::display.tab.size` point height.
 * @note MESSAGE THREAD.
 */
juce::Font LookAndFeel::getTabButtonFont (juce::TabBarButton& button, float height)
{
    const auto* cfg { lua::Engine::getContext() };
    return juce::Font { juce::FontOptions()
                            .withName (cfg->display.tab.family)
                            .withPointHeight (cfg->display.tab.size) };
}

/**
 * @brief Creates the "extras" button for the tab bar with a ">" chevron.
 *
 * @return Pointer to the created Button (ownership transferred to caller).
 * @note MESSAGE THREAD — called by JUCE tab bar layout.
 */
juce::Button* LookAndFeel::createTabBarExtrasButton()
{
    // JUCE API convention: createTabBarExtrasButton returns raw pointer, caller takes ownership.
    return new juce::TextButton (">");
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Terminal
