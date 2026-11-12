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
 * the constructor. The name branch creates the editor, then lays out the pane.
 * addPlugin() sets Id::name after it binds the plugin, so the
 * session holds the plugin at that time. When the Id::plugin of the pane row changes,
 * the editor is destroyed.
 * When the shellExited property of the pluginState changes to true, the pane closes itself.
 *
 * A DropTarget covers the pane. A drag over the pane focuses it. A plugin file dropped on the pane
 * splits the focused pane at the drop edge, and the file loads into the new pane.
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
     * @brief Lays out the pane, then places the hosted editor inside it.
     *
     * The inset is the pane padding of the look and feel plus two outline line thicknesses, rounded to whole pixels.
     * When Session::contains() reports the pane, the editor bounds are the local bounds reduced by the inset, and the editor takes them when it exists.
     * One call to Session::setRows() then writes the width and the height of the editor bounds to the pluginState.
     */
    void resized() override;

    /** @brief Lays out the pane again, because the inset of the hosted editor comes from the pane padding of the look and feel. */
    void lookAndFeelChanged() override;

    /**
     * @brief Returns the first command target among the parent components of this pane.
     * @return The parent command target, or nullptr when no parent is a command target.
     */
    juce::ApplicationCommandTarget* getNextCommandTarget() override;

    /**
     * @brief Adds the ids of the three close commands of the pane.
     *
     * The ids are `map::CommandId::closeTerminal`, `map::CommandId::closePane` and `map::CommandId::closeTerminalPane`.
     * @param commands  Array that receives the ids.
     */
    void getAllCommands (juce::Array<juce::CommandID>& commands) override;

    /**
     * @brief Describes the command by the name of its `map::CommandId` entry.
     * @param commandID  Command to describe.
     * @param result     Info to fill.
     */
    void getCommandInfo (juce::CommandID commandID, juce::ApplicationCommandInfo& result) override;

    /**
     * @brief Runs the close command of the invocation on this pane.
     *
     * `closeTerminal` removes the `plugin` property of the pane row. The session then removes the plugin.
     * `closePane` removes the pane from its tab. It uses the nearest jam::OwnerComponent parent.
     * `closeTerminalPane` invokes `closeTerminal`, then `closePane`.
     *
     * @param info  Invocation that carries the command id.
     * @return Always true.
     */
    bool perform (const InvocationInfo& info) override;

    /**
     * @brief Writes the zoom row of the pluginState of the pane, which the session passes to the plugin.
     *
     * zoomIn adds @p step to the zoom row, zoomOut subtracts it, and zoomReset sets it to 1.0. The row is not clamped.
     * A zoom row of the pluginState that is absent reads as 1.0.
     *
     * @param command  Zoom command: zoomIn, zoomOut or zoomReset.
     * @param step     Zoom step that zoomIn adds and zoomOut subtracts.
     */
    void setZoom (juce::CommandID command, float step);

    /**
     * @brief Writes the mode row of the pluginState of the pane, which the session passes to the plugin.
     * @param newMode  Editor mode, a constant of `map::EditorMode`.
     */
    void setMode (int newMode);

    /**
     * @brief Closes the markdown file of the pane when one is shown, otherwise runs the command on this pane.
     *
     * A non-empty `markdownFile` property of the pluginState gives an empty `markdownFile` row. Otherwise the call invokes @p command on this pane.
     *
     * @param command  Close command to invoke when no markdown file is shown.
     */
    void close (juce::CommandID command);

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
         * @brief Splits the focused pane of the matrix at the drop edge, then writes the first dropped file to the plugin property of the new pane.
         *
         * The drop edge comes from PaneComponent::getEdge(). The pane is divided by its two diagonals into four triangles,
         * and the drop edge is the triangle that holds the drop point. The matrix splits its focused pane, and
         * fileDragEnter() focuses the pane under the drag.
         *
         * @param files  Dropped files. The first file is loaded.
         * @param x      Horizontal drop position in the target.
         * @param y      Vertical drop position in the target.
         */
        void filesDropped (const juce::StringArray& files, int x, int y) override;
    };

    /**
     * @brief Places the hosted editor at the editor bounds of the pane.
     *
     * A resizable editor takes @p bounds as its bounds. Any other editor moves its top left corner to the position of @p bounds.
     * The call asserts that the editor exists.
     *
     * @param bounds  Editor bounds in the coordinates of the pane, as the local bounds reduced by the inset give them.
     */
    void setEditorBounds (juce::Rectangle<int> bounds);

    /**
     * @brief Writes one row of the pluginState to the session of the pane.
     *
     * The call gives the row, its value and the identity of the pane to Session::setRow(). It builds no tree itself.
     * The session does nothing when the pane has no PluginState.
     *
     * @param row    Identifier of the row to write.
     * @param value  Value of the row.
     */
    void setRow (const juce::Identifier& row, const juce::var& value);

    /**
     * @brief Returns the session that holds the plugin of this pane.
     *
     * The session is found through the ancestor session row of the pane row, and its identity selects it in Nexus.
     * The call asserts that the pane lies under a session row.
     *
     * @return The session of this pane.
     */
    Session& getSession() const;

    /**
     * @brief Fills events with one handler for each of Id::shellExited, Id::plugin and Id::name.
     *
     * The Id::shellExited handler invokes closeTerminalPane on this pane, asynchronously, when the value is true.
     * The Id::plugin handler destroys the editor.
     * The Id::name handler runs when the changed tree holds Id::name. When the session holds a PluginState for that pane,
     * it adds the plugin file to the ENAX plugin modules of Nexus. Then it creates the editor and lays out the pane.
     */
    void registerEvents();

    /**
     * @brief Runs the handler that registerEvents() stored for the changed property.
     *
     * A property with no handler does nothing.
     *
     * @param tree      The ValueTree whose property changed.
     * @param property  The identifier of the changed property. Id::shellExited, Id::plugin and Id::name have an effect.
     */
    void valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property) override;

    /**
     * @brief Creates the editor of the plugin that the session holds for this pane, and mounts it in the pane.
     *
     * The session must hold the plugin. Session::getPlugin() looks the plugin up with `at` and requires contains() to be true first.
     * When the plugin returns no editor, the pane mounts none. The drop target moves to the front, above the editor.
     */
    void createProcessorEditor();

    /** The hosted plugin editor. It is nullptr until createProcessorEditor() creates one. It is also nullptr after a change of the plugin row, and when the plugin returns no editor. */
    std::unique_ptr<juce::AudioProcessorEditor> editor;

    /** @brief Handlers of the pane row, keyed by the changed property. registerEvents() fills them, and valueTreePropertyChanged() runs them. */
    jam::Function::Map<juce::Identifier, jam::Function::pointerCapacity, void (juce::ValueTree&)> events;

    /** @brief Drop target that covers the pane. It stays above the hosted editor. */
    DropTarget dropTarget;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EditorView)
};
