#include "plugins/Session.h"
#include "config/ConfigDirectory.h"

Session::Session (jam::UUID newUuid,
                  ENDModel& newModel,
                  juce::AudioProcessorGraph& newGraph,
                  juce::AudioProcessorGraph::NodeID newAudioInputNodeId,
                  juce::AudioProcessorGraph::NodeID newAudioOutputNodeId,
                  juce::AudioProcessorGraph::NodeID newMidiInputNodeId,
                  juce::AudioPluginFormatManager& newFormatManager)
    : model (newModel),
      graph (newGraph),
      audioInputNodeId (newAudioInputNodeId),
      audioOutputNodeId (newAudioOutputNodeId),
      midiInputNodeId (newMidiInputNodeId),
      formatManager (newFormatManager)
{
    model.createAndAddParameter<jam::Parameter<int64_t>> (state, Id::id, newUuid.value);
    state.addListener (this);
}

Session::~Session()
{
    state.removeListener (this);
}

juce::AudioProcessor& Session::getPlugin (jam::UUID uuid) { return *graph.getNodeForId (plugins.at (uuid))->getProcessor(); }

bool Session::contains (jam::UUID uuid) const { return plugins.contains (uuid); }

void Session::valueTreeChildAdded (juce::ValueTree&, juce::ValueTree& child)
{
    const jam::UUID uuid { child.getProperty (Id::id) };

    if (child.hasType (Id::toType (Id::pane)) and not contains (uuid))
        newPlugin (uuid);
}

void Session::newPlugin (jam::UUID uuid)
{
    auto paneRow { jam::Model::getChildWithID (state, juce::var (uuid.value)) };
    jassert (paneRow.isValid());

    const auto file { ConfigDirectory::Config::getPath (Id::plugins).getChildFile (files::evePlugin) };
    juce::OwnedArray<juce::PluginDescription> descriptions;

    for (auto* format : formatManager.getFormats())
        if (format->fileMightContainThisPluginType (file.getFullPathName()))
            format->findAllTypesForFile (descriptions, file.getFullPathName());

    jassert (descriptions.size() > 0);

    if (descriptions.size() > 0)
    {
        juce::String errorMessage;
        auto instance { formatManager.createPluginInstance (*descriptions.getFirst(), graph.getSampleRate(), graph.getBlockSize(), errorMessage) };
        jassert (instance != nullptr);

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

            paneRow.setProperty (Id::pluginId, files::evePlugin, nullptr);
        }
    }
}

void Session::removePlugin (jam::UUID uuid)
{
    if (plugins.contains (uuid))
    {
        graph.removeNode (plugins.at (uuid));
        plugins.erase (uuid);
    }
}
