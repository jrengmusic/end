/**
 * @file LookAndFeelMenu.cpp
 * @brief Terminal::LookAndFeel implementation — popup menu drawing.
 *
 * @see LookAndFeel.h
 */

#include "LookAndFeel.h"

namespace Terminal
{ /*____________________________________________________________________________*/
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
                const auto* cfg { lua::Engine::getContext() };
                jam::BackgroundBlur::enable (safeComponent.getComponent(),
                                             cfg->display.window.blurRadius,
                                             cfg->display.window.colour.withAlpha (cfg->display.menu.opacity));
            }
        });
#endif
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

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Terminal
