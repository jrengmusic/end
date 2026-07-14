#pragma once
#include <JuceHeader.h>
#include "generated/Lexicon.h"

class EditorView
    : public jam::PaneComponent
    , public juce::ValueTree::Listener
{
public:
    EditorView (jam::Model& model, juce::ValueTree tabState, jam::UUID uuid);
    ~EditorView() override;

    static constexpr float defaultZoom { 1.0f };
    static constexpr float zoomMin { 0.25f };
    static constexpr float zoomMax { 4.0f };

    void resized() override;
    void childBoundsChanged (juce::Component* child) override;

private:
    void valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property) override;

    void createProcessorEditor();

    std::unique_ptr<juce::AudioProcessorEditor> editor;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EditorView)
};
