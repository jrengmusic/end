/**
 * @file LookAndFeel.cpp
 * @brief Terminal::LookAndFeel implementation — core: constructor, colours, fonts, shape helpers, misc overrides.
 *
 * @see LookAndFeel.h
 * @see Config
 */

#include "LookAndFeel.h"

namespace Terminal
{ /*____________________________________________________________________________*/
/**
 * @brief Constructs the LookAndFeel and sets colours from Config.
 *
 * Calls setColours() to read Config once and set all JUCE colour IDs.
 * The paint methods use findColour() to retrieve colours.
 *
 * @note MESSAGE THREAD.
 */
LookAndFeel::LookAndFeel() { setColours(); }

/**
 * @brief Refreshes all colour IDs from Config.
 *
 * Reads Config once and sets all JUCE colour IDs for tabs, popup menus,
 * and text buttons. Call this after Config reload to update colours.
 *
 * @note MESSAGE THREAD.
 */
void LookAndFeel::setColours()
{
    const auto* cfg { lua::Engine::getContext() };
    const auto fg          { cfg->display.colours.foreground };
    const auto windowColour { cfg->display.window.colour };
    const auto menuOpacity  { cfg->display.menu.opacity };

    setColour (cursorColourId, cfg->display.colours.cursor);

    setColour (jam::TabbedButtonBar::tabTextColourId,      cfg->display.tab.inactive);
    setColour (jam::TabbedButtonBar::frontTextColourId,    cfg->display.tab.foreground);
    setColour (jam::TabbedButtonBar::tabOutlineColourId,   juce::Colours::transparentBlack);
    setColour (jam::TabbedButtonBar::frontOutlineColourId, juce::Colours::transparentBlack);
    setColour (jam::TabbedComponent::backgroundColourId,   juce::Colours::transparentBlack);
    setColour (jam::TabbedComponent::outlineColourId,      juce::Colours::transparentBlack);
    setColour (tabBarBackgroundColourId, juce::Colours::transparentBlack);
    setColour (tabLineColourId,          cfg->display.tab.line);
    setColour (tabActiveColourId,        cfg->display.tab.active);
    setColour (tabIndicatorColourId,     cfg->display.tab.indicator);

    setColour (juce::PopupMenu::backgroundColourId,            windowColour.withAlpha (menuOpacity));
    setColour (juce::PopupMenu::textColourId,                  fg);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, windowColour.brighter (0.15f));
    setColour (juce::PopupMenu::highlightedTextColourId,       fg);

    setColour (juce::Label::textColourId, fg);

    setColour (juce::TextButton::textColourOffId,  fg);
    setColour (juce::TextButton::textColourOnId,   fg);
    setColour (juce::TextButton::buttonColourId,   juce::Colours::transparentBlack);
    setColour (juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);
    setColour (juce::ComboBox::outlineColourId,    windowColour.brighter (0.15f));

    setColour (paneBarColourId,          cfg->display.pane.barColour);
    setColour (paneBarHighlightColourId, cfg->display.pane.barHighlight);

    setColour (statusBarBackgroundColourId,      cfg->display.colours.statusBar);
    setColour (statusBarLabelBackgroundColourId, cfg->display.colours.statusBarLabelBg);
    setColour (statusBarLabelTextColourId,       cfg->display.colours.statusBarLabelFg);

    setColour (juce::DocumentWindow::backgroundColourId, windowColour);

    setColour (juce::ScrollBar::thumbColourId,      cfg->whelmed.scrollbarThumb);
    setColour (juce::ScrollBar::trackColourId,      cfg->whelmed.scrollbarTrack);
    setColour (juce::ScrollBar::backgroundColourId, cfg->whelmed.scrollbarBackground);

    loadTabButtonSvg();
}

/**
 * @brief Loads custom SVG tab button paths from the configured path.
 *
 * Reads `display.tab.button_svg` from config. When non-empty, resolves the path
 * (relative paths are anchored to the config directory), parses the SVG, locates
 * the six required elements by ID ("active-left", "active-center", "active-right",
 * "inactive-left", "inactive-center", "inactive-right"), and extracts each as a
 * cached `juce::Path` via `jam::SVG::getPath`. Sets `hasSvgTabButton` true only
 * when all six paths are non-empty. On any failure, emits a DBG warning and leaves
 * `hasSvgTabButton` false so drawing falls back to the built-in parallelogram.
 *
 * @note MESSAGE THREAD — called at end of setColours().
 */
void LookAndFeel::loadTabButtonSvg()
{
    hasSvgTabButton = false;
    svgActiveLeft     = {};
    svgActiveCenter   = {};
    svgActiveRight    = {};
    svgInactiveLeft   = {};
    svgInactiveCenter = {};
    svgInactiveRight  = {};

    const auto* cfg { lua::Engine::getContext() };
    const auto svgPath { cfg->display.tab.buttonSvg };

    if (svgPath.isNotEmpty())
    {
        const juce::File svgFile { juce::File::isAbsolutePath (svgPath)
                                       ? juce::File (svgPath)
                                       : lua::Engine::getConfigPath().getChildFile (svgPath) };

        if (svgFile.existsAsFile())
        {
            auto xml { juce::parseXML (svgFile) };

            if (xml != nullptr)
            {
                auto* activeLeftEl     { jam::XML::getChildByID (xml, "active-left") };
                auto* activeCenterEl   { jam::XML::getChildByID (xml, "active-center") };
                auto* activeRightEl    { jam::XML::getChildByID (xml, "active-right") };
                auto* inactiveLeftEl   { jam::XML::getChildByID (xml, "inactive-left") };
                auto* inactiveCenterEl { jam::XML::getChildByID (xml, "inactive-center") };
                auto* inactiveRightEl  { jam::XML::getChildByID (xml, "inactive-right") };

                if (activeLeftEl != nullptr
                    and activeCenterEl != nullptr
                    and activeRightEl != nullptr
                    and inactiveLeftEl != nullptr
                    and inactiveCenterEl != nullptr
                    and inactiveRightEl != nullptr)
                {
                    svgActiveLeft     = jam::SVG::getPath (activeLeftEl,    jam::SVG::ElementType::all);
                    svgActiveCenter   = jam::SVG::getPath (activeCenterEl,  jam::SVG::ElementType::all);
                    svgActiveRight    = jam::SVG::getPath (activeRightEl,   jam::SVG::ElementType::all);
                    svgInactiveLeft   = jam::SVG::getPath (inactiveLeftEl,  jam::SVG::ElementType::all);
                    svgInactiveCenter = jam::SVG::getPath (inactiveCenterEl, jam::SVG::ElementType::all);
                    svgInactiveRight  = jam::SVG::getPath (inactiveRightEl,  jam::SVG::ElementType::all);

                    hasSvgTabButton = not svgActiveLeft.isEmpty()
                                      and not svgActiveCenter.isEmpty()
                                      and not svgActiveRight.isEmpty()
                                      and not svgInactiveLeft.isEmpty()
                                      and not svgInactiveCenter.isEmpty()
                                      and not svgInactiveRight.isEmpty();
                }

                if (not hasSvgTabButton)
                    DBG ("LookAndFeel::loadTabButtonSvg: failed to extract paths from " + svgFile.getFullPathName());
            }
            else
            {
                DBG ("LookAndFeel::loadTabButtonSvg: failed to parse SVG: " + svgFile.getFullPathName());
            }
        }
        else
        {
            DBG ("LookAndFeel::loadTabButtonSvg: file not found: " + svgFile.getFullPathName());
        }
    }
}

/**
 * @brief Computes the tab bar height from the configured tab font.
 *
 * Queries the real rendered font height from `getTabButtonFont()` and
 * derives the bar height so the font occupies 50% of the bar.
 *
 * @return Tab bar height in pixels, rounded to nearest integer.
 * @note MESSAGE THREAD.
 */
int LookAndFeel::getTabBarHeight() noexcept
{
    const auto* cfg { lua::Engine::getContext() };
    const juce::Font font { juce::FontOptions()
                                .withName (cfg->display.tab.family)
                                .withPointHeight (cfg->display.tab.size) };
    return juce::roundToInt (font.getHeight() / tabFontRatio);
}

/**
 * @brief Returns the font used for popup menu items.
 *
 * Uses the same font family and point size as tab buttons, via Config.
 *
 * @return The popup menu font at configured tab size.
 * @note MESSAGE THREAD.
 */
juce::Font LookAndFeel::getPopupMenuFont()
{
    const auto* cfg { lua::Engine::getContext() };
    return juce::Font { juce::FontOptions()
                            .withName (cfg->display.tab.family)
                            .withPointHeight (cfg->display.tab.size) };
}

/**
 * @brief Dispatches text button fonts via component property inspection.
 *
 * Reads the `font` property from the button's property map.  A value equal to
 * `jam::ID::name` selects the action list name font (same branch as
 * getLabelFont's `name`-role path).  All other buttons fall back to the
 * configured tab font at 60 % of the button height.
 *
 * @param button       The text button being queried.
 * @param buttonHeight The button height in pixels.
 * @return             The resolved font for the given button.
 * @note MESSAGE THREAD.
 */
juce::Font LookAndFeel::getTextButtonFont (juce::TextButton& button, int buttonHeight)
{
    const auto* cfg { lua::Engine::getContext() };
    const auto fontRole { button.getProperties()[jam::ID::font].toString() };

    juce::Font result { juce::FontOptions()
                            .withName (cfg->display.tab.family)
                            .withPointHeight (static_cast<float> (buttonHeight) * 0.6f) };

    if (fontRole == jam::ID::name.toString())
        result = juce::Font { juce::FontOptions()
                                  .withName (cfg->display.actionList.nameFamily)
                                  .withStyle (cfg->display.actionList.nameStyle)
                                  .withPointHeight (cfg->display.actionList.nameSize) };

    return result;
}

/**
 * @brief Dispatches label fonts via component property inspection.
 *
 * Reads the `font` property from the label's property map.  A value equal to
 * `jam::ID::name` selects the action list name font; a value equal to
 * `jam::ID::keyPress` selects the action list shortcut font.  All other labels
 * fall back to LookAndFeel_V4 default behaviour.
 *
 * @param label  The label being queried.
 * @return       The resolved font for the given label.
 * @note MESSAGE THREAD.
 */
juce::Font LookAndFeel::getLabelFont (juce::Label& label)
{
    juce::Font result { juce::LookAndFeel_V4::getLabelFont (label) };

    const auto& props { label.getProperties() };
    const auto fontRole { props[jam::ID::font].toString() };
    const auto* cfg { lua::Engine::getContext() };

    if (fontRole == jam::ID::name.toString())
    {
        result = juce::Font { juce::FontOptions()
                                  .withName (cfg->display.actionList.nameFamily)
                                  .withStyle (cfg->display.actionList.nameStyle)
                                  .withPointHeight (cfg->display.actionList.nameSize) };
    }
    else if (fontRole == jam::ID::keyPress.toString())
    {
        result = juce::Font { juce::FontOptions()
                                  .withName (cfg->display.actionList.shortcutFamily)
                                  .withStyle (cfg->display.actionList.shortcutStyle)
                                  .withPointHeight (cfg->display.actionList.shortcutSize) };
    }

    return result;
}

void LookAndFeel::drawStretchableLayoutResizerBar (juce::Graphics& g,
                                                   int w,
                                                   int h,
                                                   bool isVerticalBar,
                                                   bool isMouseOver,
                                                   bool isMouseDragging)
{
    const auto colour { (isMouseOver or isMouseDragging) ? findColour (paneBarHighlightColourId)
                                                         : findColour (paneBarColourId) };
    g.setColour (colour);

    if (isVerticalBar)
    {
        const float centreX { w * 0.5f };
        g.drawLine (centreX, 0.0f, centreX, static_cast<float> (h), 1.0f);
    }
    else
    {
        const float centreY { h * 0.5f };
        g.drawLine (0.0f, centreY, static_cast<float> (w), centreY, 1.0f);
    }
}

juce::Path LookAndFeel::getTabButtonIndicator (const juce::Rectangle<float>& area) noexcept
{
    juce::Path p;
    p.startNewSubPath (area.getX() + skew, area.getY());
    p.lineTo (area.getRight() + (skew - gap), area.getY());
    p.lineTo (area.getRight() - gap, area.getBottom());
    p.lineTo (area.getX(), area.getBottom());
    p.closeSubPath();
    return p;
}

juce::Path LookAndFeel::getTabButtonShape (const juce::Rectangle<float>& area) noexcept
{
    juce::Path p;
    p.startNewSubPath (area.getX() + skew, area.getY());
    p.lineTo (area.getRight(), area.getY());
    p.lineTo (area.getRight() - skew, area.getBottom());
    p.lineTo (area.getX(), area.getBottom());
    p.closeSubPath();
    return p;
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Terminal
