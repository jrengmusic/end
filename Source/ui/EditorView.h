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
 * The editor is created once, in the constructor, from the plugin that the
 * session holds for this pane. When the Id::focus of the own row changes, the
 * focus parameter of the hosted plugin takes that value.
 */
class EditorView
    : public jam::PaneComponent
    , public juce::ValueTree::Listener
{
public:
    /**
     * @brief Creates the pane row under the tab, listens to it, and creates the hosted editor.
     * @param model     Model that owns the pane parameters.
     * @param tabState  Tree of the tab that holds the pane row.
     * @param uuid      Identity of the pane row.
     */
    EditorView (jam::Model& model, juce::ValueTree tabState, jam::UUID uuid);

    /** @brief Stops listening to the pane row and tells the processor that its editor is being deleted. */
    ~EditorView() override;

    /** @brief Longest parameter name that `getParameter()` compares. */
    static constexpr int maximumNameLength { 128 };

    /**
     * @brief Lays out the hosted editor in the pane, then writes the pane size to the width and height parameters of the plugin.
     *
     * A resizable editor takes the local bounds of the pane. Any other editor goes to the top left corner.
     * The width and the height parameters take the pane size, snapped to the legal values of their ranges.
     */
    void resized() override;

    /**
     * @brief Lays out the pane again when the hosted editor changes its bounds.
     * @param child  Component whose bounds changed.
     */
    void childBoundsChanged (juce::Component* child) override;

    /**
     * @brief Moves the zoom parameter of the hosted plugin by a zoom command.
     *
     * Zoom in adds @p step to the value and zoom out subtracts it, both clamped to 0..1. Zoom reset sets the default value.
     * The function does nothing when the pane holds no editor, or when the plugin has no zoom parameter.
     *
     * @param command  Zoom command name: zoomIn, zoomOut or zoomReset.
     * @param step     Zoom step, in normalised units.
     */
    void setZoom (const juce::Identifier& command, float step);

    /**
     * @brief Writes the editor mode to the mode parameter of the hosted plugin.
     *
     * The function does nothing when the pane holds no editor, or when the plugin has no mode parameter.
     *
     * @param newMode  Editor mode, a constant of `map::EditorMode`.
     */
    void setMode (int newMode);

    /** @brief Walks tree's own ancestor chain up to and including the first
     *  row of the given type.
     *  @param tree Starting ValueTree; walked upward through getParent().
     *  @param type Row type to stop at.
     *  @return The first ancestor row (or tree itself) matching type, or
     *          an invalid ValueTree when no ancestor of that type exists.
     */
    static juce::ValueTree findAncestorRow (juce::ValueTree tree, const juce::Identifier& type);

private:
    /**
     * @brief Returns the parameter of a plugin whose name equals a name.
     * @param plugin  Plugin whose parameters are searched.
     * @param name    Name to match, compared with the first `maximumNameLength` characters of each parameter name.
     * @return The first parameter with that name, or nullptr when none has it.
     */
    static juce::AudioProcessorParameter* getParameter (juce::AudioProcessor& plugin, const juce::String& name);

    /**
     * @brief Sets the normalised value of a parameter in a change gesture, which notifies the host.
     * @param parameter        Parameter to set.
     * @param normalisedValue  Value in the range 0 to 1.
     */
    static void setParameter (juce::AudioProcessorParameter& parameter, float normalisedValue);

    /**
     * @brief Writes the Id::focus value of the own row to the focus parameter of the hosted plugin.
     * @param tree      The ValueTree whose property changed.
     * @param property  The identifier of the changed property. Only Id::focus writes the parameter.
     */
    void valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property) override;

    /**
     * @brief Finds the plugin of this pane through the ancestor session row and the pane identity, then creates its editor and mounts it.
     *
     * The call creates no editor when the session does not hold the plugin of this pane.
     */
    void createProcessorEditor();

    /** The hosted plugin editor. It is nullptr when the session does not hold the plugin of this pane. */
    std::unique_ptr<juce::AudioProcessorEditor> editor;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EditorView)
};
