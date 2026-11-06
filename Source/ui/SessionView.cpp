#include "ui/SessionView.h"

SessionView::SessionView (jam::Model& newModel, juce::ValueTree sessionState)
    : jam::TabbedComponent { newModel, sessionState }
{
    setTabBarDepth (0);
}

void SessionView::add (jam::UUID uuid)
{
    juce::ValueTree row { jam::Format::toType (Id::tab) };
    row.setProperty (Id::id, uuid.value, nullptr);
    state.appendChild (row, nullptr);
}

void SessionView::valueTreeChildAdded (juce::ValueTree& parentTree, juce::ValueTree& child)
{
    if (parentTree == state and child.hasType (jam::Format::toType (Id::tab)))
    {
        const jam::UUID uuid { static_cast<int64_t> (child.getProperty (Id::id)) };

        OwnerComponent::add (uuid, std::make_unique<TabView> (model, child));

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
    }
}

void SessionView::valueTreeChildRemoved (juce::ValueTree& parentTree, juce::ValueTree& child, int)
{
    if (parentTree == state and child.hasType (jam::Format::toType (Id::tab)))
    {
        OwnerComponent::remove (jam::UUID { static_cast<int64_t> (child.getProperty (Id::id)) });
        lookAndFeelChanged();
    }
}

void SessionView::childRemoved (jam::UUID uuid, const juce::ValueTree& row)
{
    jam::TabbedComponent::childRemoved (uuid, row);

    if (getNumChildren() == 0)
        commandManager->invokeDirectly (map::CommandId::quit, false);
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

void SessionView::setZoom (juce::CommandID command, float step)
{
    for (const auto& [uuid, child] : getChildren())
        static_cast<TabView&> (*child).setZoom (command, step);
}

void SessionView::valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property)
{
    OwnerComponent::valueTreePropertyChanged (tree, property);

    if (property == Id::name or property == Id::focusedPane or tree.hasType (jam::Format::toType (Id::pluginState)))
    {
        for (const auto& [uuid, child] : getChildren())
            if (tree == child->state or tree.isAChildOf (child->state))
                applyTabName (uuid);
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

    if (sourcePane.isValid())
    {
        const auto pluginState { sourcePane.getChildWithName (jam::Format::toType (Id::pluginState)) };

        for (const auto& id : { Id::title, Id::foregroundProcess })
        {
            const juce::String paneTitle { pluginState.getProperty (id).toString() };

            if (paneTitle.isNotEmpty())
                return paneTitle;
        }

        const juce::String directoryName { juce::File { pluginState.getProperty (Id::cwd).toString() }.getFileName() };

        return directoryName.isNotEmpty() ? directoryName : sourcePane.getProperty (Id::name).toString();
    }

    return juce::String {};
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
