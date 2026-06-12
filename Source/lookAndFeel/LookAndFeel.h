#pragma once
#include <JuceHeader.h>
#include <JamFontsBinaryData.h>
#include "Identifier.h"
#include "../config/Config.h"

namespace end
{
/*____________________________________________________________________________*/

class LookAndFeel
    : public jam::LookAndFeel::Methods<LookAndFeel>
    , public juce::ValueTree::Listener
{
public:
    /**
     * @brief END-private colour identifiers.
     *
     * Keyed by LookAndFeel::setColour / findColour for components that have no
     * JUCE-native colour id. Components sourcing JUCE-native ids (CaretComponent,
     * TextEditor, ScrollBar, ResizableWindow, juce::TextButton, juce::Label) use
     * those directly — this enum covers END-only components only.
     */
    enum ColourIds
    {
        cursorColourId = 0x2000001,
        paneBarColourId = 0x2000009,
        paneBarHighlightColourId = 0x200000A,
        statusBarBackgroundColourId = 0x2000100,
        statusBarLabelBackgroundColourId = 0x2000101,
        statusBarLabelTextColourId = 0x2000102,
        statusBarSpinnerColourId = 0x2000103,
        hintLabelBgColourId = 0x2000200,
        hintLabelFgColourId = 0x2000201,
        selectionCursorColourId = 0x2000301,
    };

    LookAndFeel();
    ~LookAndFeel();

    void
    valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property) override;

    void drawBarBackground (juce::Graphics&, juce::Component& bar) override;
    void drawBarIndicator (juce::Graphics&, juce::Component& indicator) override;
    void drawTabButton (juce::Graphics&,
                        juce::Button& button,
                        bool isMouseOver,
                        bool isMouseDown) override;

    juce::Font getTabFont() const;
    juce::Font getTabFont (int barDepth) const override;

    void drawStretchableLayoutResizerBar (juce::Graphics&,
                                          int w,
                                          int h,
                                          bool isVertical,
                                          bool isMouseOver,
                                          bool isMouseDown) override;

private:
    config::Model& config { *config::Model::getInstance() };

    /** @brief Parsed SVG assets keyed by their GRAPHICS property id.
     *
     *  Each entry is a SVG::Flex::Segments set (9-slice layout, paint-ready)
     *  produced by SVG::Flex::getSegments from the SVG file named by the
     *  matching property in the IDtype::graphics ValueTree node. Rebuilt by
     *  loadGraphics() on construction and on every SVG file-change event. Key
     *  is the GRAPHICS property identifier (e.g. ID::tabBar for the tab bar
     *  background asset).
     */
    jam::HashMap<juce::Identifier, jam::SVG::Flex::Segments> graphics;

    void loadGraphics();
    void initialiseColours();
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LookAndFeel)
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace end
