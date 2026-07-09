#include "end/SessionView.h"
#include "lookAndFeel/ENDLookAndFeel.h"

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
    auto* tabView { new TabView (uuid, model, state) };

    addTab (uuid.toString(), juce::Colours::transparentBlack, tabView, true);
    setCurrentTabIndex (getNumTabs() - 1);

    attachments.try_emplace (uuid, std::make_unique<jam::Model::Attachment> (*tabView));

    if (auto* tab { getBar().getTabButton (getNumTabs() - 1) })
    {
        tab->label.onTextChange = [tabView, tab]
        {
            tabView->state.setProperty (jam::ID::name, tab->label.getText(), nullptr);
        };
    }

    applyTabTitle (tabView->state);
    lookAndFeelChanged();

    return *tabView;
}

void SessionView::remove (jam::UUID uuid)
{
    for (int i { 0 }; i < getNumTabs(); ++i)
    {
        auto* tabView { static_cast<TabView*> (getTabContentComponent (i)) };

        if (static_cast<int64_t> (tabView->state.getProperty (jam::ID::id)) == uuid.value)
        {
            state.removeChild (tabView->state, nullptr);
            attachments.erase (uuid);
            removeTab (i);
            setCurrentTabIndex (juce::jmin (i, getNumTabs() - 1));
            lookAndFeelChanged();
            break;
        }
    }
}

TabView& SessionView::get (jam::UUID uuid)
{
    TabView* found { nullptr };

    for (int i { 0 }; i < getNumTabs() and found == nullptr; ++i)
    {
        auto* tabView { static_cast<TabView*> (getTabContentComponent (i)) };

        if (static_cast<int64_t> (tabView->state.getProperty (jam::ID::id)) == uuid.value)
            found = tabView;
    }

    jassert (found != nullptr);
    return *found;
}

TabView* SessionView::getActiveTabView() noexcept
{
    return static_cast<TabView*> (getCurrentContentComponent());
}

void SessionView::valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property)
{
    if (property == jam::ID::name or property == jam::ID::cwd or property == ID::foregroundProcess)
        applyTabTitle (findAncestorTab (tree));
}

juce::String SessionView::getTitle (const juce::ValueTree& tabState)
{
    const juce::String rename { tabState.getProperty (jam::ID::name).toString() };
    juce::String title;

    if (rename.isNotEmpty())
    {
        title = rename;
    }
    else
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

            title = foregroundProcess.isNotEmpty()
                        ? foregroundProcess
                        : juce::File (textState.getProperty (jam::ID::cwd).toString())
                              .getFileName();
        }
    }

    return title;
}

juce::ValueTree SessionView::findAncestorTab (juce::ValueTree tree)
{
    while (tree.isValid() and tree.getType() != IDtype::tab)
        tree = tree.getParent();

    return tree;
}

void SessionView::applyTabTitle (const juce::ValueTree& tabState)
{
    const auto title { getTitle (tabState) };

    for (int i { 0 }; i < getNumTabs(); ++i)
    {
        if (static_cast<TabView*> (getTabContentComponent (i))->getValueTree() == tabState)
        {
            setTabName (i, title);

            if (auto* tab { getBar().getTabButton (i) })
                tab->label.setText (title, juce::dontSendNotification);

            break;
        }
    }
}

void SessionView::lookAndFeelChanged()
{
    setTabBarDepth (lookAndFeel.getTabBarDepth (*this));
    setOrientation (lookAndFeel.getTabPosition());
}

void SessionView::currentTabChanged (int newCurrentTabIndex, const juce::String&)
{
    if (auto* tabView { static_cast<TabView*> (getTabContentComponent (newCurrentTabIndex)) })
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
