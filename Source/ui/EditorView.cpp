#include "ui/EditorView.h"
#include "plugins/Nexus.h"
#include "ui/SessionView.h"

static juce::String getInheritedDirectory (const juce::ValueTree& tabState)
{
    const auto tabRow { EditorView::findAncestorRow (tabState, jam::Format::toType (Id::tab)) };
    const jam::UUID focusedUuid { static_cast<int64_t> (tabRow.getProperty (Id::focusedPane)) };
    const auto focusedPane { jam::Model::getChildWithID (tabRow, juce::var (focusedUuid.value)) };

    return focusedPane.getChildWithName (jam::Format::toType (Id::terminal)).getProperty (Id::cwd).toString();
}

EditorView::EditorView (jam::Model& model, juce::ValueTree tabState, jam::UUID uuid)
    : jam::PaneComponent (model, tabState, jam::Format::toType (Id::pane), uuid)
{
    state.getOrCreateChildWithName (jam::Format::toType (Id::terminal), nullptr).setProperty (Id::cwd, getInheritedDirectory (tabState), nullptr);
    state.addListener (this);
    addAndMakeVisible (dropTarget);
}

EditorView::~EditorView()
{
    state.removeListener (this);

    if (editor != nullptr)
        editor->processor.editorBeingDeleted (editor.get());
}

void EditorView::resized()
{
    jam::PaneComponent::resized();
    dropTarget.setBounds (getLocalBounds());

    const jam::UUID uuid { state.getProperty (Id::id) };
    auto& session { getSession() };

    if (session.contains (uuid))
    {
        jassert (dynamic_cast<jam::StyleCustom*> (&getLookAndFeel()) != nullptr);

        constexpr float outlineSides { 2.0f };

        const int inset { juce::roundToInt (
            static_cast<float> (static_cast<jam::StyleCustom&> (getLookAndFeel()).getPanePadding())
            + outlineSides * jam::PaneComponent::lineThickness) };

        if (editor != nullptr)
            setEditorBounds (inset);
    }
}

bool EditorView::DropTarget::isInterestedInFileDrag (const juce::StringArray& files)
{
    return files.size() == 1
           and Nexus::getInstance()->getActiveSession().getPluginFormat (juce::File { files[0] })
                   != nullptr;
}

void EditorView::DropTarget::fileDragEnter (const juce::StringArray&, int, int)
{
    if (auto* pane { findParentComponentOfClass<jam::PaneComponent>() })
    {
        if (auto* owner { pane->findParentComponentOfClass<jam::OwnerComponent>() })
            owner->setFocusedChild (
                jam::UUID { static_cast<int64_t> (pane->getValueTree().getProperty (Id::id)) });
    }
}

void EditorView::DropTarget::filesDropped (const juce::StringArray& files, int x, int y)
{
    if (auto* pane { findParentComponentOfClass<jam::PaneComponent>() })
    {
        const juce::Identifier edge { pane->getEdge (
            pane->getLocalPoint (this, juce::Point<int> { x, y })) };

        if (auto* tabView { findParentComponentOfClass<TabView>() })
        {
            tabView->addPane (edge);
            tabView->loadPlugin (files[0]);
        }
    }
}

void EditorView::setEditorBounds (int inset)
{
    if (editor->isResizable())
        editor->setBounds (getLocalBounds().reduced (inset));
    else
        editor->setTopLeftPosition (inset, inset);
}

void EditorView::lookAndFeelChanged() { resized(); }

juce::ApplicationCommandTarget* EditorView::getNextCommandTarget()
{
    return findFirstTargetParentComponent();
}

void EditorView::getAllCommands (juce::Array<juce::CommandID>& commands)
{
    commands.add (map::CommandId::closePane);
}

void EditorView::getCommandInfo (juce::CommandID commandID, juce::ApplicationCommandInfo& result)
{
    jam::CommandManager::setInfo (result, map::CommandId::getInstance()->get (commandID).toString());
}

bool EditorView::perform (const InvocationInfo&)
{
    const jam::UUID uuid { state.getProperty (Id::id) };
    auto& session { getSession() };
    auto* tabView { findParentComponentOfClass<TabView>() };

    if (tabView->getNumChildren() > 1)
    {
        tabView->remove (uuid);
        session.removePlugin (uuid);
    }
    else
    {
        tabView->findParentComponentOfClass<SessionView>()->remove (jam::UUID { static_cast<int64_t> (tabView->getValueTree().getProperty (Id::id)) });
    }

    return true;
}

void EditorView::childBoundsChanged (juce::Component* child)
{
    if (child == editor.get())
        resized();
}

void EditorView::setZoom (juce::CommandID command, float step)
{
    static constexpr float defaultZoom { 1.0f };

    static const jam::Function::Map<juce::CommandID, float> zoomValues {
        []
        {
            jam::Function::Map<juce::CommandID, float> table;
            table.add<float, float> (map::CommandId::zoomIn, [] (float zoom, float zoomStep) { return zoom + zoomStep; });
            table.add<float, float> (map::CommandId::zoomOut, [] (float zoom, float zoomStep) { return zoom - zoomStep; });
            table.add<float, float> (map::CommandId::zoomReset, [] (float, float) { return defaultZoom; });
            return table;
        }()
    };

    const auto zoom { static_cast<float> (state.getChildWithName (jam::Format::toType (Id::terminal)).getProperty (Id::zoom, defaultZoom)) };

    setRows (Id::zoom, zoomValues.get (command, float { zoom }, float { step }));
}

void EditorView::setMode (int newMode)
{
    setRows (Id::mode, newMode);
}

void EditorView::setRows (const juce::Identifier& row, const juce::var& value)
{
    juce::ValueTree rows { jam::Format::toType (Id::terminal) };

    rows.setProperty (row, value, nullptr);
    getSession().setRows (jam::UUID { state.getProperty (Id::id) }, rows);
}

void EditorView::valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property)
{
    if (property == Id::shellExited and static_cast<bool> (tree.getProperty (property)))
        invokeDirectly (map::CommandId::closePane, true);

    const auto isPluginLoaded { property == Id::name and tree.hasProperty (Id::name) };

    if (property == Id::plugin and editor != nullptr)
    {
        editor->processor.editorBeingDeleted (editor.get());
        editor.reset();
    }

    if (isPluginLoaded)
    {
        if (getSession().hasPluginState (jam::UUID { tree.getProperty (Id::id) }))
            Nexus::getInstance()->addPluginModule (juce::File { tree.getProperty (Id::plugin).toString() });

        resized();
        createProcessorEditor();
        resized();
    }

    if (tree == state and (property == Id::focus or isPluginLoaded))
        setRows (Id::focus, static_cast<int> (tree.getProperty (Id::focus)));
}

void EditorView::createProcessorEditor()
{
    const jam::UUID uuid { state.getProperty (Id::id) };
    auto& session { getSession() };

#if JAM_USING_PROFILER
    const jam::Profiler::Zone zone { Id::paneEditor };
#endif
    editor.reset (session.getPlugin (uuid).createEditorAndMakeActive());

    if (editor != nullptr)
        addAndMakeVisible (*editor, 0);

    dropTarget.toFront (false);
}

Session& EditorView::getSession()
{
    const auto sessionState { findAncestorRow (state, jam::Format::toType (Id::session)) };
    jassert (sessionState.isValid());

    const jam::UUID sessionUuid { sessionState.getProperty (Id::id) };
    return Nexus::getInstance()->getSession (sessionUuid);
}

juce::ValueTree EditorView::findAncestorRow (juce::ValueTree tree, const juce::Identifier& type)
{
    while (tree.isValid() and tree.getType() != type)
        tree = tree.getParent();

    return tree;
}
