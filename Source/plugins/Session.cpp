#include "plugins/Session.h"

Session::Session (jam::UUID newUuid,
                  ENDModel& newModel,
                  juce::ValueTree newSessionsState,
                  juce::AudioProcessorGraph& newGraph,
                  juce::AudioProcessorGraph::NodeID newAudioInputId,
                  juce::AudioProcessorGraph::NodeID newAudioOutputId,
                  juce::AudioProcessorGraph::NodeID newMidiInputId,
                  juce::AudioPluginFormatManager& newFormatManager,
                  juce::KnownPluginList& newKnownPluginList)
    : sessionsState (newSessionsState),
      model (newModel),
      graph (newGraph),
      audioInputId (newAudioInputId),
      audioOutputId (newAudioOutputId),
      midiInputId (newMidiInputId),
      formatManager (newFormatManager),
      knownPluginList (newKnownPluginList)
{
    model.createAndAddParameter<jam::Parameter<int64_t>> (state, Id::id, newUuid.value);
    sessionsState.addListener (this);
}

Session::~Session()
{
    sessionsState.removeListener (this);
}

juce::AudioProcessor& Session::getPlugin (jam::UUID uuid) { return *graph.getNodeForId (plugins.at (uuid))->getProcessor(); }

bool Session::contains (jam::UUID uuid) const { return plugins.contains (uuid); }

bool Session::hasPluginState (jam::UUID uuid) const { return pluginStates.contains (uuid); }

void Session::setRows (jam::UUID uuid, const juce::ValueTree& rows)
{
    if (pluginStates.contains (uuid))
        pluginStates.at (uuid)->setRows (rows);
}

juce::AudioPluginFormat* Session::getPluginFormat (const juce::AudioPluginFormatManager& formatManager, const juce::File& file)
{
    const auto formats (formatManager.getFormats());
    const auto pluginFormat { std::find_if (formats.begin(), formats.end(), [&file] (juce::AudioPluginFormat* format)
    {
        return format->fileMightContainThisPluginType (file.getFullPathName());
    }) };

    if (pluginFormat != formats.end()) return *pluginFormat;
    return nullptr;
}

juce::AudioPluginFormat* Session::getPluginFormat (const juce::File& file) const
{
    return getPluginFormat (formatManager, file);
}

void Session::valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property)
{
    if (property == Id::plugin and tree.isAChildOf (state))
    {
        const jam::UUID uuid { tree.getProperty (Id::id) };

        removePlugin (uuid);
        tree.removeProperty (Id::name, nullptr);
        loadPlugin (tree);
    }
}

void Session::loadPlugin (const juce::ValueTree& paneRow)
{
#if JAM_USING_PROFILER
    const jam::Profiler::Zone zone { Id::sessionPlugin };
#endif
    const juce::File file { paneRow.getProperty (Id::plugin).toString() };
    auto* format { getPluginFormat (file) };

    if (format != nullptr)
    {
        juce::OwnedArray<juce::PluginDescription> typesFound;
        knownPluginList.scanAndAddFile (file.getFullPathName(), true, typesFound, *format);

        if (not typesFound.isEmpty())
        {
            formatManager.createPluginInstanceAsync (*typesFound.getFirst(), graph.getSampleRate(), graph.getBlockSize(),
                                                     [this, paneRow, file] (std::unique_ptr<juce::AudioPluginInstance> instance, const juce::String& error)
            {
                if (instance == nullptr)
                    model.setMessage (file.getFullPathName() + Id::diagnosticSeparator + error);
                else if (paneRow.isAChildOf (state)
                         and juce::File { paneRow.getProperty (Id::plugin).toString() } == file)
                    addPlugin (paneRow, std::move (instance));
            });
        }
        else
        {
            model.setMessage (file.getFullPathName());
        }
    }
}

void Session::addPlugin (juce::ValueTree paneRow, std::unique_ptr<juce::AudioPluginInstance> instance)
{
#if JAM_USING_PROFILER
    const jam::Profiler::Zone zone { Id::graphNode };
#endif
    const jam::UUID uuid { paneRow.getProperty (Id::id) };
    instance->enableAllBuses();

    if (instance->getPluginDescription().manufacturerName.compare (ProjectInfo::companyName) == 0)
    {
        const auto [entry, inserted] { pluginStates.try_emplace (uuid, std::make_unique<PluginState> (*instance, paneRow)) };
        const auto& [key, pluginState] { *entry };

        jassert (inserted);
        pluginState->setRows (paneRow.getChildWithName (jam::Format::toType (Id::pluginState)));
    }

    auto plugin { graph.addNode (std::move (instance)) };
    jassert (plugin != nullptr);

    const auto [entry, inserted] { plugins.try_emplace (uuid, plugin->nodeID) };
    jassert (inserted);

    addConnections (plugin->nodeID);

    paneRow.setProperty (Id::name, plugin->getProcessor()->getName(), nullptr);
}

void Session::addConnections (juce::AudioProcessorGraph::NodeID pluginId)
{
    auto* processor { graph.getNodeForId (pluginId)->getProcessor() };
    const auto inputChannels { std::min (processor->getTotalNumInputChannels(),
                                         graph.getNodeForId (audioInputId)->getProcessor()->getTotalNumOutputChannels()) };
    const auto outputChannels { std::min (processor->getTotalNumOutputChannels(),
                                          graph.getNodeForId (audioOutputId)->getProcessor()->getTotalNumInputChannels()) };

    for (int channel { 0 }; channel < inputChannels; ++channel)
        graph.addConnection ({ { audioInputId, channel }, { pluginId, channel } });

    for (int channel { 0 }; channel < outputChannels; ++channel)
        graph.addConnection ({ { pluginId, channel }, { audioOutputId, channel } });

    if (processor->acceptsMidi())
        graph.addConnection ({ { midiInputId, juce::AudioProcessorGraph::midiChannelIndex },
                               { pluginId, juce::AudioProcessorGraph::midiChannelIndex } });
}

void Session::removePlugin (jam::UUID uuid)
{
    pluginStates.erase (uuid);

    if (plugins.contains (uuid))
    {
        graph.removeNode (plugins.at (uuid));
        plugins.erase (uuid);
    }
}
