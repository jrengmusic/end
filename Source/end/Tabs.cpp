#include "end/Tabs.h"
#include "lookAndFeel/LookAndFeel.h"

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
        auto& laf { static_cast<end::LookAndFeel&> (getLookAndFeel()) };
        setTabBarDepth (juce::roundToInt (laf.getTabFont().getHeight() / tabFontRatio));
    }
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace end
