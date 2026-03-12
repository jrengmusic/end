#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace jreng
{

class PaneManager;

//==============================================================================
/**
    A draggable bar for resizing adjacent panes.

    Follows the juce::StretchableLayoutResizerBar pattern. Holds a reference
    to the PaneManager and the split node it controls. On drag, it queries the
    manager for the current pixel position, computes the desired position, and
    tells the manager to update the ratio.

    @tags{GUI}
*/
class PaneResizerBar : public juce::Component
{
public:
    //==============================================================================
    PaneResizerBar (PaneManager* layout, juce::ValueTree splitNode, bool isVertical);
    ~PaneResizerBar() override;

    //==============================================================================
    /** This abstract base class is implemented by LookAndFeel classes. */
    struct JUCE_API  LookAndFeelMethods
    {
        virtual ~LookAndFeelMethods() = default;

        virtual void drawStretchableLayoutResizerBar (juce::Graphics&, int w, int h,
                                                      bool isVerticalBar, bool isMouseOver, bool isMouseDragging) = 0;
    };

    //==============================================================================
    const juce::ValueTree& getSplitNode() const noexcept;

    //==============================================================================
    void hasBeenMoved();

    //==============================================================================
    /** @internal */
    void paint (juce::Graphics&) override;
    /** @internal */
    void mouseDown (const juce::MouseEvent&) override;
    /** @internal */
    void mouseDrag (const juce::MouseEvent&) override;

private:
    //==============================================================================
    PaneManager* layout;
    juce::ValueTree splitNode;
    bool isVertical;
    int mouseDownPos { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PaneResizerBar)
};

} // namespace jreng
