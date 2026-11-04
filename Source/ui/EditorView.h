/**
 * @file EditorView.h
 * @brief Pane hosting one hosted plugin's editor as a jam pane component.
 */
#pragma once
#include <JuceHeader.h>
#include "generated/Generated.h"
#include "plugins/Session.h"

/**
 * @class EditorView
 * @brief One pane in a TabView's graph, wrapping a hosted plugin's own
 *        juce::AudioProcessorEditor.
 *
 * The editor is created when the pane row receives Id::name. It is not created in
 * the constructor. The name branch lays out the pane, creates the editor, and lays
 * out the pane again. addPlugin() sets Id::name after it binds the plugin, so the
 * session holds the plugin at that time. When the Id::plugin of the pane row changes,
 * the editor is destroyed. When the Id::focus of the own row changes, the focus
 * parameter of the hosted plugin takes that value.
 *
 * A DropTarget covers the pane. A plugin file dropped on the pane splits the pane
 * at the drop edge, and the file loads into the new pane.
 */
class EditorView
    : public jam::PaneComponent
    , public juce::ValueTree::Listener
{
public:
    /**
     * @brief Builds or adopts the pane row under the tab, listens to it, and mounts the drop target.
     *
     * The editor is not created here.
     *
     * @param model     Model that owns the pane parameters.
     * @param tabState  Tree of the tab that holds the pane row.
     * @param uuid      Identity of the pane row.
     */
    EditorView (jam::Model& model, juce::ValueTree tabState, jam::UUID uuid);

    /** @brief Stops listening to the pane row and tells the processor that its editor is being deleted. */
    ~EditorView() override;

    /**
     * @brief Lays out the hosted editor in the pane, then writes the inner size to the width and height parameters of the plugin when the pane holds the ENAX mark.
     *
     * The inset is the pane padding plus two outline line thicknesses. A resizable editor takes the pane bounds reduced by the inset.
     * Any other editor sits at the inset position from the top left corner.
     * The inner size is the pane bounds reduced by the inset. The width and the height parameters take the inner size,
     * snapped to the legal values of their ranges.
     */
    void resized() override;

    /**
     * @brief Lays out the pane again when the hosted editor changes its bounds.
     * @param child  Component whose bounds changed.
     */
    void childBoundsChanged (juce::Component* child) override;

    void lookAndFeelChanged() override;

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
     * @brief Drop target that covers a pane and accepts one plugin file.
     *
     * A drag over the pane focuses the pane. A drop splits the focused pane at the drop edge, then loads the file into the new pane.
     */
    class DropTarget : public juce::Component,
                       public juce::FileDragAndDropTarget
    {
    public:
        /**
         * @brief Accepts a drag of one file that a plugin format of the focused session can read.
         * @param files  Files dragged over the target.
         * @return True when exactly one file is dragged, and a plugin format reads it.
         */
        bool isInterestedInFileDrag (const juce::StringArray& files) override;

        /**
         * @brief Focuses the pane under the drag, because a split divides the focused pane.
         * @param files  Files dragged over the target.
         * @param x      Horizontal drag position in the target.
         * @param y      Vertical drag position in the target.
         */
        void fileDragEnter (const juce::StringArray& files, int x, int y) override;

        /**
         * @brief Splits the pane at the drop edge, then loads the first dropped file into the new pane.
         *
         * The drop edge is the edge of the pane triangle that holds the drop point, from PaneComponent::getEdge().
         * The split focuses the new pane, so the file loads into it.
         *
         * @param files  Dropped files. The first file is loaded.
         * @param x      Horizontal drop position in the target.
         * @param y      Vertical drop position in the target.
         */
        void filesDropped (const juce::StringArray& files, int x, int y) override;
    };

    /**
     * @brief Returns the parameter of a plugin whose name equals a name, compared without case.
     *
     * The search runs only when the pane row holds the Id::enax mark. The ENAX lane reads the width, height,
     * zoom, mode and focus parameters through this search. A pane without the mark has no parameter to return.
     *
     * @param plugin  Plugin whose parameters are searched.
     * @param name    Name to match. The match reads each parameter name up to a fixed length limit.
     * @return The first parameter with that name, or nullptr when none has it or the pane does not hold the mark.
     */
    juce::AudioProcessorParameter* getParameter (juce::AudioProcessor& plugin, const juce::String& name) const;

    /**
     * @brief Answers whether the pane row holds the Id::enax mark, which marks the pane as an ENAX client.
     * @return True when the pane row holds the mark.
     */
    bool isENAX() const;

    /**
     * @brief Sets the normalised value of a parameter in a change gesture, which notifies the host.
     * @param parameter        Parameter to set.
     * @param normalisedValue  Value in the range 0 to 1.
     */
    static void setParameter (juce::AudioProcessorParameter& parameter, float normalisedValue);

    /**
     * @brief Places the hosted editor inside the pane outline.
     *
     * A resizable editor takes the pane bounds reduced by @p inset on each side. Any other editor moves to (inset, inset).
     *
     * @param inset  Distance in pixels between the pane bounds and the editor.
     */
    void setEditorBounds (int inset);

    /**
     * @brief Writes the inner size of the pane to the width and height parameters of a plugin, snapped to the legal range.
     *
     * The width range runs from PaneComponent::minimumPaneExtent to the total display width. The height range runs from
     * the same minimum to the total display height. The display bounds come from Nexus::getDisplayBounds(), the getter that
     * the enaxDisplayBounds host extension also returns.
     * A parameter is written only when getParameter() finds it.
     *
     * @param plugin  Plugin whose width and height parameters are written.
     * @param inner   Inner size of the pane, in pixels.
     */
    void setSizeParameters (juce::AudioProcessor& plugin, juce::Rectangle<int> inner);

    /**
     * @brief Returns the session that holds the plugin of this pane.
     *
     * The session is found through the ancestor session row of the pane row, and its identity selects it in Nexus.
     * The call asserts that the pane lies under a session row.
     *
     * @return The session of this pane.
     */
    Session& getSession();

    /**
     * @brief Applies a changed property of the pane row to the hosted plugin and to the editor.
     *
     * Id::plugin destroys the editor, because the session reloads the plugin.
     * Id::name, when the row holds it, adds the plugin file to the ENAX plugin modules of Nexus when the pane holds the
     * ENAX mark, and clears the cwd property otherwise. Then the pane lays out, creates the editor, and lays out again.
     * Id::focus writes the focus value of the row to the focus parameter of the plugin, when an editor exists. Id::name,
     * when the row holds it, does the same after the editor is created.
     *
     * @param tree      The ValueTree whose property changed.
     * @param property  The identifier of the changed property. Id::focus, Id::plugin and Id::name have an effect.
     */
    void valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property) override;

    /**
     * @brief Creates the editor of the plugin that the session holds for this pane, and mounts it in the pane.
     *
     * The session must hold the plugin. Session::getPlugin() throws std::out_of_range when it does not.
     * When the plugin returns no editor, the pane mounts none. The drop target moves to the front, above the editor.
     */
    void createProcessorEditor();

    /** The hosted plugin editor. It is nullptr when the session does not hold the plugin of this pane. */
    std::unique_ptr<juce::AudioProcessorEditor> editor;

    /** @brief Drop target that covers the pane. It stays above the hosted editor. */
    DropTarget dropTarget;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EditorView)
};
