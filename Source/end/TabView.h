#pragma once
#include <JuceHeader.h>
#include "terminal/TerminalView.h"
#include "Identifier.h"

class TabView : public jam::MatrixComponent
{
public:
    TabView (jam::UUID uuid, jam::Model& model, juce::ValueTree sessionState);
    ~TabView() override = default;

    jam::UUID add();

    void focusPane (const juce::Identifier& direction);

    jam::UUID join (const juce::Identifier& direction);

    void swap (const juce::Identifier& direction);

    TerminalView& get (jam::UUID uuid);

    void mouseDown (const juce::MouseEvent& event) override;
    void mouseDrag (const juce::MouseEvent& event) override;
    void mouseUp (const juce::MouseEvent& event) override;
    void paintOverChildren (juce::Graphics& g) override;

    static constexpr int splitDragThreshold { 8 };

protected:
    std::unique_ptr<jam::OwnedComponent> createChild (jam::UUID uuid) override;

    void childRemoved (jam::UUID uuid) override;

private:
    juce::PopupMenu buildAreaOptionsMenu();
    void handleAreaOptionsResult (int result);

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TabView)
};
