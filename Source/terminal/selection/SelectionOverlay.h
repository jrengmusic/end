/**
 * @file SelectionOverlay.h
 * @brief Full-width status bar that shows the active vim-style selection mode.
 *
 * SelectionOverlay spans the entire MainComponent width and is positioned at
 * either the top or bottom edge, controlled by `keys.selection_bar_position`
 * in Config.  It is non-interactive and passes all mouse events through.
 *
 * ### Layout
 * The bar is divided into two regions:
 * - **Mode label** — a text-width rectangle on the left filled with
 *   `labelBackgroundColourId`, showing "-- VISUAL --" etc.
 * - **Bar background** — the remainder of the width filled with
 *   `barBackgroundColourId`.
 *
 * ### Visibility
 * Hidden when the selection type is `SelectionType::none`.  Shown immediately
 * (no fade) when an active type is set.  When hidden, no space is reserved in
 * the layout; MainComponent::resized() adjusts bounds accordingly.
 *
 * ### Colours
 * Three custom ColourIds are registered in `Terminal::LookAndFeel` and applied
 * via `setColour()` in `LookAndFeel::setColours()`.  The overlay reads them
 * via `findColour()` so they participate in JUCE's colour inheritance system.
 *
 * @note All methods are called on the **MESSAGE THREAD**.
 *
 * @see MainComponent
 * @see Terminal::LookAndFeel
 * @see Terminal::SelectionType
 */

#pragma once
#include <JuceHeader.h>
#include "SelectionType.h"
#include "../../config/Config.h"

namespace Terminal
{ /*____________________________________________________________________________*/

/**
 * @class SelectionOverlay
 * @brief Full-width status bar for vim-style selection mode indicator.
 *
 * Header-only.  Inherits `juce::Component`.  Non-interactive.
 *
 * @par Thread context
 * **MESSAGE THREAD** — all public methods.
 *
 * @see MainComponent
 */
class SelectionOverlay : public juce::Component
{
public:
    /**
     * @brief Custom colour IDs for the selection status bar.
     *
     * Registered in `Terminal::LookAndFeel` alongside other terminal colour IDs.
     * Set via `LookAndFeel::setColours()` from Config; read here via `findColour()`.
     */
    enum ColourIds
    {
        barBackgroundColourId = 0x2000100,///< Full bar background colour.
        labelBackgroundColourId = 0x2000101,///< Mode label rectangle background colour.
        labelTextColourId = 0x2000102///< Mode label text colour.
    };

    /**
     * @brief Constructs SelectionOverlay: opaque bar, no mouse interception.
     *
     * Starts hidden via `addChildComponent` in MainComponent.
     *
     * @note MESSAGE THREAD.
     */
    SelectionOverlay()
    {
        setOpaque (true);
        setInterceptsMouseClicks (false, false);
    }

    /**
     * @brief Updates the displayed selection type and toggles visibility.
     *
     * When @p type is `SelectionType::none`, hides the overlay.
     * Otherwise sets the label text and shows the overlay.
     * Triggers a layout recalculation in MainComponent via a repaint
     * followed by the caller re-invoking resized().
     *
     * @param type  The currently active selection type.
     * @note MESSAGE THREAD.
     */
    void update (SelectionType type) noexcept
    {
        if (type == SelectionType::none)
        {
            setVisible (false);
        }
        else
        {
            if (type == SelectionType::visual)
                label = "-- VISUAL --";
            else if (type == SelectionType::visualLine)
                label = "-- VISUAL LINE --";
            else
                label = "-- VISUAL BLOCK --";

            setVisible (true);
            repaint();
        }
    }

    /**
     * @brief Draws the full-width status bar with mode label on the left.
     *
     * Layout:
     * ```
     * |[-- VISUAL --]                                              |
     *  ^labelBg      ^barBg (fills rest of width)
     * ```
     *
     * 1. Fill entire bounds with `barBackgroundColourId`.
     * 2. Measure text width of the mode label.
     * 3. Fill a rectangle of (text width + 2 * labelPadding) on the left
     *    with `labelBackgroundColourId`.
     * 4. Draw the label text centred within that rectangle using `labelTextColourId`.
     *
     * @param g  JUCE graphics context.
     * @note MESSAGE THREAD.
     */
    /**
     * @brief Returns the preferred height of the status bar in logical pixels.
     *
     * Height is derived from the configured font size: font size + 8 px vertical
     * padding on each side (i.e. font size + 2 * verticalPadding).  When
     * `selection_bar.font_size` is 0 the terminal `font.size` is used instead.
     *
     * @return Height in logical pixels.
     * @note MESSAGE THREAD.
     */
    int getPreferredHeight() const noexcept
    {
        const auto font { juce::FontOptions {}
                              .withPointHeight (Config::getContext()->getFloat (Config::Key::fontSize)) };
        return juce::roundToInt (juce::Font (font).getHeight()) + 1 * verticalPadding;
    }

    void paint (juce::Graphics& g) override
    {
        const auto bounds { getLocalBounds() };
        const auto* cfg { Config::getContext() };
        const auto font { juce::FontOptions {}
                              .withName (cfg->getString (Config::Key::fontFamily))
                              .withPointHeight (cfg->getFloat (Config::Key::fontSize))
                              .withStyle ("Bold") };

        g.setColour (findColour (barBackgroundColourId));
        g.fillRect (bounds);

        g.setFont (font);

        const float textW { juce::TextLayout::getStringWidth (font, label) };
        const int labelW { juce::roundToInt (textW) + 2 * labelPadding };
        const auto labelRect { bounds.withWidth (labelW) };

        g.setColour (findColour (labelBackgroundColourId));
        g.fillRect (labelRect);

        g.setColour (findColour (labelTextColourId));
        g.drawFittedText (label, labelRect, juce::Justification::centred, 1);
    }

private:
    /** @brief The text currently shown in the overlay. */
    juce::String label;

    /** @brief Horizontal padding on each side of the mode label text. */
    static constexpr int labelPadding { 8 };

    /** @brief Vertical padding above and below the label text, in logical pixels. */
    static constexpr int verticalPadding { 4 };

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SelectionOverlay)
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Terminal
