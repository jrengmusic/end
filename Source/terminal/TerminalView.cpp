#include "terminal/TerminalView.h"

TerminalView::TerminalView (jam::Model& model, juce::ValueTree tabState, jam::UUID uuid)
    : jam::PaneComponent (model, tabState, IDtype::pane, uuid)
{
    setWantsKeyboardFocus (true);
    toFront (true);
}

void TerminalView::paint (juce::Graphics& g)
{
    g.fillAll (findColour (jam::CodeView::backgroundColourId));

    g.setColour (findColour (jam::CodeView::textColourId));
    g.drawText (getComponentID(), getLocalBounds(), juce::Justification::centred);

    //==============================================================================
    jam::PaneComponent::paint (g);
}
