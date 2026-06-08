#pragma once
#include <JuceHeader.h>
#include "Identifier.h"
#include "../config/Config.h"

namespace end
{
/*____________________________________________________________________________*/

/** @brief END's LookAndFeel — ValueTree::Listener on config tree.

    valueTreePropertyChanged IS the init and reload path. Main registers
    it on config::Model. Every property change (during build, load, and
    hot-reload) triggers one setColour for the matching colourId.

    Two LAF virtuals paint the bar background and the pane resizer.
    Each virtual has a single responsibility — no combined or stateful
    paint methods. The Component (Bar) owns the layers; this class
    decides how each layer looks.
*/
class LookAndFeel
    : public jam::LookAndFeel::Methods<LookAndFeel>
    , public juce::ValueTree::Listener
{
public:
    /** @brief Colour IDs for END-specific UI elements. Named after the
        role of the colour, not the visual shape. */
    enum ColourIds
    {
        cursorColourId = 0x2000001,
        barBackgroundColourId = 0x2000002,// bar strip behind buttons
        frontBackgroundColourId = 0x2000003,// active tab fill
        inactiveBackgroundColourId = 0x2000004,// inactive tab fill
        frontTextColourId = 0x2000005,// active tab text
        inactiveTextColourId = 0x2000006,// inactive tab text
        tabOutlineColourId = 0x2000007,// 3-side border + content-edge line
        indicatorColourId = 0x2000008,// sliding highlight
        paneBarColourId = 0x2000009,
        paneBarHighlightColourId = 0x200000A,
        statusBarBackgroundColourId = 0x2000100,
        statusBarLabelBackgroundColourId = 0x2000101,
        statusBarLabelTextColourId = 0x2000102,
        statusBarSpinnerColourId = 0x2000103,
        hintLabelBgColourId = 0x2000200,
        hintLabelFgColourId = 0x2000201,
        selectionColourId = 0x2000300,
        selectionCursorColourId = 0x2000301,
    };

    /** @brief Config property → colourId map. One entry per lua key.
        Walks the ValueTree recursively; every node with a matching property
        key gets the corresponding colour set. */
    static const std::unordered_map<juce::Identifier, int> colourIds;

    LookAndFeel();
    ~LookAndFeel();

    /** @brief ValueTree::Listener — fires on every config property change.
        Re-applies the full colour set from the current tree state. */
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;

    //==============================================================================
    // Bar LAF virtuals — one per visual layer, single responsibility each.

    /** @brief Paints the bar's full background area. */
    void drawBarBackground (juce::Graphics&, juce::Component& bar) override;

    /** @brief Draws the resizer bar between panes. */
    void drawStretchableLayoutResizerBar (juce::Graphics&,
                                          int w,
                                          int h,
                                          bool isVertical,
                                          bool isMouseOver,
                                          bool isMouseDown) override;

private:
    juce::ValueTree config { config::Model::get() };

    //==============================================================================
    /** @brief One paint step: a dest rect, a colour, a path, and a stroke flag.
     *  An empty path means "fill/stroke the dest rect only" (no geometry).
     *  Built by the SVG parser, consumed by drawBarBackground. */
    struct Segment
    {
        juce::Rectangle<float> sourceBounds;
        juce::Rectangle<float> dest;
        juce::Colour colour;
        juce::Path path;
        bool stroke { false };
    };

    std::vector<Segment> barSegments;

    //==============================================================================
    /** @brief Applies the full colour set from config. Called by the ctor
        and by the ValueTree::Listener path. */
    void setColours();

    /** @brief Single-walk SVG → segments. Walks root-level \<g\> regions,
     *  finds role groups, resolves colour, extracts geometry. Corners (no
     *  bounds rect) sorted last; flex (has bounds rect) sorted first. */
    void parseTabBarSvg (const juce::XmlElement& svg, std::vector<Segment>& segments);

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LookAndFeel)
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace end
