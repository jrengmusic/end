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
    auto* panes { new Panes (uuid) };

    addTab (juce::String (getNumTabs() + 1),
            juce::Colours::transparentBlack,
            panes,
            true);

    setCurrentTabIndex (getNumTabs() - 1);
}

void Tabs::removeCurrentTab()
{
    if (getNumTabs() > 1)
    {
        auto index { getCurrentTabIndex() };
        removeTab (index);
        setCurrentTabIndex (juce::jmin (index, getNumTabs() - 1));
    }
    else
    {
        juce::JUCEApplication::getInstance()->systemRequestedQuit();
    }
}

Panes* Tabs::getActivePanes() noexcept
{
    return static_cast<Panes*> (getCurrentContentComponent());
}

void Tabs::currentTabChanged (int newCurrentTabIndex, const juce::String& newCurrentTabName)
{
    juce::ignoreUnused (newCurrentTabName);
    state.setProperty (ID::activeTab, newCurrentTabIndex, nullptr);
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace end
