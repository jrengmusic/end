/**
 * @file EditorView.h
 * @brief Pane hosting one hosted plugin's editor as a jam pane component.
 */
#pragma once
#include <JuceHeader.h>
#include "generated/Generated.h"

/**
 * @class EditorView
 * @brief One pane in a TabView's graph, wrapping a hosted plugin's own
 *        juce::AudioProcessorEditor.
 *
 * Re-creates the editor whenever state's Id::pluginId property changes,
 * so a pane that starts empty and later binds to a plugin picks up the
 * new editor without recreating the pane itself.
 */
class EditorView
    : public jam::PaneComponent
    , public juce::ValueTree::Listener
{
public:
    EditorView (jam::Model& model, juce::ValueTree tabState, jam::UUID uuid);
    ~EditorView() override;

    static constexpr float defaultZoom { 1.0f };
    static constexpr float zoomMin { 0.25f };
    static constexpr float zoomMax { 4.0f };

    void resized() override;
    void childBoundsChanged (juce::Component* child) override;

    /** @brief Walks tree's own ancestor chain up to and including the first
     *  row of the given type.
     *  @param tree Starting ValueTree; walked upward through getParent().
     *  @param type Row type to stop at.
     *  @return The first ancestor row (or tree itself) matching type, or
     *          an invalid ValueTree when no ancestor of that type exists.
     */
    static juce::ValueTree findAncestorRow (juce::ValueTree tree, const juce::Identifier& type);

private:
    /** @brief Re-creates the hosted editor when state's Id::pluginId changes.
     *  @param tree     The ValueTree whose property changed.
     *  @param property The identifier of the changed property.
     */
    void valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property) override;

    /** @brief Resolves the hosted plugin via the ancestor session and this
     *  pane's own identity, then rebuilds and mounts its editor.
     */
    void createProcessorEditor();

    std::unique_ptr<juce::AudioProcessorEditor> editor;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EditorView)
};
