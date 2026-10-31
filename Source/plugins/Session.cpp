#include "plugins/Session.h"

Session::Session (jam::UUID newUuid,
                  ENDModel& newModel,
                  juce::AudioProcessorGraph& newGraph,
                  juce::AudioProcessorGraph::NodeID newAudioInputNodeId,
                  juce::AudioProcessorGraph::NodeID newAudioOutputNodeId,
                  juce::AudioProcessorGraph::NodeID newMidiInputNodeId)
    : model (newModel),
      graph (newGraph),
      audioInputNodeId (newAudioInputNodeId),
      audioOutputNodeId (newAudioOutputNodeId),
      midiInputNodeId (newMidiInputNodeId)
{
    model.createAndAddParameter<jam::Parameter<int64_t>> (state, Id::id, newUuid.value);
}

Session::~Session() = default;

juce::AudioProcessor& Session::getPlugin (jam::UUID uuid) { return *graph.getNodeForId (plugins.at (uuid))->getProcessor(); }

bool Session::contains (jam::UUID uuid) const { return plugins.contains (uuid); }

void Session::newPlugin (jam::UUID uuid, const juce::String& pluginId, std::unique_ptr<juce::AudioPluginInstance> instance)
{
    auto paneRow { jam::Model::getChildWithID (state, juce::var (uuid.value)) };
    jassert (paneRow.isValid());

    if (instance != nullptr)
    {
        instance->enableAllBuses();

        auto node { graph.addNode (std::move (instance)) };
        jassert (node != nullptr);

        const auto [entry, inserted] { plugins.try_emplace (uuid, node->nodeID) };
        jassert (inserted);

        paneRow.setProperty (Id::name, node->getProcessor()->getName(), nullptr);

        auto* processor { node->getProcessor() };
        const auto inputChannels { std::min (processor->getTotalNumInputChannels(),
                                             graph.getNodeForId (audioInputNodeId)->getProcessor()->getTotalNumOutputChannels()) };
        const auto outputChannels { std::min (processor->getTotalNumOutputChannels(),
                                              graph.getNodeForId (audioOutputNodeId)->getProcessor()->getTotalNumInputChannels()) };

        for (int channel { 0 }; channel < inputChannels; ++channel)
            graph.addConnection ({ { audioInputNodeId, channel }, { node->nodeID, channel } });

        for (int channel { 0 }; channel < outputChannels; ++channel)
            graph.addConnection ({ { node->nodeID, channel }, { audioOutputNodeId, channel } });

        if (processor->acceptsMidi())
            graph.addConnection ({ { midiInputNodeId, juce::AudioProcessorGraph::midiChannelIndex },
                                   { node->nodeID, juce::AudioProcessorGraph::midiChannelIndex } });
    }

    paneRow.setProperty (Id::pluginId, pluginId, nullptr);
}

void Session::removePlugin (jam::UUID uuid)
{
    if (plugins.contains (uuid))
    {
        graph.removeNode (plugins.at (uuid));
        plugins.erase (uuid);
    }
}
