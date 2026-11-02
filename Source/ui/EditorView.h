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
 * session holds for this pane. When the focus of the own row becomes 1, the
 * hosted editor takes the keyboard focus.
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

    /** @brief Walks tree's own ancestor chain up to and including the first
     *  row of the given type.
     *  @param tree Starting ValueTree; walked upward through getParent().
     *  @param type Row type to stop at.
     *  @return The first ancestor row (or tree itself) matching type, or
     *          an invalid ValueTree when no ancestor of that type exists.
     */
    static juce::ValueTree findAncestorRow (juce::ValueTree tree, const juce::Identifier& type);

    /**
     * @brief Returns the parameter of a plugin whose name equals a name.
     * @param plugin  Plugin whose parameters are searched.
     * @param name    Name to match, compared with the first `maximumNameLength` characters of each parameter name.
     * @return The first parameter with that name, or nullptr when none has it.
     */
    static juce::AudioProcessorParameter* getParameter (juce::AudioProcessor& plugin, const juce::String& name);

private:
    /**
     * @brief Grabs the keyboard focus for the hosted editor when the focus of the own row becomes 1.
     * @param tree      The ValueTree whose property changed.
     * @param property  The identifier of the changed property.
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
