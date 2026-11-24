#include "ui/SessionView.h"

SessionView::SessionView (jam::Model& newModel, juce::ValueTree sessionState)
    : jam::TabbedComponent { newModel, sessionState }
{
    setTabBarDepth (0);

    const auto focused { getFocusedChild() };

    for (auto tab : state)
        addTabView (tab);

    if (getChildren().contains (focused))
        setCurrentTab (focused);
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
        addTabView (child);
    else if (parentTree.hasType (jam::Format::toType (Id::plugin)))
        for (const auto& [uuid, tab] : getChildren())
            if (parentTree.isAChildOf (tab->state))
                setTabName (uuid);
}

void SessionView::addTabView (const juce::ValueTree& tabRow)
{
    const jam::UUID uuid { static_cast<int64_t> (tabRow.getProperty (Id::id)) };

    OwnerComponent::add (uuid, std::make_unique<TabView> (model, tabRow));

    if (auto* tab { getBar().getTabButton (uuid) })
    {
        tab->label.onTextChange = [this, uuid, tab]
        {
            get (uuid).getValueTree().setProperty (Id::name, tab->label.getText(), nullptr);
        };

        tab->label.onEditorHide = [this, uuid, tab]
        {
            if (tab->label.getText().compare (tab->getButtonText()) == 0)
                setTabName (uuid);

            if (auto* tabView { getActiveTabView() })
                tabView->focusPane (tabView->getFocusedPane());
        };
    }

    setTabName (uuid);
    lookAndFeelChanged();
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

void SessionView::valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property)
{
    jam::TabbedComponent::valueTreePropertyChanged (tree, property);

    const auto isFocusOrName { (property == Id::name or property == Id::focusedPane)
                               and (tree.hasType (jam::Format::toType (Id::tab)) or tree.hasType (jam::Format::toType (Id::pane))) };
    const auto isTerminalTitle { (property == Id::title or property == Id::foregroundProcess or property == Id::cwd)
                                 and tree.hasType (jam::Format::toType (Id::terminal)) };

    if (isFocusOrName or isTerminalTitle)
    {
        for (const auto& [uuid, child] : getChildren())
            if (tree == child->state or tree.isAChildOf (child->state))
                setTabName (uuid);
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
        const auto terminal { Session::getTerminal (sourcePane) };

        if (terminal.isValid())
        {
            for (const auto& id : { Id::title, Id::foregroundProcess })
            {
                const juce::String paneTitle { terminal.getProperty (id).toString() };

                if (paneTitle.isNotEmpty())
                    return paneTitle;
            }

            const juce::URL cwd { terminal.getProperty (Id::cwd).toString() };
            const juce::String directoryName { cwd.isLocalFile() ? cwd.getLocalFile().getFileName() : juce::String {} };

            if (directoryName.isNotEmpty())
                return directoryName;
        }

        return sourcePane.getProperty (Id::name).toString();
    }

    return juce::String {};
}

void SessionView::setTabName (jam::UUID uuid)
{
    if (auto* tab { getBar().getTabButton (uuid) })
    {
        if (not tab->label.isBeingEdited())
        {
            const auto name { getTabName (get (uuid).getValueTree()) };

            jam::TabbedComponent::setTabName (uuid, name);
            tab->label.setText (name, juce::dontSendNotification);
        }
    }
}

void SessionView::lookAndFeelChanged()
{
    setTabBarDepth (lookAndFeel.getTabBarDepth (*this));
    setPosition (lookAndFeel.getTabPosition());
}
