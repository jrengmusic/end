/**
 * @file LookAndFeel.cpp
 * @brief Terminal::LookAndFeel implementation — minimal tab bar drawing.
 *
 * @see LookAndFeel.h
 * @see Config
 */

#include "LookAndFeel.h"
#include "../config/WhelmedConfig.h"

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
    const auto* cfg { Config::getContext() };
    const auto fg { cfg->getColour (Config::Key::coloursForeground) };
    const auto cursor { cfg->getColour (Config::Key::coloursCursor) };
    const auto windowColour { cfg->getColour (Config::Key::windowColour) };
    const auto menuOpacity { cfg->getFloat (Config::Key::menuOpacity) };

    setColour (cursorColourId, cursor);

    setColour (juce::TabbedButtonBar::tabTextColourId, cfg->getColour (Config::Key::tabInactive));
    setColour (juce::TabbedButtonBar::frontTextColourId, cfg->getColour (Config::Key::tabForeground));
    setColour (juce::TabbedButtonBar::tabOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::TabbedButtonBar::frontOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::TabbedComponent::backgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::TabbedComponent::outlineColourId, juce::Colours::transparentBlack);
    setColour (tabBarBackgroundColourId, juce::Colours::transparentBlack);
    setColour (tabLineColourId, cfg->getColour (Config::Key::tabLine));
    setColour (tabActiveColourId, cfg->getColour (Config::Key::tabActive));
    setColour (tabIndicatorColourId, cfg->getColour (Config::Key::tabIndicator));

    setColour (juce::PopupMenu::backgroundColourId, windowColour.withAlpha (menuOpacity));
    setColour (juce::PopupMenu::textColourId, fg);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, windowColour.brighter (0.15f));///< corbeau
    setColour (juce::PopupMenu::highlightedTextColourId, fg);

    setColour (juce::Label::textColourId, fg);

    setColour (juce::TextButton::textColourOffId, fg);
    setColour (juce::TextButton::textColourOnId, fg);
    setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    setColour (juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);
    setColour (juce::ComboBox::outlineColourId, windowColour.brighter (0.15f));///< corbeau

    setColour (paneBarColourId, cfg->getColour (Config::Key::paneBarColour));
    setColour (paneBarHighlightColourId, cfg->getColour (Config::Key::paneBarHighlight));

    setColour (statusBarBackgroundColourId, cfg->getColour (Config::Key::coloursStatusBar));
    setColour (statusBarLabelBackgroundColourId, cfg->getColour (Config::Key::coloursStatusBarLabelBg));
    setColour (statusBarLabelTextColourId, cfg->getColour (Config::Key::coloursStatusBarLabelFg));

    setColour (juce::ResizableWindow::backgroundColourId, cfg->getColour (Config::Key::coloursBackground));

    if (auto* whelmedCfg { Whelmed::Config::getContext() })
    {
        setColour (juce::ScrollBar::thumbColourId, whelmedCfg->getColour (Whelmed::Config::Key::scrollbarThumb));
        setColour (juce::ScrollBar::trackColourId, whelmedCfg->getColour (Whelmed::Config::Key::scrollbarTrack));
        setColour (
            juce::ScrollBar::backgroundColourId, whelmedCfg->getColour (Whelmed::Config::Key::scrollbarBackground));
    }
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
 * @return The tab font at `Config::Key::tabSize` point height.
 * @note MESSAGE THREAD.
 */
juce::Font LookAndFeel::getTabButtonFont (juce::TabBarButton& button, float height)
{
    const auto* cfg { Config::getContext() };
    return juce::Font { juce::FontOptions()
                            .withName (cfg->getString (Config::Key::tabFamily))
                            .withPointHeight (cfg->getFloat (Config::Key::tabSize)) };
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
    const auto* cfg { Config::getContext() };
    const juce::Font font { juce::FontOptions()
                                .withName (cfg->getString (Config::Key::tabFamily))
                                .withPointHeight (cfg->getFloat (Config::Key::tabSize)) };
    return juce::roundToInt (font.getHeight() / tabFontRatio);
}

/**
 * @brief Applies a rotation transform to @p g for vertical tab orientations.
 *
 * Rotates 90 degrees around the centre of @p area so that vertical tab
 * drawing can reuse the same horizontal path logic.
 *
 * @param g            Graphics context (transform is added in place).
 * @param area         Original tab area rectangle (in component coordinates).
 * @param orientation  The tab bar orientation being drawn.
 * @return             The transformed draw area (width and height swapped).
 */
static juce::Rectangle<float> applyVerticalTabTransform (juce::Graphics& g,
                                                         const juce::Rectangle<float>& area,
                                                         juce::TabbedButtonBar::Orientation orientation)
{
    const auto centreX { area.getCentreX() };
    const auto centreY { area.getCentreY() };
    const auto angle { orientation == juce::TabbedButtonBar::TabsAtLeft ? -juce::MathConstants<float>::halfPi
                                                                        : juce::MathConstants<float>::halfPi };

    g.addTransform (juce::AffineTransform::rotation (angle, centreX, centreY));

    return { area.getCentreX() - area.getHeight() * 0.5f,
             area.getCentreY() - area.getWidth() * 0.5f,
             area.getHeight(),
             area.getWidth() };
}

/**
 * @brief Truncates @p text so it fits within @p maxWidth using ellipsis prefix.
 *
 * Repeatedly removes the first character until the text fits.
 * Returns the original text unchanged if it already fits.
 *
 * @param font      Font used to measure the text.
 * @param text      Input text to truncate.
 * @param maxWidth  Maximum allowed width in pixels.
 * @return          The (possibly truncated) text, prefixed with "..." when truncated.
 */
static juce::String truncateTabText (const juce::Font& font, const juce::String& text, float maxWidth)
{
    juce::String result { text };

    if (juce::TextLayout::getStringWidth (font, result) > maxWidth)
    {
        while (result.length() > 1 and juce::TextLayout::getStringWidth (font, "..." + result) > maxWidth)
        {
            result = result.substring (1);
        }

        result = "..." + result;
    }

    return result;
}

/**
 * @brief Draws a single tab button with a p shape.
 *
 * Active tab: filled with tabActiveColourId.
 * Inactive tabs: stroked outline with tabTextColourId.
 * Hover on inactive: filled with tabTextColourId.
 * Text uses frontTextColourId for active, tabTextColourId for inactive.
 * Parallelogram slants right (follows reading direction for vertical tabs).
 *
 * @param button      The tab bar button being drawn.
 * @param g           Graphics context.
 * @param isMouseOver True if the mouse is hovering over this tab.
 * @param isMouseDown True if the mouse button is pressed on this tab.
 *
 * @note MESSAGE THREAD — called by JUCE tab bar painting.
 */
void LookAndFeel::drawTabButton (juce::TabBarButton& button, juce::Graphics& g, bool isMouseOver, bool isMouseDown)
{
    const auto area { button.getActiveArea().toFloat() };
    const bool isActive { button.getToggleState() };
    const auto orientation { button.getTabbedButtonBar().getOrientation() };

    const auto activeColour { findColour (tabActiveColourId) };
    const auto indicatorColour { findColour (tabIndicatorColourId) };
    const auto fgColour { findColour (juce::TabbedButtonBar::frontTextColourId) };
    const auto inactiveColour { findColour (juce::TabbedButtonBar::tabTextColourId) };

    const bool isVertical { orientation == juce::TabbedButtonBar::TabsAtLeft
                            or orientation == juce::TabbedButtonBar::TabsAtRight };

    juce::Graphics::ScopedSaveState saveState (g);

    auto drawArea { area };

    if (isVertical)
        drawArea = applyVerticalTabTransform (g, area, orientation);

    auto buttonArea { drawArea.reduced (0, buttonInset) };
    auto indicatorArea { buttonArea.removeFromLeft (isActive ? indicatorSize - skew : 0) };

    juce::Path path { getTabButtonShape (buttonArea) };

    if (isActive)
    {
        juce::Path indicator { getTabButtonIndicator (indicatorArea) };
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
        g.strokePath (path, juce::PathStrokeType (strokeWidth));
    }

    auto font { getTabButtonFont (button, buttonArea.getHeight()) };

    if (isActive)
        font = font.boldened();

    g.setFont (font);
    g.setColour (isActive ? fgColour : (isMouseOver ? fgColour : inactiveColour));

    const float maxTextWidth { juce::TextLayout::getStringWidth (font, "M") * maxTabChars };
    const auto text { truncateTabText (font, button.getButtonText(), maxTextWidth) };
    g.drawFittedText (text, buttonArea.toNearestInt(), juce::Justification::centred, 1);
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
 * @brief Returns the font used for popup menu items.
 *
 * Uses the same font family and point size as tab buttons, via Config.
 *
 * @return The popup menu font at configured tab size.
 * @note MESSAGE THREAD.
 */
juce::Font LookAndFeel::getPopupMenuFont()
{
    const auto* cfg { Config::getContext() };
    return juce::Font { juce::FontOptions()
                            .withName (cfg->getString (Config::Key::tabFamily))
                            .withPointHeight (cfg->getFloat (Config::Key::tabSize)) };
}

/**
 * @brief Prepares a popup menu window for display with background blur.
 *
 * Applies platform-specific background blur to the popup menu window
 * asynchronously after the window is created. Only available on Mac
 * and Windows platforms.
 *
 * @param newWindow  The popup menu window to prepare.
 * @note MESSAGE THREAD — called from UI show logic.
 */
void LookAndFeel::preparePopupMenuWindow (juce::Component& newWindow)
{
    newWindow.setOpaque (false);

#if JUCE_MAC || JUCE_WINDOWS
    auto safeComponent { juce::Component::SafePointer<juce::Component> (&newWindow) };

    juce::MessageManager::callAsync (
        [safeComponent]
        {
            if (safeComponent != nullptr)
            {
                const auto* cfg { Config::getContext() };
                const auto windowColour { cfg->getColour (Config::Key::windowColour) };
                const auto menuOpacity { cfg->getFloat (Config::Key::menuOpacity) };
                jreng::BackgroundBlur::enable (safeComponent.getComponent(),
                                               cfg->getFloat (Config::Key::windowBlurRadius),
                                               windowColour.withAlpha (menuOpacity));
            }
        });
#endif
}

/**
 * @brief Draws a separator line centred vertically within @p area.
 *
 * @param g     Graphics context (colour must be set by caller).
 * @param area  Bounding rectangle for the separator item.
 */
static void drawPopupMenuSeparator (juce::Graphics& g, const juce::Rectangle<int>& area)
{
    const auto y { area.getY() + area.getHeight() / 2 };

    g.drawLine (static_cast<float> (area.getX()),
                static_cast<float> (y),
                static_cast<float> (area.getRight()),
                static_cast<float> (y));
}

/**
 * @brief Draws the submenu arrow chevron at the right edge of @p area.
 *
 * @param g           Graphics context (colour must be set by caller).
 * @param area        Bounding rectangle for the full menu item.
 * @param fontHeight  Font height — used to derive arrow proportions.
 */
static void drawSubmenuArrow (juce::Graphics& g, const juce::Rectangle<int>& area, float fontHeight)
{
    const auto arrowSize { fontHeight * 0.4f };
    const auto arrowX { static_cast<float> (area.getRight()) - arrowSize * 2.0f };
    const auto arrowY { static_cast<float> (area.getCentre().getY()) };

    juce::Path arrow;
    arrow.startNewSubPath (arrowX, arrowY - arrowSize);
    arrow.lineTo (arrowX + arrowSize, arrowY);
    arrow.lineTo (arrowX, arrowY + arrowSize);

    g.strokePath (arrow, juce::PathStrokeType (fontHeight * 0.15f));
}

/**
 * @brief Draws a single popup menu item with theme styling.
 *
 * Handles normal, highlighted, active, separator, ticked, and disabled
 * states. Uses foreground colour for text, selection colour for highlights,
 * and cursor colour for ticked items.
 *
 * @param g                  Graphics context.
 * @param area               Bounding rectangle for the item.
 * @param isSeparator        True if this item is a separator.
 * @param isActive           True if the item is enabled.
 * @param isHighlighted      True if the item is hovered.
 * @param isTicked           True if the item is checked.
 * @param hasSubMenu         True if the item has a submenu.
 * @param text               Item text.
 * @param shortcutKeyText    Optional keyboard shortcut text.
 * @param icon               Optional icon drawable.
 * @param textColourToUse    Optional text colour override.
 * @note MESSAGE THREAD — called by JUCE popup menu item painting.
 */
void LookAndFeel::drawPopupMenuItem (juce::Graphics& g,
                                     const juce::Rectangle<int>& area,
                                     const bool isSeparator,
                                     const bool isActive,
                                     const bool isHighlighted,
                                     const bool isTicked,
                                     const bool hasSubMenu,
                                     const juce::String& text,
                                     const juce::String& shortcutKeyText,
                                     const juce::Drawable* icon,
                                     const juce::Colour* const textColourToUse)
{
    const auto fgColour { findColour (juce::PopupMenu::textColourId) };
    const auto cursorColour { findColour (cursorColourId) };
    const auto highlightColour { findColour (juce::PopupMenu::highlightedBackgroundColourId) };

    if (isSeparator)
    {
        g.setColour (fgColour.withAlpha (separatorAlpha));
        drawPopupMenuSeparator (g, area);
    }
    else
    {
        if (isHighlighted and isActive)
        {
            g.setColour (highlightColour);
            g.fillRect (area);
        }

        g.setFont (getPopupMenuFont());

        juce::Colour textColour { fgColour };

        if (textColourToUse != nullptr)
        {
            textColour = *textColourToUse;
        }
        else if (not isActive)
        {
            textColour = fgColour.withAlpha (0.5f);
        }
        else if (isTicked)
        {
            textColour = cursorColour;
        }

        g.setColour (textColour);

        const auto font { getPopupMenuFont() };
        const auto fontHeight { font.getHeight() };
        auto r { area.reduced (static_cast<int> (fontHeight * 0.5f), 0) };
        auto iconArea { r.removeFromLeft (juce::roundToInt (fontHeight)) };

        if (isTicked)
            g.drawText (">", iconArea, juce::Justification::centred, false);

        g.drawText (text, r, juce::Justification::centredLeft, false);

        if (not shortcutKeyText.isEmpty())
        {
            auto mutableArea { area };
            const auto shortcutArea { mutableArea.removeFromRight (mutableArea.getWidth() / 3) };
            g.drawText (shortcutKeyText, shortcutArea, juce::Justification::centredRight, false);
        }

        if (hasSubMenu)
            drawSubmenuArrow (g, area, fontHeight);
    }
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

/**
 * @brief Dispatches text button fonts via component property inspection.
 *
 * Reads the `font` property from the button's property map.  A value equal to
 * `jreng::ID::name` selects the action list name font (same branch as
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
    const auto* cfg { Config::getContext() };

    juce::Font result {
        juce::FontOptions().withName (cfg->getString (Config::Key::tabFamily)).withPointHeight (buttonHeight * 0.6f)
    };

    const auto fontRole { button.getProperties()[jreng::ID::font].toString() };

    if (fontRole == jreng::ID::name.toString())
    {
        result = juce::Font { juce::FontOptions()
                                  .withName (cfg->getString (Config::Key::actionListNameFamily))
                                  .withStyle (cfg->getString (Config::Key::actionListNameStyle))
                                  .withPointHeight (cfg->getFloat (Config::Key::actionListNameSize)) };
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

/**
 * @brief Dispatches label fonts via component property inspection.
 *
 * Reads the `font` property from the label's property map.  A value equal to
 * `jreng::ID::name` selects the action list name font; a value equal to
 * `jreng::ID::keyPress` selects the action list shortcut font.  All other labels
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
    const auto fontRole { props[jreng::ID::font].toString() };

    if (fontRole == jreng::ID::name.toString())
    {
        const auto* cfg { Config::getContext() };
        result = juce::Font { juce::FontOptions()
                                  .withName (cfg->getString (Config::Key::actionListNameFamily))
                                  .withStyle (cfg->getString (Config::Key::actionListNameStyle))
                                  .withPointHeight (cfg->getFloat (Config::Key::actionListNameSize)) };
    }
    else if (fontRole == jreng::ID::keyPress.toString())
    {
        const auto* cfg { Config::getContext() };
        result = juce::Font { juce::FontOptions()
                                  .withName (cfg->getString (Config::Key::actionListShortcutFamily))
                                  .withStyle (cfg->getString (Config::Key::actionListShortcutStyle))
                                  .withPointHeight (cfg->getFloat (Config::Key::actionListShortcutSize)) };
    }

    return result;
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

