#pragma once
#include <JuceHeader.h>
#include "Identifier.h"

class EditorView : public jam::PaneComponent
{
public:
    EditorView (jam::Model& model, juce::ValueTree tabState, jam::UUID uuid);

    static constexpr float defaultZoom { 1.0f };
    static constexpr float zoomMin { 0.25f };
    static constexpr float zoomMax { 4.0f };

private:
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EditorView)
};
