#pragma once
#include <JuceHeader.h>
#include "end/TabView.h"
#include "config/ConfigModel.h"
#include "lookAndFeel/ENDLookAndFeel.h"
#include "Identifier.h"

class SessionView
    : public jam::TabbedComponent
{
public:
    SessionView (jam::Model& model, juce::ValueTree sessionState);
    ~SessionView() override = default;

    TabView& add (jam::UUID uuid);

    void remove (jam::UUID uuid);

    TabView& get (jam::UUID uuid);

    TabView* getActiveTabView() noexcept;

private:
    ENDLookAndFeel& lookAndFeel { *ENDLookAndFeel::getInstance() };

    void
    valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property) override;
    void lookAndFeelChanged() override;

    juce::String getName (const juce::ValueTree& tabState);

    static juce::ValueTree findAncestorTab (juce::ValueTree tree);

    void setName (jam::UUID uuid);

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SessionView)
};
