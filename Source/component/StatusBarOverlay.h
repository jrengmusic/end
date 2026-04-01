/**
 * @file StatusBarOverlay.h
 * @brief Full-width status bar that reflects the active pane modal state.
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
 * @see ModalType
 * @see SelectionType
 */

#pragma once
#include <JuceHeader.h>
#include "../SelectionType.h"
#include "../ModalType.h"
#include "../config/Config.h"
#include "../AppIdentifier.h"

/**
 * @class StatusBarOverlay
 * @brief Full-width status bar for pane modal state indicators.
 *
 * Header-only.  Inherits `juce::Component` and `juce::ValueTree::Listener`.
 * Non-interactive.  Displays a mode label for each active `ModalType`:
 * selection modes ("VISUAL" etc.) and file-open mode ("OPEN").
 *
 * Listens to the TABS subtree for `modalType` and `selectionType` property
 * changes and self-updates via `refresh()`.  Hint page info (Terminal-specific,
 * pane-local) is updated externally via `updateHintInfo()`.
 *
 * @par Thread context
 * **MESSAGE THREAD** — all public methods.
 *
 * @see MainComponent
 */
class StatusBarOverlay : public juce::Component,
                         public juce::ValueTree::Listener
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
        barBackgroundColourId   = 0x2000100, ///< Full bar background colour.
        labelBackgroundColourId = 0x2000101, ///< Mode label rectangle background colour.
        labelTextColourId       = 0x2000102  ///< Mode label text colour.
    };

    /**
     * @brief Constructs StatusBarOverlay and registers as listener on the TABS subtree.
     *
     * Starts hidden via `addChildComponent` in MainComponent.
     *
     * @param tabsTree  The TABS subtree from AppState; listened to for modal property changes.
     * @note MESSAGE THREAD.
     */
    StatusBarOverlay (juce::ValueTree tabsTree)
        : tabs (tabsTree)
    {
        setOpaque (true);
        setInterceptsMouseClicks (false, false);
        tabs.addListener (this);
    }

    ~StatusBarOverlay() override
    {
        tabs.removeListener (this);
    }

    /**
     * @brief ValueTree listener callback; triggers refresh on modal property changes.
     *
     * @param tree      The tree whose property changed (unused).
     * @param property  The identifier of the changed property.
     * @note MESSAGE THREAD.
     */
    void valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property) override
    {
        juce::ignoreUnused (tree);

        if (property == App::ID::modalType or property == App::ID::selectionType)
            refresh();
    }

    /**
     * @brief Reads modal state from the TABS ValueTree and updates the overlay.
     *
     * Called by valueTreePropertyChanged when modalType or selectionType change.
     * Also called externally for hint page updates (Terminal-specific, polled).
     *
     * @note MESSAGE THREAD.
     */
    void refresh() noexcept
    {
        const auto modal { static_cast<ModalType> (static_cast<int> (tabs.getProperty (App::ID::modalType))) };
        const auto selType { static_cast<int> (tabs.getProperty (App::ID::selectionType)) };

        if (modal == ModalType::selection)
        {
            if (selType == static_cast<int> (SelectionType::visual))
            {
                label = "VISUAL";
                setVisible (true);
                repaint();
            }
            else if (selType == static_cast<int> (SelectionType::visualLine))
            {
                label = "V-LINE";
                setVisible (true);
                repaint();
            }
            else if (selType == static_cast<int> (SelectionType::visualBlock))
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
        else if (modal == ModalType::openFile)
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
     * @brief Updates hint page info for open-file mode display.
     *
     * Called from MainComponent's VBlank lambda when the active pane is a terminal
     * in openFile mode. Triggers refresh if values changed.
     *
     * @param page   Zero-based hint page index.
     * @param total  Total hint page count.
     * @note MESSAGE THREAD.
     */
    void updateHintInfo (int page, int total) noexcept
    {
        if (hintPage != page or hintTotalPages != total)
        {
            hintPage = page;
            hintTotalPages = total;
            refresh();
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
        const float fontSize { Config::getContext()->getFloat (Config::Key::statusBarFontSize) };
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
                                    .withName (cfg->getString (Config::Key::statusBarFontFamily))
                                    .withPointHeight (cfg->getFloat (Config::Key::statusBarFontSize))
                                    .withStyle (cfg->getString (Config::Key::statusBarFontStyle)) };

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
    /** @brief The TABS subtree; listened to for modalType and selectionType changes. */
    juce::ValueTree tabs;

    /** @brief The text currently shown in the overlay. */
    juce::String label;

    /** @brief Zero-based current hint page index (openFile mode). */
    int hintPage { 0 };

    /** @brief Total hint page count (openFile mode). */
    int hintTotalPages { 0 };

    /** @brief Horizontal padding on each side of the mode label text. */
    static constexpr int labelPadding { 8 };

    /** @brief Vertical padding above and below the label text, in logical pixels. */
    static constexpr int verticalPadding { 4 };

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StatusBarOverlay)
};
