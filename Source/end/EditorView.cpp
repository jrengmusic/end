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

    auto sessionState { state.getParent() };

    while (sessionState.isValid() and sessionState.getType() != Id::toType (Id::session))
        sessionState = sessionState.getParent();

    const jam::UUID sessionUuid { sessionState.getProperty (Id::id) };
    auto& session { Nexus::getInstance()->getSession (sessionUuid) };

    if (editor == nullptr and pluginId.isNotEmpty() and session.contains (uuid))
    {
        auto& instance { session.get (uuid) };

        editor.reset (instance.createEditorAndMakeActive());

        if (editor != nullptr)
        {
            addAndMakeVisible (*editor);
            resized();
        }
    }
}
