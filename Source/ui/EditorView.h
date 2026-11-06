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
 * row of the terminal state is written to the hosted plugin through the session.
 * When the shellExited row of the terminal state becomes true, the pane closes itself.
 *
 * A DropTarget covers the pane. A plugin file dropped on the pane splits the pane
 * at the drop edge, and the file loads into the new pane.
 */
class EditorView
    : public jam::PaneComponent
    , public juce::ValueTree::Listener
    , public juce::ApplicationCommandTarget
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
     * @brief Lays out the pane, then places the hosted editor inside it when the session holds the plugin of the pane.
     *
     * The inset is the pane padding plus two outline line thicknesses. A resizable editor takes the pane bounds reduced by the inset.
     * Any other editor sits at the inset position from the top left corner.
     * The function writes no row and no parameter.
     */
    void resized() override;

    /**
     * @brief Lays out the pane again when the hosted editor changes its bounds.
     * @param child  Component whose bounds changed.
     */
    void childBoundsChanged (juce::Component* child) override;

    void lookAndFeelChanged() override;

    /**
     * @brief Returns the first command target among the parent components of this pane.
     * @return The parent command target, or nullptr when no parent is a command target.
     */
    juce::ApplicationCommandTarget* getNextCommandTarget() override;

    /**
     * @brief Adds the id of the one command of the pane, `map::CommandId::closePane`.
     * @param commands  Array that receives the id.
     */
    void getAllCommands (juce::Array<juce::CommandID>& commands) override;

    /**
     * @brief Describes the command by the name of its `map::CommandId` entry.
     * @param commandID  Command to describe.
     * @param result     Info to fill.
     */
    void getCommandInfo (juce::CommandID commandID, juce::ApplicationCommandInfo& result) override;

    /**
     * @brief Closes this pane.
     *
     * When the tab holds more than one pane, the call removes the pane from the tab, then removes its plugin from the session.
     * When the pane is the only pane, the call removes the tab from its session view.
     *
     * @param info  Invocation that carries the command id. Not used.
     * @return Always true.
     */
    bool perform (const InvocationInfo& info) override;

    /**
     * @brief Writes the zoom row of the terminal state of the pane, which the session passes to the plugin.
     *
     * zoomIn adds @p step to the zoom row, zoomOut subtracts it, and zoomReset sets it to 1.0. The row is not clamped.
     * A zoom row of the terminal state that is absent reads as 1.0.
     *
     * @param command  Zoom command: zoomIn, zoomOut or zoomReset.
     * @param step     Zoom step, in normalised units.
     */
    void setZoom (juce::CommandID command, float step);

    /**
     * @brief Writes the mode row of the terminal state of the pane, which the session passes to the plugin.
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
     * @brief Places the hosted editor inside the pane outline.
     *
     * A resizable editor takes the pane bounds reduced by @p inset on each side. Any other editor moves to (inset, inset).
     *
     * @param inset  Distance in pixels between the pane bounds and the editor.
     */
    void setEditorBounds (int inset);

    /**
     * @brief Writes one row of the terminal state to the session of the pane.
     *
     * The call builds a `terminal` tree with one property, then gives it to the session, which passes it to the PluginState of the pane.
     *
     * @param row    Identifier of the row to write.
     * @param value  Value of the row.
     */
    void setRows (const juce::Identifier& row, const juce::var& value);

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
     * @brief Applies a changed property of the pane row or of its terminal state to the hosted plugin and to the editor.
     *
     * A shellExited property that becomes true invokes the closePane command on this pane.
     * Id::plugin destroys the editor, because the session reloads the plugin.
     * Id::name, when the row holds it, adds the plugin file to the ENAX plugin modules of Nexus when the session holds a PluginState
     * for the pane. Then the pane lays out, creates the editor, and lays out again.
     * Id::focus on the pane row writes the focus value of the row to the terminal state of the plugin through setRows().
     * Id::name, when the row holds it, does the same after the editor is created.
     *
     * @param tree      The ValueTree whose property changed.
     * @param property  The identifier of the changed property. Id::shellExited, Id::focus, Id::plugin and Id::name have an effect.
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
