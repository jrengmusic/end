#pragma once
#include <JuceHeader.h>

struct TerminalView : public juce::Component
{
    explicit TerminalView (jam::UUID uuid);

    void paint (juce::Graphics& g) override;

    const jam::UUID uuid;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TerminalView)
};
