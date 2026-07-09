#pragma once
#include <JuceHeader.h>
#include "terminal/TerminalView.h"
#include "Identifier.h"

class TabView
    : public juce::Component
    , public jam::Model::Component
{
public:
    TabView (jam::UUID uuid, jam::Model& model, juce::ValueTree tabsState);
    ~TabView() override;

    jam::UUID add();

    jam::UUID add (jam::UUID anchor, const juce::Identifier& edge);

    void resized() override;
    void visibilityChanged() override;

    void remove (jam::UUID uuid);

    void focusPane (const juce::Identifier& direction);

    int getPaneCount() const noexcept;

    TerminalView& get (jam::UUID uuid);

    TerminalView& get();

private:
    jam::PaneManager paneManager;
    jam::HashMap<jam::UUID, std::unique_ptr<TerminalView>> panes;

    jam::Function::Map<juce::Identifier, std::pair<bool, int>> events;

    void registerEvents();

    TerminalView* findFocusedPane() const;

    TerminalView* findNearestPane (const juce::Identifier& direction, TerminalView* focused) const;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TabView)
};
