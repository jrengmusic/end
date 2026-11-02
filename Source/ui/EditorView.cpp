#include "ui/EditorView.h"
#include "plugins/Nexus.h"

EditorView::EditorView (jam::Model& model, juce::ValueTree tabState, jam::UUID uuid)
    : jam::PaneComponent (model, tabState, Id::toType (Id::pane), uuid)
{
    state.addListener (this);

    createProcessorEditor();
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

    if (editor != nullptr)
    {
        if (editor->isResizable())
            editor->setBounds (getLocalBounds());
        else
            editor->setTopLeftPosition (0, 0);

        const auto bounds { (*static_cast<juce::Rectangle<int> (* const*)()> (Nexus::getInstance()->getExtension (Id::enaxDisplayBounds.toRawUTF8())))() };
        const juce::NormalisableRange<float> widthRange { static_cast<float> (jam::PaneComponent::minimumPaneExtent), static_cast<float> (bounds.getWidth()) };
        const juce::NormalisableRange<float> heightRange { static_cast<float> (jam::PaneComponent::minimumPaneExtent), static_cast<float> (bounds.getHeight()) };

        static const juce::String widthName { jam::Format::toParameterName (Id::master.toString(), Id::width.toString()) };
        static const juce::String heightName { jam::Format::toParameterName (Id::master.toString(), Id::height.toString()) };

        const auto setNormalised { [] (juce::AudioProcessorParameter* parameter, float value)
                                   {
                                       if (parameter != nullptr)
                                       {
                                           parameter->beginChangeGesture();
                                           parameter->setValueNotifyingHost (value);
                                           parameter->endChangeGesture();
                                       }
                                   } };

        setNormalised (getParameter (editor->processor, widthName), widthRange.convertTo0to1 (widthRange.snapToLegalValue (static_cast<float> (getWidth()))));
        setNormalised (getParameter (editor->processor, heightName), heightRange.convertTo0to1 (heightRange.snapToLegalValue (static_cast<float> (getHeight()))));
    }
}

void EditorView::childBoundsChanged (juce::Component* child)
{
    if (child == editor.get())
        resized();
}

void EditorView::valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property)
{
    if (property == Id::focus and static_cast<int> (tree.getProperty (Id::focus)) == 1)
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

juce::AudioProcessorParameter* EditorView::getParameter (juce::AudioProcessor& plugin, const juce::String& name)
{
    const auto& parameters { plugin.getParameters() };
    const auto found { std::find_if (parameters.begin(), parameters.end(),
                                     [&name] (const juce::AudioProcessorParameter* parameter)
                                     {
                                         return parameter->getName (maximumNameLength).compare (name) == 0;
                                     }) };

    if (found != parameters.end()) return *found;
    return nullptr;
}
