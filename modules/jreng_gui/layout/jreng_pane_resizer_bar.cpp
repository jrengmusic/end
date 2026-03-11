#include "jreng_pane_resizer_bar.h"

namespace jreng
{

PaneResizerBar::PaneResizerBar (PaneManager* layout_,
                                const int index,
                                const bool vertical)
    : layout (layout_),
      itemIndex (index),
      isVertical (vertical)
{
    setRepaintsOnMouseActivity (true);
    setMouseCursor (vertical ? juce::MouseCursor::LeftRightResizeCursor
                             : juce::MouseCursor::UpDownResizeCursor);
}

PaneResizerBar::~PaneResizerBar()
{
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
    mouseDownPos = layout->getItemCurrentPosition (itemIndex);
}

void PaneResizerBar::mouseDrag (const juce::MouseEvent& e)
{
    const int desiredPos = mouseDownPos + (isVertical ? e.getDistanceFromDragStartX()
                                                      : e.getDistanceFromDragStartY());


    if (layout->getItemCurrentPosition (itemIndex) != desiredPos)
    {
        layout->setItemPosition (itemIndex, desiredPos);
        hasBeenMoved();
    }
}

void PaneResizerBar::hasBeenMoved()
{
    auto* parent = getParentComponent();
    if (parent != nullptr)
        parent->resized();
}

} // namespace jreng
