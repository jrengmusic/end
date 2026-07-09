#include "end/SessionView.h"

SessionView::SessionView (jam::Model& m, juce::ValueTree sessionState)
    : jam::Model::Component { *this, m, sessionState }
{
    model.createAndAddParameter<jam::Parameter<int64_t>> (state, ID::focusedTab, int64_t { 0 });

    setTabBarDepth (0);
    state.addListener (this);
}

SessionView::~SessionView() { state.removeListener (this); }

TabView& SessionView::add (jam::UUID uuid)
{
    addTab (uuid, juce::String {}, juce::Colours::transparentBlack,
            std::make_unique<TabView> (uuid, model, state));
    setCurrentTab (uuid);

    auto* tabView { static_cast<TabView*> (getTabContentComponent (uuid)) };
    attachments.try_emplace (uuid, std::make_unique<jam::Model::Attachment> (*tabView));

    if (auto* tab { getBar().getTabButton (uuid) })
    {
        tab->label.onTextChange = [tabView, tab]
        {
            tabView->state.setProperty (jam::ID::name, tab->label.getText(), nullptr);
        };
    }

    setName (uuid);
    lookAndFeelChanged();

    return *tabView;
}

void SessionView::remove (jam::UUID uuid)
{
    if (auto* tabView { static_cast<TabView*> (getTabContentComponent (uuid)) })
    {
        state.removeChild (tabView->state, nullptr);
        attachments.erase (uuid);
        removeTab (uuid);
        lookAndFeelChanged();
    }
}

TabView& SessionView::get (jam::UUID uuid)
{
    auto* tabView { static_cast<TabView*> (getTabContentComponent (uuid)) };
    jassert (tabView != nullptr);
    return *tabView;
}

TabView* SessionView::getActiveTabView() noexcept
{
    return static_cast<TabView*> (getCurrentContentComponent());
}

void SessionView::valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property)
{
    if (property == jam::ID::name or property == jam::ID::cwd or property == ID::foregroundProcess)
    {
        const auto tabState { findAncestorTab (tree) };

        if (tabState.isValid())
        {
            const jam::UUID uuid { static_cast<int64_t> (tabState.getProperty (jam::ID::id)) };
            setName (uuid);
        }
    }
}

juce::String SessionView::getTerminalName (const juce::ValueTree& tabState)
{
    const jam::UUID sourceUuid { static_cast<int64_t> (
        tabState.getProperty (ID::focusedPane)) };
    const auto sourcePane {
        jam::Model::getChildWithID (tabState, juce::var (sourceUuid.value))
    };

    if (sourcePane.isValid())
    {
        const auto terminalState { sourcePane.getChildWithName (IDtype::terminal) };
        jassert (terminalState.isValid());

        const auto textState { terminalState.getChildWithName (jam::IDtype::text) };
        const juce::String foregroundProcess {
            textState.getProperty (ID::foregroundProcess).toString()
        };

        return foregroundProcess.isNotEmpty()
                    ? foregroundProcess
                    : juce::File (textState.getProperty (jam::ID::cwd).toString())
                          .getFileName();
    }

    return {};
}

juce::String SessionView::getName (const juce::ValueTree& tabState)
{
    const juce::String rename { tabState.getProperty (jam::ID::name).toString() };

    if (rename.isNotEmpty())
        return rename;

    return getTerminalName (tabState);
}

juce::ValueTree SessionView::findAncestorTab (juce::ValueTree tree)
{
    while (tree.isValid() and tree.getType() != IDtype::tab)
        tree = tree.getParent();

    return tree;
}

void SessionView::setName (jam::UUID uuid)
{
    if (auto* tabView { static_cast<TabView*> (getTabContentComponent (uuid)) })
    {
        const auto name { getName (tabView->getValueTree()) };
        setTabName (uuid, name);

        if (auto* tab { getBar().getTabButton (uuid) })
            tab->label.setText (name, juce::dontSendNotification);
    }
}

void SessionView::lookAndFeelChanged()
{
    setTabBarDepth (lookAndFeel.getTabBarDepth (*this));
    setPosition (lookAndFeel.getTabPosition());
}

void SessionView::currentTabChanged (jam::UUID newCurrentTab, const juce::String&)
{
    if (auto* tabView { static_cast<TabView*> (getTabContentComponent (newCurrentTab)) })
    {
        state.setProperty (ID::focusedTab, tabView->getValueTree().getProperty (jam::ID::id), nullptr);

        if (tabView->getPaneCount() > 0)
        {
            const jam::UUID rememberedPane { static_cast<int64_t> (
                tabView->getValueTree().getProperty (ID::focusedPane)) };

            if (rememberedPane.value != 0)
                tabView->get (rememberedPane).grabKeyboardFocus();
            else
                tabView->get().grabKeyboardFocus();
        }
    }
}
