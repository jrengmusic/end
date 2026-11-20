/**
 * @file Session.h
 * @brief One session's hosted-plugin registry — keeps the graph node of each
 *        plugin instance bound to the session's own panes.
 */
#pragma once
#include <JuceHeader.h>
#include "end/ENDModel.h"
#include "generated/Generated.h"

/**
 * @class Session
 * @brief Holds the hosted juce::AudioPluginInstance of each pane in one
 *        session, keyed by the pane's own identity. The audio graph owns the
 *        instances. The session keeps the `juce::AudioProcessorGraph::NodeID` of each instance.
 *
 * A plugin loads asynchronously. A change of the Id::plugin property of a pane row
 * starts the load of that file, and the instance joins the graph when it is created.
 * The session listens to the sessions row, the parent of the session row.
 * It reacts to a change of the Id::plugin and Id::focus properties of a pane row, and to a change of the Id::cwd property
 * of a terminal row under the session.
 * The session owns no connection. The plugin connects to the server of END, and the server attaches the tree
 * of the plugin under the plugin row of the pane.
 * TabView::childRemoved() removes the plugins of its panes through removePlugin().
 */
class Session : private juce::ValueTree::Listener
{
public:
    /**
     * @brief Adopts the session row. It binds state to the row, creates the identity parameter from the row, registers the
     *        events, listens to the sessions row, then starts the load of the plugin of each pane row under the session row.
     *
     * @param row                 Session row to adopt.
     * @param newModel            Model that creates the identity parameter of the row.
     * @param newSessionsState    Sessions row, the parent of the session row.
     * @param newGraph            Audio graph that owns the plugin instances.
     * @param newAudioInputId     Identity of the audio input processor in newGraph.
     * @param newAudioOutputId    Identity of the audio output processor in newGraph.
     * @param newMidiInputId      Identity of the MIDI input processor in newGraph.
     * @param newFormatManager    Format manager that creates the plugin instances.
     * @param newKnownPluginList  Known plugin list that receives the scanned plugin files.
     */
    Session (const juce::ValueTree& row,
             ENDModel& newModel,
             juce::ValueTree newSessionsState,
             juce::AudioProcessorGraph& newGraph,
             juce::AudioProcessorGraph::NodeID newAudioInputId,
             juce::AudioProcessorGraph::NodeID newAudioOutputId,
             juce::AudioProcessorGraph::NodeID newMidiInputId,
             juce::AudioPluginFormatManager& newFormatManager,
             juce::KnownPluginList& newKnownPluginList);

    /** @brief Stops listening to the sessions row. */
    ~Session();

    /**
     * @brief Returns the hosted plugin instance bound to a pane.
     *
     * The pane must hold a bound plugin. Check it with contains() first.
     *
     * @param uuid Identity of the pane the plugin is bound to.
     * @return The bound plugin instance.
     */
    juce::AudioPluginInstance& getPlugin (jam::UUID uuid);

    /** @brief Answers whether a pane has a bound plugin instance.
     *  @param uuid Identity of the pane.
     *  @return True when a plugin instance is bound to uuid.
     */
    bool contains (jam::UUID uuid) const;

    /**
     * @brief Removes the graph node of the plugin bound to a pane, then forgets its identity.
     *
     * The call does nothing when no plugin is bound to @p uuid. The graph releases the instance when it removes the node.
     *
     * @param uuid Identity of the pane whose plugin instance is removed.
     */
    void removePlugin (jam::UUID uuid);

    /**
     * @brief Writes the state of each bound plugin onto its pane row in a sessions row, as binary data in Id::state.
     *
     * The pane row is found by identity through Model::getChildWithID(). The Id::plugin child of the pane row holds the live
     * replica of the plugin, so the call removes it from the sessions row. The call also removes the Id::name property of the pane row.
     * Nexus::getSessionsState() calls it on a copy of the sessions row.
     *
     * @param sessions  Sessions row that receives the states.
     */
    void setPluginStates (juce::ValueTree sessions) const;

    /**
     * @brief Returns the terminal row of the plugin replica under a pane row.
     *
     * The replica is the first child of the plugin row of the pane, and the call returns its terminal child.
     * The call returns an invalid tree when the pane row has no plugin row, or the replica has no terminal child.
     *
     * @param paneRow  Pane row whose plugin replica is read.
     * @return The terminal row, or an invalid tree.
     */
    static juce::ValueTree getTerminal (const juce::ValueTree& paneRow);

    /**
     * @brief Returns the first plugin format of a manager that can read a plugin file.
     * @param formatManager  Manager whose formats are asked, in their order.
     * @param file           Plugin file to match.
     * @return The first format whose fileMightContainThisPluginType() accepts the file path, or nullptr when none does.
     */
    static juce::AudioPluginFormat* getPluginFormat (const juce::AudioPluginFormatManager& formatManager, const juce::File& file);

    /**
     * @brief Returns the first plugin format of the session's format manager that can read a plugin file.
     * @param file  Plugin file to match.
     * @return The format, or nullptr when none reads the file.
     */
    juce::AudioPluginFormat* getPluginFormat (const juce::File& file) const;

    /** @brief Tree of the session row. Its identity parameter holds the session identity. */
    juce::ValueTree state;

private:
    /**
     * @brief Adds a created plugin instance to the graph under the identity of its pane, then connects it to the audio and MIDI processors.
     *
     * The call enables all buses of the instance. When the pane row holds Id::state, the call restores that state into the
     * instance, then removes the property. The graph then adds the node. The node is stored under the identity of the pane.
     * When the instance returns a VST2 client and the client returns a chunk with an Id::id, the call appends a held row
     * under the plugin row of the pane. The held row has the type of the chunk and holds only that Id::id. The server replaces
     * the held row with the full tree of the plugin when the plugin connects.
     * The name of the processor is then written to Id::name of the pane row. The pane creates its editor when that property arrives.
     *
     * @param paneRow   Pane row that receives the instance.
     * @param instance  Created plugin instance. The graph takes ownership.
     */
    void addPlugin (juce::ValueTree paneRow, std::unique_ptr<juce::AudioPluginInstance> instance);

    /**
     * @brief Connects a plugin node to the audio and MIDI processors of the graph.
     *
     * Each audio side connects the lower of the two channel counts. The MIDI input connects only when the plugin accepts MIDI.
     *
     * @param pluginId  Graph node of the plugin.
     */
    void addConnections (juce::AudioProcessorGraph::NodeID pluginId);

    /**
     * @brief Starts the load of the plugin file that a pane row names.
     *
     * The file is scanned into the known plugin list. A file that is listed with an unchanged modification time is not tested again.
     * The instance is created asynchronously. addPlugin() runs when it arrives, and only when the pane row still belongs to
     * the session and still names the same file. When no plugin format reads the file, the call does nothing. When the scan finds no plugin type in the file, or the instance is not created, the call writes the failure to the overlay message of the model (ENDModel::setMessage()).
     *
     * @param paneRow  Pane row whose Id::plugin property names the file.
     */
    void loadPlugin (const juce::ValueTree& paneRow);

    /**
     * @brief Starts the load of each pane row in the subtree of a row that has an Id::plugin property.
     *
     * The call visits every descendant of @p row. The constructor calls it with the session row.
     *
     * @param row  Root of the subtree to visit.
     */
    void loadPlugins (const juce::ValueTree& row);

    /**
     * @brief Fills events with one handler for each of Id::plugin, Id::focus and Id::cwd.
     *
     * The Id::plugin handler acts on a pane row under the session row. It removes the plugin of the pane, removes the Id::name
     * property of the pane row, removes the plugin row of the pane, then starts the load of the file that the pane row names.
     * The load runs only when a plugin format reads that file.
     * The Id::focus handler acts on a pane row under the session row. It writes the focus value to the terminal row of the pane.
     * When the pane is focused, it also writes the cwd of that terminal row to the sessions row.
     * The Id::cwd handler acts on a terminal row under the session row. When the pane of that row is focused, it writes the cwd
     * of the row to the sessions row.
     */
    void registerEvents();

    /**
     * @brief Runs the handler that registerEvents() stored for the changed property.
     *
     * A property with no handler does nothing. The Id::plugin and Id::focus handlers act on a pane row under the session row.
     * The Id::cwd handler acts on a terminal row under the session row.
     *
     * @param tree      Tree whose property changed.
     * @param property  Identifier of the changed property. Id::plugin, Id::focus and Id::cwd have an effect.
     */
    void valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property) override;

    /** @brief Sessions row, the parent of the session row. The session listens to it. */
    juce::ValueTree sessionsState;

    /** @brief Model that creates the identity parameter of the session row. */
    ENDModel& model;

    /** @brief Audio graph that owns the plugin instances. */
    juce::AudioProcessorGraph& graph;

    /** @brief Identity of the audio input processor in the graph. addPlugin() connects each plugin to it. */
    const juce::AudioProcessorGraph::NodeID audioInputId;

    /** @brief Identity of the audio output processor in the graph. addPlugin() connects each plugin to it. */
    const juce::AudioProcessorGraph::NodeID audioOutputId;

    /** @brief Identity of the MIDI input processor in the graph. addPlugin() connects it to each plugin that accepts MIDI. */
    const juce::AudioProcessorGraph::NodeID midiInputId;

    /** @brief Format manager that creates the plugin instances. */
    juce::AudioPluginFormatManager& formatManager;

    /** @brief Known plugin list that loadPlugin() scans each plugin file into. Nexus owns it. */
    juce::KnownPluginList& knownPluginList;

    /** @brief Identity of each hosted plugin in the graph, keyed by the identity of its pane. */
    jam::HashMap<jam::UUID, juce::AudioProcessorGraph::NodeID> plugins;

    /** @brief Handlers keyed by the changed property. registerEvents() fills them. valueTreePropertyChanged() calls the handler of the changed property with the tree. */
    jam::Function::Map<juce::Identifier, jam::Function::pointerCapacity, void (juce::ValueTree&)> events;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Session)
};
