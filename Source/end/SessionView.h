#pragma once
#include <JuceHeader.h>
#include "end/TabView.h"
#include "config/ConfigModel.h"
#include "lookAndFeel/ENDLookAndFeel.h"
#include "Identifier.h"

class SessionView
    : public jam::TabbedComponent
    , public jam::Model::Component
    , public juce::ValueTree::Listener
{
public:
    SessionView (jam::Model& model, juce::ValueTree sessionState);
    ~SessionView() override;

    TabView& add (jam::UUID uuid);

    void remove (jam::UUID uuid);

    TabView& get (jam::UUID uuid);

    TabView* getActiveTabView() noexcept;

private:
    void valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property) override;
    void lookAndFeelChanged() override;
    void currentTabChanged (int newCurrentTabIndex, const juce::String&) override;

    juce::String getTitle (const juce::ValueTree& tabState);

    static juce::ValueTree findAncestorTab (juce::ValueTree tree);

    void applyTabTitle (const juce::ValueTree& tabState);

    ENDLookAndFeel& lookAndFeel { *ENDLookAndFeel::getInstance() };

    jam::HashMap<jam::UUID, std::unique_ptr<jam::Model::Attachment>> attachments;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SessionView)
};
