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

    jam::UUID add (jam::UUID anchor, const juce::Identifier& edge);

    void remove (jam::UUID uuid);

    void focusPane (const juce::Identifier& direction);

    TerminalView& get (jam::UUID uuid);

    TerminalView& get();

private:
    TerminalView* findFocusedPane() const;

    TerminalView* findNearestPane (const juce::Identifier& direction, TerminalView* focused) const;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TabView)
};
