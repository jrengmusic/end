#include "ui/EditorView.h"
#include "plugins/Nexus.h"

static juce::ValueTree findAncestorRow (juce::ValueTree tree, const juce::Identifier& type)
{
    while (tree.isValid() and tree.getType() != type)
        tree = tree.getParent();

    return tree;
}

static juce::String getInheritedDirectory (const juce::ValueTree& tabState)
{
    const auto tabRow { findAncestorRow (tabState, jam::Format::toType (Id::tab)) };
    const jam::UUID focusedUuid { static_cast<int64_t> (tabRow.getProperty (Id::focusedPane)) };
    const auto focusedPane { jam::Model::getChildWithID (tabRow, juce::var (focusedUuid.value)) };

    return focusedPane.getChildWithName (jam::Format::toType (Id::pluginState))
        .getProperty (Id::cwd)
        .toString();
}

EditorView::EditorView (jam::Model& model, juce::ValueTree tabState, jam::UUID uuid)
    : jam::PaneComponent (model, tabState, jam::Format::toType (Id::pane), uuid)
{
    state.getOrCreateChildWithName (jam::Format::toType (Id::pluginState), nullptr)
        .setProperty (Id::cwd, getInheritedDirectory (tabState), nullptr);
    registerEvents();
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

        const auto inner { getLocalBounds().reduced (inset) };

        juce::ValueTree rows { jam::Format::toType (Id::pluginState) };

        rows.setProperty (Id::width, inner.getWidth(), nullptr);
        rows.setProperty (Id::height, inner.getHeight(), nullptr);
        session.setRows (uuid, rows);
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
    auto& pane { *findParentComponentOfClass<jam::PaneComponent>() };

    pane.findParentComponentOfClass<jam::OwnerComponent>()->setFocusedChild (
        jam::UUID { static_cast<int64_t> (pane.getValueTree().getProperty (Id::id)) });
}

void EditorView::DropTarget::filesDropped (const juce::StringArray& files, int x, int y)
{
    auto& pane { *findParentComponentOfClass<jam::PaneComponent>() };
    auto& matrix { *findParentComponentOfClass<jam::MatrixComponent>() };
    const juce::Identifier edge { pane.getEdge (
        pane.getLocalPoint (this, juce::Point<int> { x, y })) };

    matrix.get (matrix.split (edge)).getValueTree().setProperty (Id::plugin, juce::File { files[0] }.getFullPathName(), nullptr);
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
    commands.add (map::CommandId::closeTerminal);
    commands.add (map::CommandId::closePane);
    commands.add (map::CommandId::closeTerminalPane);
}

void EditorView::getCommandInfo (juce::CommandID commandID, juce::ApplicationCommandInfo& result)
{
    jam::CommandManager::setInfo (
        result, map::CommandId::getInstance()->get (commandID).toString());
}

bool EditorView::perform (const InvocationInfo& info)
{
    static const jam::Function::Map<juce::CommandID, jam::Function::pointerCapacity, void (EditorView&)> paneCommands (
        []
        {
            jam::Function::Map<juce::CommandID, jam::Function::pointerCapacity, void (EditorView&)> table;

            table.add (map::CommandId::closeTerminal,
                       [] (const juce::CommandID&, EditorView& pane)
                       {
                           pane.state.removeProperty (Id::plugin, nullptr);
                       });

            table.add (map::CommandId::closePane,
                       [] (const juce::CommandID&, EditorView& pane)
                       {
                           pane.findParentComponentOfClass<jam::OwnerComponent>()->remove (
                               jam::UUID { pane.state.getProperty (Id::id) });
                       });

            table.add (map::CommandId::closeTerminalPane,
                       [] (const juce::CommandID&, EditorView& pane)
                       {
                           pane.invokeDirectly (map::CommandId::closeTerminal, false);
                           pane.invokeDirectly (map::CommandId::closePane, false);
                       });

            return table;
        }()
    );

    paneCommands.get (info.commandID, *this);

    return true;
}

void EditorView::setZoom (juce::CommandID command, float step)
{
    static constexpr float defaultZoom { 1.0f };

    static const jam::Function::Map<juce::CommandID, jam::Function::pointerCapacity, float (float, float)> zoomValues (
        []
        {
            jam::Function::Map<juce::CommandID, jam::Function::pointerCapacity, float (float, float)> table;
            table.add (map::CommandId::zoomIn,
                       [] (const juce::CommandID&, float zoom, float zoomStep)
                       {
                           return zoom + zoomStep;
                       });
            table.add (map::CommandId::zoomOut,
                       [] (const juce::CommandID&, float zoom, float zoomStep)
                       {
                           return zoom - zoomStep;
                       });
            table.add (map::CommandId::zoomReset,
                       [] (const juce::CommandID&, float, float)
                       {
                           return defaultZoom;
                       });
            return table;
        }()
    );

    const auto zoom { static_cast<float> (
        state.getChildWithName (jam::Format::toType (Id::pluginState))
            .getProperty (Id::zoom, defaultZoom)) };

    setRow (Id::zoom, zoomValues.get (command, float { zoom }, float { step }));
}

void EditorView::setMode (int newMode) { setRow (Id::mode, newMode); }

void EditorView::close (juce::CommandID command)
{
    const auto markdownFile { state.getChildWithName (jam::Format::toType (Id::pluginState)).getProperty (Id::markdownFile).toString() };

    if (markdownFile.isNotEmpty())
        setRow (Id::markdownFile, juce::String {});
    else
        invokeDirectly (command, false);
}

void EditorView::setRow (const juce::Identifier& row, const juce::var& value)
{
    getSession().setRow (jam::UUID { state.getProperty (Id::id) }, row, value);
}

void EditorView::registerEvents()
{
    events.add (Id::shellExited, [this] (const juce::Identifier&, juce::ValueTree& tree)
    {
        if (static_cast<bool> (tree.getProperty (Id::shellExited)))
            invokeDirectly (map::CommandId::closeTerminalPane, true);
    });

    events.add (Id::plugin, [this] (const juce::Identifier&, juce::ValueTree&)
    {
        if (editor != nullptr)
        {
            editor->processor.editorBeingDeleted (editor.get());
            editor.reset();
        }
    });

    events.add (Id::name, [this] (const juce::Identifier&, juce::ValueTree& tree)
    {
        if (tree.hasProperty (Id::name))
        {
            if (getSession().hasPluginState (jam::UUID { tree.getProperty (Id::id) }))
                Nexus::getInstance()->addPluginModule (
                    juce::File { tree.getProperty (Id::plugin).toString() });

            resized();
            createProcessorEditor();
            resized();
        }
    });
}

void EditorView::valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property)
{
    if (events.contains (property))
        events.get (property, tree);
}

void EditorView::createProcessorEditor()
{
    const jam::UUID uuid { state.getProperty (Id::id) };
    auto& session { getSession() };

#if JAM_USING_PROFILER
    const jam::Profiler::Zone zone { Id::paneEditor };
#endif
    editor.reset (session.getPlugin (uuid).createEditorAndMakeActive());
    addAndMakeVisible (editor.get());
    dropTarget.toFront (false);
}

Session& EditorView::getSession() const
{
    const auto sessionState { findAncestorRow (state, jam::Format::toType (Id::session)) };
    jassert (sessionState.isValid());

    const jam::UUID sessionUuid { sessionState.getProperty (Id::id) };
    return Nexus::getInstance()->getSession (sessionUuid);
}
