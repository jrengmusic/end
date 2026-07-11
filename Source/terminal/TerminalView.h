#pragma once
#include <JuceHeader.h>
#include "Identifier.h"

class TerminalView : public jam::PaneComponent
{
public:
    TerminalView (jam::Model& model, juce::ValueTree tabState, jam::UUID uuid);

    void paint (juce::Graphics& g) override;

private:
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TerminalView)
};
