#include "end/EditorView.h"
#include "Nexus.h"

EditorView::EditorView (jam::Model& model, juce::ValueTree tabState, jam::UUID uuid)
    : jam::PaneComponent (model, tabState, Id::toType (Id::pane), uuid)
{
    model.createAndAddParameter<jam::Parameter<float>> (state, Id::zoom, defaultZoom);

    model.addListener (this);

    createProcessorEditor();
}

EditorView::~EditorView()
{
    model.removeListener (this);

    if (editor != nullptr)
        editor->processor.editorBeingDeleted (editor.get());
}

void EditorView::resized()
{
    jam::PaneComponent::resized();

    if (editor != nullptr)
    {
        if (editor->isResizable())
            editor->setBounds (getLocalBounds());
        else
            editor->setTopLeftPosition (0, 0);
    }
}

void EditorView::childBoundsChanged (juce::Component* child)
{
    if (child == editor.get())
        resized();
}

void EditorView::valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property)
{
    if (tree == state and property == Id::pluginId)
        createProcessorEditor();
}

void EditorView::createProcessorEditor()
{
    const auto pluginId { state.getProperty (Id::pluginId).toString() };
    const jam::UUID uuid { state.getProperty (Id::id) };

    const auto sessionState { findAncestorRow (state, Id::toType (Id::session)) };
    jassert (sessionState.isValid());

    if (sessionState.isValid())
    {
        const jam::UUID sessionUuid { sessionState.getProperty (Id::id) };
        auto& session { Nexus::getInstance()->getSession (sessionUuid) };

        if (pluginId.isNotEmpty() and session.contains (uuid))
        {
            if (editor != nullptr)
                editor->processor.editorBeingDeleted (editor.get());

            editor.reset (session.getPlugin (uuid).createEditorAndMakeActive());

            if (editor != nullptr)
            {
                addAndMakeVisible (*editor);
                resized();
            }
        }
    }
}

juce::ValueTree EditorView::findAncestorRow (juce::ValueTree tree, const juce::Identifier& type)
{
    while (tree.isValid() and tree.getType() != type)
        tree = tree.getParent();

    return tree;
}
