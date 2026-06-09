#include "end/Tabs.h"

namespace end
{
/*____________________________________________________________________________*/

Tabs::Tabs (jam::Model& m)
    : jam::Model::Component { m, IDtype::tabs }
{
    setTabBarDepth (0);
}

void Tabs::addNewTab()
{
    jam::UUID uuid;
    auto* panes { new Panes (uuid, model) };

    addTab (juce::String (getNumTabs() + 1), juce::Colours::transparentBlack, panes, true);

    setCurrentTabIndex (getNumTabs() - 1);

    attachments.add (std::make_unique<jam::Model::Attachment> (*panes));

    updateTabBarVisibility();
}

void Tabs::removeCurrentTab()
{
    if (getNumTabs() > 1)
    {
        auto index { getCurrentTabIndex() };

        attachments.remove (index);
        removeTab (index);
        setCurrentTabIndex (juce::jmin (index, getNumTabs() - 1));

        updateTabBarVisibility();
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

void Tabs::currentTabChanged (int newCurrentTabIndex, const juce::String& newCurrentTabName) {}

void Tabs::updateTabBarVisibility()
{
    if (getNumTabs() <= 1)
    {
        setTabBarDepth (0);
    }
    else
    {
        config::Model& config { *config::Model::getContext() };
        auto display { config.getChildWithName (IDtype::display) };
        auto tabNode { display.getChildWithName (IDtype::tab) };
        auto family { tabNode.getProperty (ID::family, "Display Mono").toString() };
        auto points { static_cast<float> (tabNode.getProperty (ID::size, 12)) };
        juce::Font font { juce::FontOptions().withName (family).withPointHeight (points) };

        setTabBarDepth (juce::roundToInt (font.getHeight() / tabFontRatio));
    }
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace end
