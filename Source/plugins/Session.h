/**
 * @file Session.h
 * @brief One session's hosted-plugin registry — keeps the graph node of each
 *        plugin instance bound to the session's own panes.
 */
#pragma once
#include <JuceHeader.h>
#include "end/ENDModel.h"
#include "generated/Generated.h"
#include "plugins/PluginState.h"

/**
 * @class Session
 * @brief Holds the hosted juce::AudioPluginInstance of each pane in one
 *        session, keyed by the pane's own identity. The audio graph owns the
 *        instances. The session keeps the `juce::AudioProcessorGraph::NodeID` of each instance.
 *        For a plugin of the company, the session also keeps the `PluginState` of the pane, which holds the pluginState rows of that pane.
 *
 * A plugin loads asynchronously. A change of the Id::plugin property of a pane row
 * starts the load of that file, and the instance joins the graph when it is created.
 * The session listens to the sessions row, the parent of the session row.
 * It reacts to a change of the Id::plugin property of a pane row. It does not handle the removal of a tab.
 * TabView::childRemoved() removes the plugins of its panes through removePlugin().
 */
class Session : private juce::ValueTree::Listener
{
public:
    /**
     * @brief Adds the identity parameter to the session row, and listens to the sessions row that holds it.
     * @param newUuid                  Identity of the session.
     * @param newModel                 Model that creates the identity parameter.
     * @param newSessionsState         Sessions row, the parent of the session row. The session listens to it.
     * @param newGraph                 Audio graph that owns the plugin instances.
     * @param newAudioInputId          Identity of the audio input processor in the graph.
     * @param newAudioOutputId         Identity of the audio output processor in the graph.
     * @param newMidiInputId           Identity of the MIDI input processor in the graph.
     * @param newFormatManager         Format manager that creates the plugin instances.
     * @param newKnownPluginList       Known plugin list that receives each scanned plugin file.
     */
    Session (jam::UUID newUuid,
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
    juce::AudioProcessor& getPlugin (jam::UUID uuid);

    /** @brief Answers whether a pane has a bound plugin instance.
     *  @param uuid Identity of the pane.
     *  @return True when a plugin instance is bound to uuid.
     */
    bool contains (jam::UUID uuid) const;

    /**
     * @brief Answers whether a pane has a PluginState. Only a plugin of the company has one.
     * @param uuid Identity of the pane.
     * @return True when a PluginState is held for uuid.
     */
    bool hasPluginState (jam::UUID uuid) const;

    /**
     * @brief Writes the pluginState rows to the PluginState of a pane.
     *
     * The call does nothing when the pane has no PluginState.
     *
     * @param uuid  Identity of the pane.
     * @param rows  Tree whose properties are the pluginState rows to write.
     */
    void setRows (jam::UUID uuid, const juce::ValueTree& rows);

    /**
     * @brief Erases the PluginState of a pane, then removes the plugin instance bound to the pane from the graph.
     *
     * The graph node is removed only when a plugin is bound to @p uuid.
     *
     * @param uuid Identity of the pane whose plugin instance is removed.
     */
    void removePlugin (jam::UUID uuid);

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
    juce::ValueTree state { jam::Format::toType (Id::session) };

private:
    /**
     * @brief Adds a created plugin instance to the graph, and binds it to its pane.
     *
     * The instance enables all its buses. For a plugin of the company, the call creates the PluginState of the pane and
     * writes the pluginState rows of the pane to the plugin before the instance joins the graph.
     * The instance then joins the graph, and connects to the audio and MIDI processors.
     * The graph node is stored under the identity of the pane. The name of the processor is then written to
     * Id::name of the pane row. The pane creates its editor when that property arrives.
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
     * the session and still names the same file. When no plugin format reads the file, the call does nothing.
     * The call asserts when a format reads the file, but the scan finds no plugin type in it.
     *
     * @param paneRow  Pane row whose Id::plugin property names the file.
     */
    void loadPlugin (const juce::ValueTree& paneRow);

    /**
     * @brief Reloads the plugin of a pane when the pane row names another file.
     *
     * The old instance leaves the graph. The Id::name property of the pane row is removed. The pane
     * creates its editor again when the new instance is added. A change of any other property has no effect.
     * A `plugin` row with no value finds no format, so nothing loads and the pane is a blank canvas.
     *
     * @param tree      Tree whose property changed. Only a descendant of the session row is handled.
     * @param property  Identifier of the changed property. Only Id::plugin has an effect.
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

    /** @brief PluginState of each hosted plugin of the company, keyed by the identity of its pane. addPlugin() creates it and removePlugin() erases it. */
    jam::HashMap<jam::UUID, std::unique_ptr<PluginState>> pluginStates;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Session)
};
