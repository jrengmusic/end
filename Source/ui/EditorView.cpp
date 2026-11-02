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

        const auto bounds { (*static_cast<juce::Rectangle<int> (*const*)()> (
            Nexus::getInstance()->getExtension (Id::enaxDisplayBounds.toRawUTF8())))() };
        const juce::NormalisableRange<float> widthRange {
            static_cast<float> (jam::PaneComponent::minimumPaneExtent),
            static_cast<float> (bounds.getWidth())
        };
        const juce::NormalisableRange<float> heightRange {
            static_cast<float> (jam::PaneComponent::minimumPaneExtent),
            static_cast<float> (bounds.getHeight())
        };

        static const juce::String widthName { jam::Format::toParameterName (
            Id::master.toString(), Id::width.toString()) };
        static const juce::String heightName { jam::Format::toParameterName (
            Id::master.toString(), Id::height.toString()) };

        if (auto* parameter { getParameter (editor->processor, widthName) })
            setParameter (*parameter,
                          widthRange.convertTo0to1 (
                              widthRange.snapToLegalValue (static_cast<float> (getWidth()))));

        if (auto* parameter { getParameter (editor->processor, heightName) })
            setParameter (*parameter,
                          heightRange.convertTo0to1 (
                              heightRange.snapToLegalValue (static_cast<float> (getHeight()))));
    }
}

void EditorView::childBoundsChanged (juce::Component* child)
{
    if (child == editor.get())
        resized();
}

void EditorView::setZoom (const juce::Identifier& command, float step)
{
    static const jam::Function::Map<juce::Identifier, float> zoomValues {
        []
        {
            jam::Function::Map<juce::Identifier, float> table;
            table.add<const juce::AudioProcessorParameter&, const float&> (
                Id::zoomIn,
                [] (const juce::AudioProcessorParameter& parameter, const float& zoomStep)
                {
                    return juce::jlimit (0.0f, 1.0f, parameter.getValue() + zoomStep);
                });
            table.add<const juce::AudioProcessorParameter&, const float&> (
                Id::zoomOut,
                [] (const juce::AudioProcessorParameter& parameter, const float& zoomStep)
                {
                    return juce::jlimit (0.0f, 1.0f, parameter.getValue() - zoomStep);
                });
            table.add<const juce::AudioProcessorParameter&, const float&> (
                Id::zoomReset,
                [] (const juce::AudioProcessorParameter& parameter, const float&)
                {
                    return parameter.getDefaultValue();
                });
            return table;
        }()
    };
    static const juce::String zoomName { jam::Format::toParameterName (Id::master.toString(), Id::zoom.toString()) };

    if (editor != nullptr)
    {
        if (auto* parameter { getParameter (editor->processor, zoomName) })
            setParameter (*parameter, zoomValues.get (command, std::as_const (*parameter), step));
    }
}

void EditorView::setMode (int newMode)
{
    static const juce::String modeName { jam::Format::toParameterName (Id::master.toString(), Id::mode.toString()) };
    static const juce::NormalisableRange<float> modeRange { static_cast<float> (map::EditorMode::normal), static_cast<float> (map::EditorMode::terminal) };

    if (editor != nullptr)
    {
        if (auto* parameter { getParameter (editor->processor, modeName) })
            setParameter (*parameter, modeRange.convertTo0to1 (static_cast<float> (newMode)));
    }
}

void EditorView::valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property)
{
    static const juce::String focusName { jam::Format::toParameterName (Id::master.toString(), Id::focus.toString()) };

    if (property == Id::focus and editor != nullptr)
    {
        if (auto* parameter { getParameter (editor->processor, focusName) })
            setParameter (*parameter, static_cast<float> (static_cast<int> (tree.getProperty (Id::focus))));
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
                addAndMakeVisible (*editor, 0);
        }
    }
}

juce::ValueTree EditorView::findAncestorRow (juce::ValueTree tree, const juce::Identifier& type)
{
    while (tree.isValid() and tree.getType() != type)
        tree = tree.getParent();

    return tree;
}

juce::AudioProcessorParameter*
EditorView::getParameter (juce::AudioProcessor& plugin, const juce::String& name)
{
    const auto& parameters { plugin.getParameters() };
    const auto found { std::find_if (
        parameters.begin(),
        parameters.end(),
        [&name] (const juce::AudioProcessorParameter* parameter)
        {
            return parameter->getName (maximumNameLength).compare (name) == 0;
        }) };

    if (found != parameters.end())
        return *found;
    return nullptr;
}

void EditorView::setParameter (juce::AudioProcessorParameter& parameter, float normalisedValue)
{
    parameter.beginChangeGesture();
    parameter.setValueNotifyingHost (normalisedValue);
    parameter.endChangeGesture();
}
