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

Tabs::~Tabs() {}

void Tabs::addNewTab()
{
    jam::UUID uuid;
    auto* panes { new Panes (uuid, model) };

    static constexpr const char* tabNames[] {
        "Home", "Settings", "Terminal", "Dev", "Logs & Output", "DB", "Configuration Panel"
    };
    static constexpr int numNames { 7 };

    const auto tabName { tabNames[getNumTabs() % numNames] };

    addTab (tabName, juce::Colours::transparentBlack, panes, true);

    setCurrentTabIndex (getNumTabs() - 1);

    attachments.add (std::make_unique<jam::Model::Attachment> (*panes));

    panes->createPane (uuid);

    // State is parented in model tree after Attachment — VTPC now fires on setProperty.
    panes->state.setProperty (jam::ID::name, juce::String { tabName }, nullptr);

    // Wire label value to model state — must happen AFTER Attachment grafts the state.

    if (auto* tab { getBar().getTabButton (getNumTabs() - 1) })
    {
        auto nameValue { panes->state.getPropertyAsValue (jam::ID::name, nullptr) };
        tab->label.getTextValue().referTo (nameValue);
    }

    lookAndFeelChanged();
}

void Tabs::removeCurrentTab()
{
    if (getNumTabs() > 1)
    {
        auto index { getCurrentTabIndex() };

        attachments.remove (index);
        removeTab (index);
        setCurrentTabIndex (juce::jmin (index, getNumTabs() - 1));
        lookAndFeelChanged();
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

void Tabs::valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property)
{
    juce::ignoreUnused (property);

    if (tree.getType() == IDtype::tab)
    {
        lookAndFeelChanged();
        resized();
    }
}

void Tabs::lookAndFeelChanged()
{
    auto& laf { static_cast<end::LookAndFeel&> (getLookAndFeel()) };
    setTabBarDepth (laf.getTabBarDepth (*this));
    setOrientation (laf.getTabPosition());
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace end
