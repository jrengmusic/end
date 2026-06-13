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
    void drawBarHighlight (juce::Graphics&, juce::Component& highlight) override;
    void drawTabButton (juce::Graphics&,
                        juce::Button& button,
                        bool isMouseOver,
                        bool isMouseDown) override;

    juce::Font getTabFont() const override;

    /** @brief Returns horizontal padding in pixels per side of the tab label.
     *  Reads tab.padding from the display config (user-configurable).
     *  @param font  Tab font (ignored — padding is read from config, not derived
     *               from font metrics).
     */
    int getTabPadding (const juce::Font& font) const override;

    /** @brief Display transform applied to a tab label before measuring and painting.
     *  Reads tab.uppercase from the display config; returns toUpperCase() when set,
     *  identity otherwise. Consumed by both Bar::getBestTabLength (measurement) and
     *  drawTabButton (render) so measured width and painted string never diverge.
     */
    juce::String getTabText (const juce::String& tabName) const override;

    void drawStretchableLayoutResizerBar (juce::Graphics&,
                                          int w,
                                          int h,
                                          bool isVertical,
                                          bool isMouseOver,
                                          bool isMouseDown) override;

private:
    config::Model& config { *config::Model::getInstance() };

    /** @brief Parsed SVG assets keyed by their GRAPHICS property id or button
     *  state identifier.
     *
     *  Each entry is a SVG::Flex::Segments set (9-slice layout, paint-ready)
     *  produced by SVG::Flex::getSegments from the SVG file named by the
     *  matching property in the IDtype::graphics ValueTree node. Rebuilt by
     *  loadGraphics() on construction and on every SVG file-change event.
     *
     *  Keys are either:
     *  - GRAPHICS property identifiers (ID::tabBar, ID::tabHighlight) for the
     *    bar background and sliding highlight assets, or
     *  - tab-button state identifiers (jam::ID::normal, jam::ID::over, …) for
     *    the per-state button Segments, one entry per authored state slot.
     */
    jam::HashMap<juce::Identifier, jam::SVG::Flex::Segments> graphics;

    void loadGraphics();
    void initialiseColours();

    // TODO: SHOULD BE CONFIG DRIVEN
    static inline const juce::Typeface::Ptr displayBold { juce::Typeface::createSystemTypefaceFor (
        jam::fonts::DisplayMedium_ttf,
        jam::fonts::DisplayMedium_ttfSize) };
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LookAndFeel)
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace end
