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
 * @brief Owns the hosted juce::AudioPluginInstance for each pane in one
 *        session, keyed by the pane's own identity.
 */
class Session : private juce::ValueTree::Listener
{
public:
    Session (jam::UUID newUuid,
             ENDModel& newModel,
             juce::AudioProcessorGraph& newGraph,
             juce::AudioProcessorGraph::NodeID newAudioInputNodeId,
             juce::AudioProcessorGraph::NodeID newAudioOutputNodeId,
             juce::AudioProcessorGraph::NodeID newMidiInputNodeId,
             juce::AudioPluginFormatManager& newFormatManager);

    ~Session();

    /** @brief Resolves the hosted plugin instance bound to a pane.
     *  @param uuid Identity of the pane the plugin is bound to.
     *  @return The bound plugin instance.
     */
    juce::AudioProcessor& getPlugin (jam::UUID uuid);

    /** @brief Answers whether a pane has a bound plugin instance.
     *  @param uuid Identity of the pane.
     *  @return True when a plugin instance is bound to uuid.
     */
    bool contains (jam::UUID uuid) const;

    /** @brief Releases the plugin instance bound to a pane.
     *  @param uuid Identity of the pane whose plugin instance is released.
     */
    void removePlugin (jam::UUID uuid);

    juce::ValueTree state { Id::toType (Id::session) };

private:
    void newPlugin (jam::UUID uuid);

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
