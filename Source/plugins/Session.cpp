#include "plugins/Session.h"
#include "config/ConfigDirectory.h"

Session::Session (jam::UUID newUuid,
                  ENDModel& newModel,
                  juce::AudioProcessorGraph& newGraph,
                  juce::AudioProcessorGraph::NodeID newAudioInputId,
                  juce::AudioProcessorGraph::NodeID newAudioOutputId,
                  juce::AudioProcessorGraph::NodeID newMidiInputId,
                  juce::AudioPluginFormatManager& newFormatManager)
    : model (newModel),
      graph (newGraph),
      audioInputId (newAudioInputId),
      audioOutputId (newAudioOutputId),
      midiInputId (newMidiInputId),
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
        addPlugin (child);
}

void Session::addPlugin (juce::ValueTree& paneRow)
{
    const jam::UUID uuid { paneRow.getProperty (Id::id) };

    const auto file { ConfigDirectory::Config::getPath (Id::plugins).getChildFile (files::evePlugin) };
    const auto formats (formatManager.getFormats());
    const auto pluginFormat { std::find_if (formats.begin(), formats.end(), [&file] (juce::AudioPluginFormat* format)
    {
        return format->fileMightContainThisPluginType (file.getFullPathName());
    }) };
    jassert (pluginFormat != formats.end());

    juce::PluginDescription description;
    description.fileOrIdentifier = file.getFullPathName();
    description.pluginFormatName = (*pluginFormat)->getName();

    juce::String errorMessage;
    auto instance { formatManager.createPluginInstance (description, graph.getSampleRate(), graph.getBlockSize(), errorMessage) };
    jassert (instance != nullptr);

    if (instance != nullptr)
    {
        instance->enableAllBuses();

        auto plugin { graph.addNode (std::move (instance)) };
        jassert (plugin != nullptr);

        const auto [entry, inserted] { plugins.try_emplace (uuid, plugin->nodeID) };
        jassert (inserted);

        paneRow.setProperty (Id::name, plugin->getProcessor()->getName(), nullptr);

        auto* processor { plugin->getProcessor() };
        const auto inputChannels { std::min (processor->getTotalNumInputChannels(),
                                             graph.getNodeForId (audioInputId)->getProcessor()->getTotalNumOutputChannels()) };
        const auto outputChannels { std::min (processor->getTotalNumOutputChannels(),
                                              graph.getNodeForId (audioOutputId)->getProcessor()->getTotalNumInputChannels()) };

        for (int channel { 0 }; channel < inputChannels; ++channel)
            graph.addConnection ({ { audioInputId, channel }, { plugin->nodeID, channel } });

        for (int channel { 0 }; channel < outputChannels; ++channel)
            graph.addConnection ({ { plugin->nodeID, channel }, { audioOutputId, channel } });

        if (processor->acceptsMidi())
            graph.addConnection ({ { midiInputId, juce::AudioProcessorGraph::midiChannelIndex },
                                   { plugin->nodeID, juce::AudioProcessorGraph::midiChannelIndex } });
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
