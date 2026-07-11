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

    jam::UUID add (const juce::Identifier& edge, float position = 0.5f);

    void remove (jam::UUID uuid);

    void focusPane (const juce::Identifier& direction);

    TerminalView& get (jam::UUID uuid);

    TerminalView& get();

    void mouseDown (const juce::MouseEvent& event) override;
    void mouseDrag (const juce::MouseEvent& event) override;
    void mouseUp (const juce::MouseEvent& event) override;
    void paintOverChildren (juce::Graphics& g) override;

    static constexpr int cornerZoneExtent { 24 };
    static constexpr int splitDragThreshold { 8 };

protected:
    std::unique_ptr<jam::OwnedComponent> createChild (jam::UUID uuid) override;

private:
    TerminalView* findFocusedPane() const;

    TerminalView* findNearestPane (const juce::Identifier& direction, TerminalView* focused) const;

    jam::UUID target { jam::UUID::none() };
    juce::Point<int> gestureStart;
    juce::Rectangle<int> preview;
    int splitLine { -1 };
    bool splitVertical { false };

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TabView)
};
