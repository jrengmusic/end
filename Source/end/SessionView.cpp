#include "end/SessionView.h"

SessionView::SessionView (jam::Model& m, juce::ValueTree sessionState)
    : jam::TabbedComponent { m, sessionState }
{
    setTabBarDepth (0);
}

TabView& SessionView::add (jam::UUID uuid)
{
    OwnerComponent::add (uuid, std::make_unique<TabView> (uuid, model, state));
    setCurrentTab (uuid);

    auto* tabView { &get (uuid) };

    if (auto* tab { getBar().getTabButton (uuid) })
    {
        tab->label.onTextChange = [tabView, tab]
        {
            tabView->state.setProperty (Id::name, tab->label.getText(), nullptr);
        };
    }

    applyTabName (uuid);
    lookAndFeelChanged();

    return *tabView;
}

void SessionView::remove (jam::UUID uuid)
{
    auto& tabView { get (uuid) };
    state.removeChild (tabView.state, nullptr);
    OwnerComponent::remove (uuid);
    lookAndFeelChanged();
}

TabView& SessionView::get (jam::UUID uuid)
{
    return static_cast<TabView&> (OwnerComponent::get (uuid));
}

TabView* SessionView::getActiveTabView() noexcept
{
    const auto focused { getFocusedChild() };

    return getChildren().contains (focused)
               ? static_cast<TabView*> (getChildren().at (focused).get())
               : nullptr;
}

void SessionView::valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property)
{
    OwnerComponent::valueTreePropertyChanged (tree, property);

    if (property == Id::name or property == Id::pluginId)
    {
        const auto tabState { EditorView::findAncestorRow (tree, Id::toType (Id::tab)) };

        if (tabState.isValid())
        {
            const jam::UUID uuid { static_cast<int64_t> (tabState.getProperty (Id::id)) };
            applyTabName (uuid);
        }
    }
}

juce::String SessionView::getTabName (const juce::ValueTree& tabState) const
{
    const juce::String rename { tabState.getProperty (Id::name).toString() };

    if (rename.isNotEmpty())
        return rename;

    const jam::UUID sourceUuid { static_cast<int64_t> (
        tabState.getProperty (Id::focusedPane)) };
    const auto sourcePane {
        jam::Model::getChildWithID (tabState, juce::var (sourceUuid.value))
    };

    return sourcePane.isValid() ? sourcePane.getProperty (Id::name).toString() : juce::String {};
}

void SessionView::applyTabName (jam::UUID uuid)
{
    if (getChildren().contains (uuid))
    {
        const auto name { getTabName (get (uuid).getValueTree()) };
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
