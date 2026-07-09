#include "terminal/TerminalView.h"

TerminalView::TerminalView (jam::UUID uuid)
    : uuid (uuid)
{
}

void TerminalView::paint (juce::Graphics& g)
{
    g.fillAll (findColour (jam::CodeView::backgroundColourId));

    g.setColour (findColour (jam::CodeView::textColourId));
    g.drawText (uuid.toString(), getLocalBounds(), juce::Justification::centred);
}
