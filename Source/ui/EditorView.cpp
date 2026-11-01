#include "ui/EditorView.h"
#include "plugins/Nexus.h"

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

        static constexpr int maximumNameLength { 128 };
        const auto bounds { (*static_cast<juce::Rectangle<int> (* const*)()> (Nexus::getInstance()->getExtension (Id::enaxDisplayBounds.toRawUTF8())))() };
        const juce::NormalisableRange<float> widthRange { static_cast<float> (jam::PaneComponent::minimumPaneExtent), static_cast<float> (bounds.getWidth()) };
        const juce::NormalisableRange<float> heightRange { static_cast<float> (jam::PaneComponent::minimumPaneExtent), static_cast<float> (bounds.getHeight()) };

        for (auto* parameter : editor->processor.getParameters())
        {
            if (juce::Identifier { parameter->getName (maximumNameLength) } == juce::Identifier { jam::Format::toTitleCase (Id::width.toString()) })
            {
                parameter->beginChangeGesture();
                parameter->setValueNotifyingHost (widthRange.convertTo0to1 (widthRange.snapToLegalValue (static_cast<float> (getWidth()))));
                parameter->endChangeGesture();
            }

            if (juce::Identifier { parameter->getName (maximumNameLength) } == juce::Identifier { jam::Format::toTitleCase (Id::height.toString()) })
            {
                parameter->beginChangeGesture();
                parameter->setValueNotifyingHost (heightRange.convertTo0to1 (heightRange.snapToLegalValue (static_cast<float> (getHeight()))));
                parameter->endChangeGesture();
            }
        }
    }
}

void EditorView::childBoundsChanged (juce::Component* child)
{
    if (child == editor.get())
        resized();
}

void EditorView::valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property)
{
    if (tree == state and property == Id::focus and static_cast<int> (tree.getProperty (Id::focus)) == 1)
    {
        if (editor != nullptr)
            editor->grabKeyboardFocus();
    }
}

void EditorView::createProcessorEditor()
{
    const jam::UUID uuid { state.getProperty (Id::id) };

    const auto sessionState { findAncestorRow (state, Id::toType (Id::session)) };
    jassert (sessionState.isValid());

    if (sessionState.isValid())
    {
        const jam::UUID sessionUuid { sessionState.getProperty (Id::id) };
        auto& session { Nexus::getInstance()->getSession (sessionUuid) };

        if (session.contains (uuid))
        {
            editor.reset (session.getPlugin (uuid).createEditorAndMakeActive());

            if (editor != nullptr)
                addAndMakeVisible (*editor);
        }
    }
}

juce::ValueTree EditorView::findAncestorRow (juce::ValueTree tree, const juce::Identifier& type)
{
    while (tree.isValid() and tree.getType() != type)
        tree = tree.getParent();

    return tree;
}
