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
        cursorColourId                   = 0x2000001,
        paneBarColourId                  = 0x2000009,
        paneBarHighlightColourId         = 0x200000A,
        statusBarBackgroundColourId      = 0x2000100,
        statusBarLabelBackgroundColourId = 0x2000101,
        statusBarLabelTextColourId       = 0x2000102,
        statusBarSpinnerColourId         = 0x2000103,
        hintLabelBgColourId              = 0x2000200,
        hintLabelFgColourId              = 0x2000201,
        selectionCursorColourId          = 0x2000301,
    };

    /**
     * @brief Scoped colour registry: node type → property → list of target colourIds.
     *
     * Outer key matches the ValueTree type (IDtype::tab, IDtype::colours, etc.).
     * Inner key matches the lua property name (an END or jam Identifier).
     * Value is the list of JUCE-native or END-private colourIds to set from that
     * property — one lua value can feed several colourId targets.
     *
     * Rule: components follow JUCE-native colourId semantics. Tab buttons are
     * juce::TextButton instances — their text/fill use TextButton ids. Bar uses
     * only bar-level ids (background, outline, highlight). Lua keys are named to
     * match the colourId they drive, 1:1.
     */
    static const jam::HashMap<juce::Identifier, jam::HashMap<juce::Identifier, std::vector<int>>> colourIds;

    LookAndFeel();
    ~LookAndFeel();

    void valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property) override;

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

    void setColours();
    void loadGraphics();

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LookAndFeel)
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace end
