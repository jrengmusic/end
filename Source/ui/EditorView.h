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
 * The editor is created when the pane row receives Id::name. initialise(), which each constructor runs,
 * creates it only when the row already holds Id::name. The name branch creates the editor, then lays out the pane.
 * addPlugin() sets Id::name after it binds the plugin, so the
 * session holds the plugin at that time. When the Id::plugin of the pane row changes,
 * the editor is destroyed.
 * The server attaches the tree of the plugin under the plugin row of the pane. When that tree arrives,
 * valueTreeChildAdded() writes the cwd of the pane row to the terminal row, then removes it from the pane row. It then
 * writes the focus of the pane row to the terminal row, and writes the size through resized().
 * When the Id::shellExited property of a tree under the pane row changes to true, the pane closes itself.
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
     * @brief Builds the pane row under the tab row, or matches the row with the same identity, then runs initialise().
     *
     * When the cwd of the sessions row is not empty, the call writes that cwd to the cwd of the pane row. The sessions row is
     * the grandparent of the tab row. valueTreeChildAdded() moves the cwd to the terminal row when the plugin tree attaches.
     *
     * @param model     Model that owns the pane parameters.
     * @param tabState  Tree of the tab that holds the pane row.
     * @param uuid      Identity of the pane row.
     */
    EditorView (jam::Model& model, juce::ValueTree tabState, jam::UUID uuid);

    /**
     * @brief Adopts an existing pane row, then runs initialise(). The call writes no cwd.
     *
     * @param model  Model that owns the pane parameters.
     * @param row    Existing pane row.
     */
    EditorView (jam::Model& model, const juce::ValueTree& row);

    /** @brief Stops listening to the pane row and tells the processor that its editor is being deleted. */
    ~EditorView() override;

    /**
     * @brief Lays out the pane, sets the bounds of the drop target, and writes the inset size to the attached terminal row.
     *
     * The call lays out the pane through jam::PaneComponent and sets the bounds of the drop target to the local bounds of the pane.
     * The inset is the pane padding of the look and feel plus two outline line thicknesses, rounded to whole pixels.
     * When Session::contains() reports the pane, the call writes the width and height of the inset area to the terminal row of
     * the attached plugin, when that row exists. When the pane also holds an editor, the editor bounds are the inset area.
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
     * @brief Writes the editor mode to the Id::mode property of the terminal row of the attached plugin, when that row exists.
     * @param mode  Editor mode, one value of map::EditorMode.
     */
    void setMode (int mode);

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
     * @brief Registers the events, listens to the pane row, and mounts the drop target.
     *
     * When the pane row already holds Id::name, the call registers the plugin file of the row as an ENAX plugin module
     * of Nexus, then creates the editor.
     */
    void initialise();

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
     * The Id::shellExited handler reads the changed tree, which is a tree under the pane row. When its value is true,
     * the handler invokes closeTerminalPane on this pane, asynchronously.
     * The Id::plugin handler destroys the editor. It acts only when the changed tree is the pane row itself.
     * The Id::name handler acts only when the changed tree is the pane row itself, and the row holds Id::name. It adds the
     * plugin file of the row to the ENAX plugin modules of Nexus, creates the editor, and then runs resized() to lay out the pane.
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
     * @brief Runs when the plugin row of this pane gains a child. The child is the attached plugin tree.
     *
     * When the attached tree holds a terminal row, the call writes the cwd of the pane row to the terminal row, when the pane
     * row holds a cwd, and removes that cwd from the pane row. It then writes the focus of the pane row to the terminal row,
     * and runs resized(), which writes the size to the terminal row. Any other child has no effect.
     *
     * @param parentTree  Tree that gained the child. Only the plugin row of this pane acts.
     * @param child       Added child, the attached plugin tree.
     */
    void valueTreeChildAdded (juce::ValueTree& parentTree, juce::ValueTree& child) override;

    /**
     * @brief Creates the editor of the plugin that the session holds for this pane, and mounts it in the pane.
     *
     * The session must hold the plugin. The call does not check it, because Session::getPlugin() looks the plugin up with `at`.
     * The plugin creates the editor through juce::AudioProcessor::createEditorAndMakeActive(). That call returns nullptr when
     * the plugin returns no editor, and also when the plugin already has an active editor. The result replaces `editor`, so a
     * previously held editor is destroyed, also when the result is nullptr. The drop target then moves to the front, above the editor.
     * After that, when `editor` is not null, the pane is showing, and `Id::focus` is true, `editor` grabs keyboard focus.
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
