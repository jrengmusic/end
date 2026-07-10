#pragma once
#include <JuceHeader.h>

class SidebarComponent : public jam::PaneComponent
{
public:
    SidebarComponent (jam::Model& model, juce::ValueTree existingState);
    ~SidebarComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SidebarComponent)
};
