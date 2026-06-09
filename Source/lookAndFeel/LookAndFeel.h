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
    static const jam::HashMap<juce::Identifier, int> colourIds;

    LookAndFeel();
    ~LookAndFeel();

    /** @brief ValueTree::Listener — fires on every config property change.
        Re-applies the full colour set from the current tree state. */
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;

    //==============================================================================
    // Bar LAF virtuals — one per visual layer, single responsibility each.

    /** @brief Paints the bar's full background area. */
    void drawBarBackground (juce::Graphics&, juce::Component& bar) override;

    /** @brief Paints the sliding indicator behind the active tab. */
    void drawBarIndicator (juce::Graphics&, juce::Component& indicator) override;

    /** @brief Paints an individual tab button shape. */
    void drawTabButton (juce::Graphics&, juce::Button& button,
                        bool isMouseOver, bool isMouseDown) override;

    /** @brief Draws the resizer bar between panes. */
    void drawStretchableLayoutResizerBar (juce::Graphics&,
                                          int w,
                                          int h,
                                          bool isVertical,
                                          bool isMouseOver,
                                          bool isMouseDown) override;

private:
    config::Model& config { *config::Model::getContext() };

    //==============================================================================
    /** @brief Applies the full colour set from config. Called by the ctor
        and by the ValueTree::Listener path. */
    void setColours();

    /** @brief Reads SVG file content from disk into string members.
        Called from ctor and from valueTreePropertyChanged on SVG property changes.
        No parsing — content is stored as raw strings for future use. */
    void loadGraphics();

    //==============================================================================
    /** @brief Raw SVG content for the tab bar background. Loaded from disk by loadGraphics(). */
    juce::String barSvg;
    /** @brief Raw SVG content for the active tab indicator. Loaded from disk by loadGraphics(). */
    juce::String indicatorSvg;
    /** @brief Raw SVG content for inactive tab buttons. Loaded from disk by loadGraphics(). */
    juce::String buttonSvg;
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LookAndFeel)
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace end
