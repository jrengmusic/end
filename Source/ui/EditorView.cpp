#include "ui/EditorView.h"
#include "plugins/Nexus.h"
#include "action/ENDActions.h"

EditorView::EditorView (jam::Model& model, juce::ValueTree tabState, jam::UUID uuid)
    : jam::PaneComponent (model, tabState, Id::toType (Id::pane), uuid)
{
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

        setSizeParameters (session.getPlugin (uuid), getLocalBounds().reduced (inset));
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
        const juce::String plugin { files[0] };

        ENDActions::getInstance()->get (Id::newPane, edge);
        ENDActions::getInstance()->get (Id::loadPlugin, plugin);
    }
}

void EditorView::setEditorBounds (int inset)
{
    if (editor->isResizable())
        editor->setBounds (getLocalBounds().reduced (inset));
    else
        editor->setTopLeftPosition (inset, inset);
}

void EditorView::setSizeParameters (juce::AudioProcessor& plugin, juce::Rectangle<int> inner)
{
    const auto bounds { (*static_cast<juce::Rectangle<int> (*const*)()> (
        Nexus::getInstance()->getExtension (Id::enaxDisplayBounds.toRawUTF8())))() };
    const juce::NormalisableRange<float> widthRange { static_cast<float> (
                                                          jam::PaneComponent::minimumPaneExtent),
                                                      static_cast<float> (bounds.getWidth()) };
    const juce::NormalisableRange<float> heightRange { static_cast<float> (
                                                           jam::PaneComponent::minimumPaneExtent),
                                                       static_cast<float> (bounds.getHeight()) };

    if (auto* parameter { getParameter (plugin, Id::width.toString()) })
        setParameter (*parameter,
                      widthRange.convertTo0to1 (
                          widthRange.snapToLegalValue (static_cast<float> (inner.getWidth()))));

    if (auto* parameter { getParameter (plugin, Id::height.toString()) })
        setParameter (*parameter,
                      heightRange.convertTo0to1 (
                          heightRange.snapToLegalValue (static_cast<float> (inner.getHeight()))));
}

void EditorView::lookAndFeelChanged() { resized(); }

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

    if (editor != nullptr)
    {
        if (auto* parameter { getParameter (editor->processor, Id::zoom.toString()) })
            setParameter (*parameter, zoomValues.get (command, std::as_const (*parameter), step));
    }
}

void EditorView::setMode (int newMode)
{
    static const juce::NormalisableRange<float> modeRange {
        static_cast<float> (map::EditorMode::normal), static_cast<float> (map::EditorMode::terminal)
    };

    if (editor != nullptr)
    {
        if (auto* parameter { getParameter (editor->processor, Id::mode.toString()) })
            setParameter (*parameter, modeRange.convertTo0to1 (static_cast<float> (newMode)));
    }
}

void EditorView::valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property)
{
    if (property == Id::focus and editor != nullptr)
    {
        if (auto* parameter { getParameter (editor->processor, Id::focus.toString()) })
            setParameter (
                *parameter, static_cast<float> (static_cast<int> (tree.getProperty (Id::focus))));
    }

    if (property == Id::plugin and editor != nullptr)
    {
        editor->processor.editorBeingDeleted (editor.get());
        editor.reset();
    }

    if (property == Id::name and tree.hasProperty (Id::name))
    {
        resized();
        createProcessorEditor();
        resized();
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

    if (editor != nullptr)
        addAndMakeVisible (*editor, 0);

    dropTarget.toFront (false);
}

Session& EditorView::getSession()
{
    const auto sessionState { findAncestorRow (state, Id::toType (Id::session)) };
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

juce::AudioProcessorParameter*
EditorView::getParameter (juce::AudioProcessor& plugin, const juce::String& name) const
{
    if (juce::File { state.getProperty (Id::plugin).toString() }
        == ConfigDirectory::Config::getPath (Id::plugins).getChildFile (files::evePlugin))
    {
        const auto& parameters { plugin.getParameters() };
        const auto found { std::find_if (
            parameters.begin(),
            parameters.end(),
            [&name] (const juce::AudioProcessorParameter* parameter)
            {
                return parameter->getName (maximumNameLength).compareIgnoreCase (name) == 0;
            }) };

        if (found != parameters.end())
            return *found;
    }

    return nullptr;
}

void EditorView::setParameter (juce::AudioProcessorParameter& parameter, float normalisedValue)
{
    parameter.beginChangeGesture();
    parameter.setValueNotifyingHost (normalisedValue);
    parameter.endChangeGesture();
}
