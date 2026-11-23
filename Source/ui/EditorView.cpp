#include "ui/EditorView.h"
#include "plugins/Nexus.h"

static juce::ValueTree findAncestorRow (juce::ValueTree tree, const juce::Identifier& type)
{
    while (tree.isValid() and tree.getType() != type)
        tree = tree.getParent();

    return tree;
}

EditorView::EditorView (jam::Model& model, juce::ValueTree tabState, jam::UUID uuid)
    : jam::PaneComponent (model, tabState, jam::Format::toType (Id::pane), uuid)
{
    const auto cwd { tabState.getParent().getParent().getProperty (Id::cwd) };

    if (cwd.toString().isNotEmpty())
        state.setProperty (Id::cwd, cwd, nullptr);

    initialise();
}

EditorView::EditorView (jam::Model& model, const juce::ValueTree& row)
    : jam::PaneComponent (model, row)
{
    initialise();
}

void EditorView::initialise()
{
    registerEvents();
    state.addListener (this);
    addAndMakeVisible (dropTarget);

    if (state.hasProperty (Id::name))
    {
        Nexus::getInstance()->addPluginModule (
            juce::File { state.getProperty (Id::plugin).toString() });
        createProcessorEditor();
    }
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

        const auto bounds { getLocalBounds().reduced (inset) };
        auto terminal { Session::getTerminal (state) };

        if (terminal.isValid())
        {
            terminal.setProperty (Id::width, bounds.getWidth(), nullptr);
            terminal.setProperty (Id::height, bounds.getHeight(), nullptr);
        }

        if (editor != nullptr)
            setEditorBounds (bounds);
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
    findParentComponentOfClass<jam::MatrixComponent>()->focusPane (*this);
}

void EditorView::DropTarget::filesDropped (const juce::StringArray& files, int x, int y)
{
    auto& pane { *findParentComponentOfClass<jam::PaneComponent>() };
    auto& matrix { *findParentComponentOfClass<jam::MatrixComponent>() };
    const juce::Identifier edge { pane.getEdge (
        pane.getLocalPoint (this, juce::Point<int> { x, y })) };

    matrix.get (matrix.split (edge))
        .getValueTree()
        .setProperty (Id::plugin, juce::File { files[0] }.getFullPathName(), nullptr);
}

void EditorView::setEditorBounds (juce::Rectangle<int> bounds)
{
    jassert (editor != nullptr);

    if (editor->isResizable())
        editor->setBounds (bounds);
    else
        editor->setTopLeftPosition (bounds.getPosition());
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
    static const jam::Function::Map<juce::CommandID,
                                    jam::Function::pointerCapacity,
                                    void (EditorView&)>
        paneCommands (
            []
            {
                jam::Function::Map<juce::CommandID, jam::Function::pointerCapacity, void (EditorView&)>
                    table;

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
                               auto terminal { Session::getTerminal (pane.state) };

                               if (terminal.getProperty (Id::markdownFile).toString().isNotEmpty())
                               {
                                   terminal.setProperty (
                                       Id::markdownFile, juce::String {}, nullptr);
                               }
                               else
                               {
                                   pane.invokeDirectly (map::CommandId::closeTerminal, false);
                                   pane.invokeDirectly (map::CommandId::closePane, false);
                               }
                           });

                return table;
            }());

    paneCommands.get (info.commandID, *this);

    return true;
}

void EditorView::setMode (int mode)
{
    auto terminal { Session::getTerminal (state) };

    if (terminal.isValid())
        terminal.setProperty (Id::mode, mode, nullptr);
}

void EditorView::registerEvents()
{
    events.add (Id::shellExited,
                [this] (const juce::Identifier&, juce::ValueTree& tree)
                {
                    if (static_cast<bool> (tree.getProperty (Id::shellExited)))
                    {
                        invokeDirectly (map::CommandId::closeTerminal, true);
                        invokeDirectly (map::CommandId::closePane, true);
                    }
                });

    events.add (Id::plugin,
                [this] (const juce::Identifier&, juce::ValueTree& tree)
                {
                    if (tree == state)
                    {
                        if (editor != nullptr)
                        {
                            editor->processor.editorBeingDeleted (editor.get());
                            editor.reset();
                        }
                    }
                });

    events.add (Id::name,
                [this] (const juce::Identifier&, juce::ValueTree& tree)
                {
                    if (tree == state)
                    {
                        if (tree.hasProperty (Id::name))
                        {
                            Nexus::getInstance()->addPluginModule (
                                juce::File { tree.getProperty (Id::plugin).toString() });

                            createProcessorEditor();
                            resized();
                        }
                    }
                });

    events.add (Id::focus,
                [this] (const juce::Identifier&, juce::ValueTree& tree)
                {
                    if (tree == state)
                    {
                        repaint();

                        if (editor != nullptr and isShowing() and static_cast<bool> (tree.getProperty (Id::focus)))
                            editor->grabKeyboardFocus();
                    }
                });
}

void EditorView::valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property)
{
    if (events.contains (property))
        events.get (property, tree);
}

void EditorView::valueTreeChildAdded (juce::ValueTree& parentTree, juce::ValueTree& child)
{
    if (parentTree.hasType (jam::Format::toType (Id::plugin)) and parentTree.getParent() == state)
    {
        auto terminal { child.getChildWithName (jam::Format::toType (Id::terminal)) };

        if (terminal.isValid())
        {
            if (state.hasProperty (Id::cwd))
            {
                terminal.setProperty (Id::cwd, state.getProperty (Id::cwd), nullptr);
                state.removeProperty (Id::cwd, nullptr);
            }

            terminal.setProperty (Id::focus, state.getProperty (Id::focus), nullptr);
            resized();
        }
    }
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

    if (editor != nullptr)
        editor->setMouseClickGrabsKeyboardFocus (false);

    events.get (Id::focus, state);
}

Session& EditorView::getSession() const
{
    const auto sessionState { findAncestorRow (state, jam::Format::toType (Id::session)) };
    jassert (sessionState.isValid());

    const jam::UUID sessionUuid { sessionState.getProperty (Id::id) };
    return Nexus::getInstance()->getSession (sessionUuid);
}
