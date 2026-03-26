/**
 * @file StatusBarOverlay.h
 * @brief Full-width status bar that reflects the active terminal modal state.
 *
 * StatusBarOverlay spans the entire MainComponent width and is positioned at
 * either the top or bottom edge, controlled by `keys.status_bar_position`
 * in Config.  It is non-interactive and passes all mouse events through.
 *
 * ### Layout
 * The bar is divided into two regions:
 * - **Mode label** — a text-width rectangle on the left filled with
 *   `labelBackgroundColourId`, showing "VISUAL", "OPEN", etc.
 * - **Bar background** — the remainder of the width filled with
 *   `barBackgroundColourId`.
 *
 * ### Visibility
 * Hidden when `ModalType::none`.  Shown immediately (no fade) when any
 * active modal type is set.  When hidden, no space is reserved in the
 * layout; MainComponent::resized() adjusts bounds accordingly.
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
 * @see Terminal::ModalType
 * @see Terminal::SelectionType
 */

#pragma once
#include <JuceHeader.h>
#include "SelectionType.h"
#include "../../config/Config.h"
#include "../data/State.h"

namespace Terminal
{ /*____________________________________________________________________________*/

/**
 * @class StatusBarOverlay
 * @brief Full-width status bar for terminal modal state indicators.
 *
 * Header-only.  Inherits `juce::Component`.  Non-interactive.
 * Displays a mode label for each active `ModalType`: selection modes
 * ("VISUAL" etc.) and file-open mode ("OPEN").
 *
 * @par Thread context
 * **MESSAGE THREAD** — all public methods.
 *
 * @see MainComponent
 */
class StatusBarOverlay : public juce::Component
{
public:
    /**
     * @brief Custom colour IDs for the status bar overlay.
     *
     * Registered in `Terminal::LookAndFeel` alongside other terminal colour IDs.
     * Set via `LookAndFeel::setColours()` from Config; read here via `findColour()`.
     */
    enum ColourIds
    {
        barBackgroundColourId = 0x2000100,  ///< Full bar background colour.
        labelBackgroundColourId = 0x2000101,///< Mode label rectangle background colour.
        labelTextColourId = 0x2000102       ///< Mode label text colour.
    };

    /**
     * @brief Constructs StatusBarOverlay: opaque bar, no mouse interception.
     *
     * Starts hidden via `addChildComponent` in MainComponent.
     *
     * @note MESSAGE THREAD.
     */
    StatusBarOverlay()
    {
        setOpaque (true);
        setInterceptsMouseClicks (false, false);
    }

    /**
     * @brief Updates the displayed modal state and toggles visibility.
     *
     * When @p modalType is `ModalType::none`, hides the overlay.
     * For `ModalType::selection`, shows a vim-style mode label derived from
     * @p selectionType.  For `ModalType::openFile`, shows "OPEN N/T" where
     * N is the 1-based current page and T is the total page count.
     * Hidden when the selection type within a selection modal is unrecognised.
     *
     * @param modalType     The currently active modal type.
     * @param selectionType Integer cast of the current SelectionType (used
     *                      when @p modalType is `ModalType::selection`).
     * @param hintPage      Current hint page index (0-based).
     * @param hintTotalPages Total number of hint pages.
     * @note MESSAGE THREAD.
     */
    void update (ModalType modalType, int selectionType,
                 int hintPage, int hintTotalPages) noexcept
    {
        if (modalType == ModalType::selection)
        {
            if (selectionType == static_cast<int> (SelectionType::visual))
            {
                label = "VISUAL";
                setVisible (true);
                repaint();
            }
            else if (selectionType == static_cast<int> (SelectionType::visualLine))
            {
                label = "V-LINE";
                setVisible (true);
                repaint();
            }
            else if (selectionType == static_cast<int> (SelectionType::visualBlock))
            {
                label = "V-BLOCK";
                setVisible (true);
                repaint();
            }
            else
            {
                setVisible (false);
            }
        }
        else if (modalType == ModalType::openFile)
        {
            if (hintTotalPages > 1)
            {
                label = "OPEN " + juce::String (hintPage + 1) + "/" + juce::String (hintTotalPages);
            }
            else
            {
                label = "OPEN";
            }

            setVisible (true);
            repaint();
        }
        else
        {
            setVisible (false);
        }
    }

    /**
     * @brief Returns the preferred height of the status bar in logical pixels.
     *
     * Height is derived from the configured font size: font size + vertical
     * padding.  When `font.size` is 0 the terminal default is used instead.
     *
     * @return Height in logical pixels.
     * @note MESSAGE THREAD.
     */
    int getPreferredHeight() const noexcept
    {
        const float fontSize { Config::getContext()->getFloat (Config::Key::fontSize) };
        return juce::roundToInt (fontSize) + 2 * verticalPadding;
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
    void paint (juce::Graphics& g) override
    {
        const auto bounds { getLocalBounds() };
        const auto* cfg { Config::getContext() };
        const juce::Font font { juce::FontOptions {}
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
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StatusBarOverlay)
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Terminal
