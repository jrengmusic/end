/**
 * @file LookAndFeel.cpp
 * @brief terminal::LookAndFeel implementation — core: constructor, colours, fonts, shape helpers, misc overrides.
 *
 * @see LookAndFeel.h
 * @see Config
 */

#include "LookAndFeel.h"

namespace terminal
{
/*____________________________________________________________________________*/
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
    const auto* appState { AppModel::getContext() };
    const auto fg           { juce::Colour (static_cast<juce::uint32> (appState->getValue<int> (app::id::DISPLAY_LUA, app::id::foreground))) };
    const auto windowColour { juce::Colour (static_cast<juce::uint32> (appState->getValue<int> (app::id::DISPLAY_LUA, app::id::windowColour))) };
    const auto menuOpacity  { appState->getValue<float> (app::id::DISPLAY_LUA, app::id::menuOpacity) };
    const auto cursorColour { juce::Colour (static_cast<juce::uint32> (appState->getValue<int> (app::id::DISPLAY_LUA, app::id::cursorColour))) };

    setColour (cursorColourId, cursorColour);

    setColour (jam::TabbedButtonBar::tabTextColourId,      juce::Colour (static_cast<juce::uint32> (appState->getValue<int> (app::id::DISPLAY_LUA, app::id::tabInactive))));
    setColour (jam::TabbedButtonBar::frontTextColourId,    juce::Colour (static_cast<juce::uint32> (appState->getValue<int> (app::id::DISPLAY_LUA, app::id::tabForeground))));
    setColour (jam::TabbedButtonBar::tabOutlineColourId,   juce::Colours::transparentBlack);
    setColour (jam::TabbedButtonBar::frontOutlineColourId, juce::Colours::transparentBlack);
    setColour (jam::TabbedComponent::backgroundColourId,   juce::Colours::transparentBlack);
    setColour (jam::TabbedComponent::outlineColourId,      juce::Colours::transparentBlack);
    setColour (tabBarBackgroundColourId, juce::Colours::transparentBlack);
    setColour (tabLineColourId,          juce::Colour (static_cast<juce::uint32> (appState->getValue<int> (app::id::DISPLAY_LUA, app::id::tabLine))));
    setColour (tabActiveColourId,        juce::Colour (static_cast<juce::uint32> (appState->getValue<int> (app::id::DISPLAY_LUA, app::id::tabActive))));
    setColour (tabIndicatorColourId,     juce::Colour (static_cast<juce::uint32> (appState->getValue<int> (app::id::DISPLAY_LUA, app::id::tabIndicator))));

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

    setColour (paneBarColourId,          juce::Colour (static_cast<juce::uint32> (appState->getValue<int> (app::id::DISPLAY_LUA, app::id::paneBarColour))));
    setColour (paneBarHighlightColourId, juce::Colour (static_cast<juce::uint32> (appState->getValue<int> (app::id::DISPLAY_LUA, app::id::paneBarHighlight))));

    setColour (statusBarBackgroundColourId,      juce::Colour (static_cast<juce::uint32> (appState->getValue<int> (app::id::DISPLAY_LUA, app::id::statusBarColour))));
    setColour (statusBarLabelBackgroundColourId, juce::Colour (static_cast<juce::uint32> (appState->getValue<int> (app::id::DISPLAY_LUA, app::id::statusBarLabelBg))));
    setColour (statusBarLabelTextColourId,       juce::Colour (static_cast<juce::uint32> (appState->getValue<int> (app::id::DISPLAY_LUA, app::id::statusBarLabelFg))));

    setColour (juce::DocumentWindow::backgroundColourId, windowColour);

    setColour (juce::ScrollBar::thumbColourId,      juce::Colour (static_cast<juce::uint32> (appState->getValue<int> (app::id::DISPLAY_LUA, app::id::scrollbarThumb))));
    setColour (juce::ScrollBar::trackColourId,      juce::Colour (static_cast<juce::uint32> (appState->getValue<int> (app::id::DISPLAY_LUA, app::id::scrollbarTrack))));
    setColour (juce::ScrollBar::backgroundColourId, juce::Colours::transparentBlack);

    setColour (jam::CodeView::backgroundColourId,     juce::Colour (static_cast<juce::uint32> (appState->getValue<int> (app::id::DISPLAY_LUA, app::id::background))));
    setColour (jam::CodeView::outlineColourId,        juce::Colour (static_cast<juce::uint32> (appState->getValue<int> (app::id::DISPLAY_LUA, app::id::editorOutline))));
    setColour (jam::CodeView::focusedOutlineColourId, juce::Colour (static_cast<juce::uint32> (appState->getValue<int> (app::id::DISPLAY_LUA, app::id::editorOutline))));
    setColour (jam::CodeView::textColourId,           fg);
    setColour (jam::CaretComponent::caretColourId,    cursorColour);
    setColour (Display::cursorColourId,               cursorColour);
    setColour (Display::selectionColourId,            juce::Colour (static_cast<juce::uint32> (appState->getValue<int> (app::id::DISPLAY_LUA, app::id::selectionColour))));
    setColour (Display::selectionCursorColourId,      juce::Colour (static_cast<juce::uint32> (appState->getValue<int> (app::id::DISPLAY_LUA, app::id::selectionCursorColour))));
    setColour (Display::hintLabelFgColourId,          juce::Colour (static_cast<juce::uint32> (appState->getValue<int> (app::id::DISPLAY_LUA, app::id::hintLabelFg))));
    setColour (Display::hintLabelBgColourId,          juce::Colour (static_cast<juce::uint32> (appState->getValue<int> (app::id::DISPLAY_LUA, app::id::hintLabelBg))));

    static const std::array<const juce::Identifier*, 16> ansiIds
    {
        &app::id::ansi0,  &app::id::ansi1,  &app::id::ansi2,  &app::id::ansi3,
        &app::id::ansi4,  &app::id::ansi5,  &app::id::ansi6,  &app::id::ansi7,
        &app::id::ansi8,  &app::id::ansi9,  &app::id::ansi10, &app::id::ansi11,
        &app::id::ansi12, &app::id::ansi13, &app::id::ansi14, &app::id::ansi15
    };

    for (int i { 0 }; i < 16; ++i)
    {
        const juce::Colour ansi { juce::Colour (static_cast<juce::uint32> (appState->getValue<int> (app::id::DISPLAY_LUA, *ansiIds[static_cast<size_t> (i)]))) };
        setColour (Display::ansi0ColourId + i, ansi);
        terminal::setAnsi16Colour (i, ansi);
    }

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

    const auto svgPath { AppModel::getContext()->getValue<juce::String> (app::id::DISPLAY_LUA, app::id::tabButtonSvg) };

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
    const auto* appState { AppModel::getContext() };
    const juce::Font font { juce::FontOptions()
                                .withName (appState->getValue<juce::String> (app::id::DISPLAY_LUA, app::id::tabFamily))
                                .withPointHeight (appState->getValue<float> (app::id::DISPLAY_LUA, app::id::tabSize)) };
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
    const auto* appState { AppModel::getContext() };
    return juce::Font { juce::FontOptions()
                            .withName (appState->getValue<juce::String> (app::id::DISPLAY_LUA, app::id::tabFamily))
                            .withPointHeight (appState->getValue<float> (app::id::DISPLAY_LUA, app::id::tabSize)) };
}

juce::Font LookAndFeel::getTextButtonFont (juce::TextButton& button, int buttonHeight)
{
    const auto* appState { AppModel::getContext() };
    const auto fontRole { button.getProperties()[jam::ID::font].toString() };

    juce::Font result { juce::FontOptions()
                            .withName (appState->getValue<juce::String> (app::id::DISPLAY_LUA, app::id::tabFamily))
                            .withPointHeight (static_cast<float> (buttonHeight) * 0.6f) };

    if (fontRole == jam::ID::name.toString())
        result = juce::Font { juce::FontOptions()
                                  .withName (appState->getValue<juce::String> (app::id::DISPLAY_LUA, app::id::actionListNameFamily))
                                  .withStyle (appState->getValue<juce::String> (app::id::DISPLAY_LUA, app::id::actionListNameStyle))
                                  .withPointHeight (appState->getValue<float> (app::id::DISPLAY_LUA, app::id::actionListNameSize)) };

    return result;
}

juce::Font LookAndFeel::getLabelFont (juce::Label& label)
{
    juce::Font result { juce::LookAndFeel_V4::getLabelFont (label) };

    const auto& props { label.getProperties() };
    const auto fontRole { props[jam::ID::font].toString() };
    const auto* appState { AppModel::getContext() };

    if (fontRole == jam::ID::name.toString())
    {
        result = juce::Font { juce::FontOptions()
                                  .withName (appState->getValue<juce::String> (app::id::DISPLAY_LUA, app::id::actionListNameFamily))
                                  .withStyle (appState->getValue<juce::String> (app::id::DISPLAY_LUA, app::id::actionListNameStyle))
                                  .withPointHeight (appState->getValue<float> (app::id::DISPLAY_LUA, app::id::actionListNameSize)) };
    }
    else if (fontRole == jam::ID::keyPress.toString())
    {
        result = juce::Font { juce::FontOptions()
                                  .withName (appState->getValue<juce::String> (app::id::DISPLAY_LUA, app::id::actionListShortcutFamily))
                                  .withStyle (appState->getValue<juce::String> (app::id::DISPLAY_LUA, app::id::actionListShortcutStyle))
                                  .withPointHeight (appState->getValue<float> (app::id::DISPLAY_LUA, app::id::actionListShortcutSize)) };
    }

    return result;
}

/**
 * @brief Returns the terminal scrollbar width from config.
 *
 * @return Scrollbar width in pixels as set by `lua::Engine::display.scrollbarWidth`.
 * @note MESSAGE THREAD.
 */
int LookAndFeel::getDefaultScrollbarWidth()
{
    return AppModel::getContext()->getValue<int> (app::id::DISPLAY_LUA, app::id::scrollbarWidth);
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
} // namespace terminal
