#include "end/Tabs.h"

namespace end
{
/*____________________________________________________________________________*/

Tabs::Tabs()
    : jam::ValueTree::Component { IDtype::tab }
{
}

void Tabs::addNewTab()
{
    auto uuid { juce::Uuid().toString() };
    auto panes { std::make_unique<Panes> (uuid) };
    addAndMakeVisible (*panes);
    tabPanes.add (std::move (panes));

    addTab (juce::String (getNumTabs() + 1), juce::Colours::transparentBlack);
    setCurrentTabIndex (getNumTabs() - 1);
}

void Tabs::removeCurrentTab()
{
    if (getNumTabs() > 1)
    {
        auto index { getCurrentTabIndex() };
        removeTab (index);
        tabPanes.remove (index);
    }
    else
    {
        juce::JUCEApplication::getInstance()->systemRequestedQuit();
    }
}

Panes* Tabs::getActivePanes() noexcept { return tabPanes.at (getCurrentTabIndex()).get(); }

void Tabs::resized()
{
    jam::TabbedComponent::resized();

    auto contentArea { getContentArea() };

    for (auto& panes : tabPanes)
    {
        if (panes->isVisible())
            panes->setBounds (contentArea);
    }
}

void Tabs::currentTabChanged (int newCurrentTabIndex, const juce::String& newCurrentTabName)
{
    juce::ignoreUnused (newCurrentTabName);
    state.setProperty (ID::activeTab, newCurrentTabIndex, nullptr);

    for (auto& panes : tabPanes)
        panes->setVisible (tabPanes.is (panes, newCurrentTabIndex));

    tabPanes.at (newCurrentTabIndex)->setBounds (getContentArea());
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace end
