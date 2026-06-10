#pragma once
#include <JuceHeader.h>
#include "Identifier.h"
#include "../config/Config.h"

namespace end
{
/*____________________________________________________________________________*/

/** @brief END's LookAndFeel — ValueTree::Listener on config tree.

    valueTreePropertyChanged dispatches narrowly: SVG property changes
    reload all three Segments caches and repaint Desktop root components;
    colour property changes re-apply the full colour set. Construction
    calls setColours() and loadGraphics().

    SVG parsing and painting are fully delegated to jam::SVG::Flex. Three
    jam::SVG::Flex::Segments members (bar, indicator, button) hold the
    pre-parsed caches. Colours are selected from the active LAF palette at
    paint time; no colour data is stored in the caches.

    Three LAF virtuals paint the bar background, the sliding indicator,
    and individual tab buttons. Each virtual has a single responsibility.

    ### 1:1 colour chain (SSOT)

    colourIds is the single registry that drives both setColours() and
    jam::SVG::Flex::getSegments(). Keys are Identifiers that match lua
    config keys, display.lua SVG group names, and JUCE/jam component
    ColourIds — one chain, one source.  Tab bar colours (background,
    outline, indicator, inactiveBackground, foreground, inactiveText)
    map to JUCE TabbedComponent / TabbedButtonBar and jam::button ColourIds
    so that JUCE paint delegates resolve them without additional wiring.
*/
class LookAndFeel
    : public jam::LookAndFeel::Methods<LookAndFeel>
    , public juce::ValueTree::Listener
{
public:
    /** @brief Colour IDs for END-specific UI elements that have no
        corresponding JUCE or jam component ColourId. Named after the
        role of the colour, not the visual shape.

        Tab bar colours are omitted here — they map directly to JUCE
        TabbedComponent / TabbedButtonBar and jam::button ColourIds
        via colourIds and are registered there. */
    enum ColourIds
    {
        /** @brief Terminal text caret (cursor) fill colour. */
        cursorColourId = 0x2000001,
        /** @brief Pane divider bar fill colour (idle). */
        paneBarColourId = 0x2000009,
        /** @brief Pane divider bar fill colour (mouse-over or dragging). */
        paneBarHighlightColourId = 0x200000A,
        /** @brief Status bar full background fill colour. */
        statusBarBackgroundColourId = 0x2000100,
        /** @brief Status bar mode label background fill colour. */
        statusBarLabelBackgroundColourId = 0x2000101,
        /** @brief Status bar mode label text colour. */
        statusBarLabelTextColourId = 0x2000102,
        /** @brief Status bar activity spinner stroke colour. */
        statusBarSpinnerColourId = 0x2000103,
        /** @brief Open File mode hint label background fill colour. */
        hintLabelBgColourId = 0x2000200,
        /** @brief Open File mode hint label foreground (text) colour. */
        hintLabelFgColourId = 0x2000201,
        /** @brief Terminal selection highlight fill colour. */
        selectionColourId = 0x2000300,
        /** @brief Selection-mode caret (cursor) fill colour. */
        selectionCursorColourId = 0x2000301,
    };

    /** @brief Config property → colourId map. One entry per lua key.
        Walks the ValueTree recursively; every node with a matching property
        key gets the corresponding colour set.

        This registry is the SSOT for the 1:1 chain: lua key =
        Identifier key = SVG group name = component ColourId. */
    static const jam::HashMap<juce::Identifier, int> colourIds;

    /** @brief Construct: call setColours(), loadGraphics(), and register as
        a ValueTree::Listener on the config tree. */
    LookAndFeel();
    /** @brief Destruct: unregister the ValueTree::Listener. */
    ~LookAndFeel();

    /** @brief ValueTree::Listener — narrow dispatch on property identity.
        SVG properties (tabBar, tabActive, tabInactive) → loadGraphics()
        then repaint all Desktop root components. Colour properties →
        setColours(). Anything else is ignored. */
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;

    //==============================================================================
    // Bar LAF virtuals — one per visual layer, single responsibility each.

    /** @brief Paints the bar's full background area via jam::SVG::Flex::paint
        using the bar Segments cache. SVG group colours are resolved from
        the LAF at paint time via the colourIds registry. */
    void drawBarBackground (juce::Graphics&, juce::Component& bar) override;

    /** @brief Paints the sliding indicator via jam::SVG::Flex::paint using
        the indicator Segments cache. SVG group colours are resolved from
        the LAF at paint time via the colourIds registry. */
    void drawBarIndicator (juce::Graphics&, juce::Component& indicator) override;

    /** @brief Paints an individual tab button shape via jam::SVG::Flex::paint
        using the button Segments cache, then draws the button text centred.
        Text colour: juce::TabbedButtonBar::frontTextColourId when toggled,
        juce::TabbedButtonBar::tabTextColourId otherwise. Mouse state is ignored. */
    void drawTabButton (juce::Graphics&,
                        juce::Button& button,
                        bool isMouseOver,
                        bool isMouseDown) override;

    /** @brief Returns the tab font built from config display → tab → family
        and size properties. BinaryData defaults guarantee both properties
        exist; no fallback default is applied on getProperty. */
    juce::Font getTabFont() const;

    /**
     * @brief Draws the resizer bar between panes.
     *
     * Idle bar uses paneBarColourId; mouse-over or mouse-down uses
     * paneBarHighlightColourId. The full \p w × \p h area is filled.
     *
     * @param g            JUCE graphics context for this paint pass.
     * @param w            Bar width in pixels.
     * @param h            Bar height in pixels.
     * @param isVertical   Orientation flag from the base virtual
     *                     (currently unused — colour is always chosen
     *                     from mouse state, not orientation).
     * @param isMouseOver  Whether the cursor is over the bar.
     * @param isMouseDown  Whether the bar is being dragged.
     */
    void drawStretchableLayoutResizerBar (juce::Graphics&,
                                          int w,
                                          int h,
                                          bool isVertical,
                                          bool isMouseOver,
                                          bool isMouseDown) override;

private:
    /** @brief Reference to the application config tree; the LAF listens
        for property changes on this tree. */
    config::Model& config { *config::Model::getContext() };

    //==============================================================================
    /** @brief Applies the full colour set from config. Called by the ctor
        and by the ValueTree::Listener colour-property path. */
    void setColours();

    /** @brief Reads SVG files from disk and parses each into a Segments cache
        via jam::SVG::Flex::getSegments, using this LookAndFeel's colourIds
        registry as the LAF-group binding. Populates the graphics map keyed
        by ID::tabBar, ID::tabActive, ID::tabInactive. Called from the ctor
        and on SVG property changes. */
    void loadGraphics();

    //==============================================================================
    /** @brief Parsed Segments caches for all tab SVG graphics, keyed by
        config graphics Identifier (ID::tabBar, ID::tabActive, ID::tabInactive). */
    jam::HashMap<juce::Identifier, jam::SVG::Flex::Segments> flexGraphics;
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LookAndFeel)
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace end
