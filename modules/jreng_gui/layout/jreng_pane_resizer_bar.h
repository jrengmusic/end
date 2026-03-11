#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "jreng_pane_manager.h"

namespace jreng
{

//==============================================================================
/**
    A component that acts as one of the vertical or horizontal bars you see being
    used to resize panels in a window.

    One of these acts with a PaneManager to resize the other components.

    @see PaneManager

    @tags{GUI}
*/
class PaneResizerBar : public juce::Component
{
public:
    //==============================================================================
    /** Creates a resizer bar for use on a specified layout.

        @param layoutToUse          the layout that will be affected when this bar
                                    is dragged
        @param itemIndexInLayout    the item index in the layout that corresponds to
                                    this bar component. You'll need to set up the item
                                    properties in a suitable way for a divider bar, e.g.
                                    for an 8-pixel wide bar which, you could call
                                    myLayout->setItemLayout (barIndex, 8, 8, 8)
        @param isBarVertical        true if it's an upright bar that you drag left and
                                    right; false for a horizontal one that you drag up and
                                    down
    */
    PaneResizerBar (PaneManager* layoutToUse,
                    int itemIndexInLayout,
                    bool isBarVertical);

    /** Destructor. */
    ~PaneResizerBar() override;

    //==============================================================================
    /** This is called when the bar is dragged.

        This method must update the positions of any components whose position is
        determined by the PaneManager, because they might have just
        moved.

        The default implementation calls the resized() method of this component's
        parent component, because that's often where you're likely to apply the
        layout, but it can be overridden for more specific needs.
    */
    virtual void hasBeenMoved();

    //==============================================================================
    /** This abstract base class is implemented by LookAndFeel classes. */
    struct JUCE_API  LookAndFeelMethods
    {
        virtual ~LookAndFeelMethods() = default;

        virtual void drawStretchableLayoutResizerBar (juce::Graphics&, int w, int h,
                                                        bool isVerticalBar, bool isMouseOver, bool isMouseDragging) = 0;
    };

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
    int itemIndex, mouseDownPos;
    bool isVertical;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PaneResizerBar)
};

} // namespace jreng
