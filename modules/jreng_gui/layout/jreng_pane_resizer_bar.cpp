#include "jreng_pane_resizer_bar.h"
#include "jreng_pane_manager.h"

namespace jreng
{ /*____________________________________________________________________________*/

PaneResizerBar::PaneResizerBar (PaneManager* layout_, juce::ValueTree splitNode_, bool isVertical_)
    : layout (layout_)
    , splitNode (splitNode_)
    , isVertical (isVertical_)
{
    setRepaintsOnMouseActivity (true);
    setMouseCursor (isVertical_ ? juce::MouseCursor::LeftRightResizeCursor
                                : juce::MouseCursor::UpDownResizeCursor);
}

PaneResizerBar::~PaneResizerBar() = default;

//==============================================================================
const juce::ValueTree& PaneResizerBar::getSplitNode() const noexcept
{
    return splitNode;
}

//==============================================================================
void PaneResizerBar::hasBeenMoved()
{
    if (auto* p { getParentComponent() }; p != nullptr)
        p->resized();
}

//==============================================================================
void PaneResizerBar::paint (juce::Graphics& g)
{
    getLookAndFeel().drawStretchableLayoutResizerBar (g,
                                                      getWidth(), getHeight(),
                                                      isVertical,
                                                      isMouseOver(),
                                                      isMouseButtonDown());
}

void PaneResizerBar::mouseDown (const juce::MouseEvent&)
{
    mouseDownPos = layout->getItemCurrentPosition (splitNode);
}

void PaneResizerBar::mouseDrag (const juce::MouseEvent& e)
{
    const int delta { isVertical ? e.getDistanceFromDragStartX()
                                 : e.getDistanceFromDragStartY() };
    const int desiredPos { mouseDownPos + delta };

    if (layout->getItemCurrentPosition (splitNode) != desiredPos)
    {
        layout->setItemPosition (splitNode, desiredPos);
        hasBeenMoved();
    }
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace jreng
