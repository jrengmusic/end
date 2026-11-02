/**
 * @file Session.h
 * @brief One session's hosted-plugin registry — owns every plugin instance
 *        bound to the session's own panes.
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
 */
class Session : private juce::ValueTree::Listener
{
public:
    /**
     * @brief Creates the session row with its identity parameter, and listens to it.
     * @param newUuid                     Identity of the session.
     * @param newModel                    Model that creates the identity parameter.
     * @param newGraph                    Audio graph that owns the plugin instances.
     * @param newAudioInputNodeId         Identity of the audio input processor in the graph.
     * @param newAudioOutputNodeId        Identity of the audio output processor in the graph.
     * @param newMidiInputNodeId          Identity of the MIDI input processor in the graph.
     * @param newFormatManager            Format manager that creates the plugin instances.
     */
    Session (jam::UUID newUuid,
             ENDModel& newModel,
             juce::AudioProcessorGraph& newGraph,
             juce::AudioProcessorGraph::NodeID newAudioInputNodeId,
             juce::AudioProcessorGraph::NodeID newAudioOutputNodeId,
             juce::AudioProcessorGraph::NodeID newMidiInputNodeId,
             juce::AudioPluginFormatManager& newFormatManager);

    /** @brief Stops listening to the session row. */
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
     * @brief Removes the plugin instance bound to a pane from the graph.
     *
     * The call does nothing when no plugin is bound to @p uuid.
     *
     * @param uuid Identity of the pane whose plugin instance is removed.
     */
    void removePlugin (jam::UUID uuid);

    /**
     * @brief Calls a function with the processor of each hosted plugin of the session.
     * @tparam Function  Callable taking the processor as `juce::AudioProcessor&`.
     * @param function   Callable applied to each processor.
     */
    template <typename Function>
    void applyToPlugins (Function&& function)
    {
        for (const auto& [uuid, nodeId] : plugins)
            function (*graph.getNodeForId (nodeId)->getProcessor());
    }

    /** @brief Tree of the session row. Its identity parameter holds the session identity. */
    juce::ValueTree state { Id::toType (Id::session) };

private:
    /**
     * @brief Creates the hosted plugin of a pane and connects it to the graph.
     *
     * The plugin file is `files::evePlugin` in the plugins folder. The first format of the format manager that accepts the file creates the instance.
     * The plugin takes the audio input and output channels that both sides have, and the MIDI input when it accepts MIDI.
     * The name of the plugin is written to the pane row.
     *
     * @param uuid  Identity of the pane that receives the plugin.
     */
    void newPlugin (jam::UUID uuid);

    /**
     * @brief Creates the hosted plugin of each pane row that is added to the session.
     *
     * A pane that the session already holds gets no new plugin.
     *
     * @param parent  Parent of the added child. Not used.
     * @param child   Added child row.
     */
    void valueTreeChildAdded (juce::ValueTree& parent, juce::ValueTree& child) override;

    ENDModel& model;
    juce::AudioProcessorGraph& graph;
    const juce::AudioProcessorGraph::NodeID audioInputNodeId;
    const juce::AudioProcessorGraph::NodeID audioOutputNodeId;
    const juce::AudioProcessorGraph::NodeID midiInputNodeId;
    juce::AudioPluginFormatManager& formatManager;

    jam::HashMap<jam::UUID, juce::AudioProcessorGraph::NodeID> plugins;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Session)
};
