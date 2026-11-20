#include "plugins/Session.h"

Session::Session (const juce::ValueTree& row,
                  ENDModel& newModel,
                  juce::ValueTree newSessionsState,
                  juce::AudioProcessorGraph& newGraph,
                  juce::AudioProcessorGraph::NodeID newAudioInputId,
                  juce::AudioProcessorGraph::NodeID newAudioOutputId,
                  juce::AudioProcessorGraph::NodeID newMidiInputId,
                  juce::AudioPluginFormatManager& newFormatManager,
                  juce::KnownPluginList& newKnownPluginList)
    : state (row),
      sessionsState (newSessionsState),
      model (newModel),
      graph (newGraph),
      audioInputId (newAudioInputId),
      audioOutputId (newAudioOutputId),
      midiInputId (newMidiInputId),
      formatManager (newFormatManager),
      knownPluginList (newKnownPluginList)
{
    model.createAndAddParameter<jam::Parameter<int64_t>> (state, Id::id, static_cast<int64_t> (state.getProperty (Id::id)));
    registerEvents();
    sessionsState.addListener (this);
    loadPlugins (state);
}

Session::~Session()
{
    sessionsState.removeListener (this);
}

juce::AudioPluginInstance& Session::getPlugin (jam::UUID uuid) { return static_cast<juce::AudioPluginInstance&> (*graph.getNodeForId (plugins.at (uuid))->getProcessor()); }

bool Session::contains (jam::UUID uuid) const { return plugins.contains (uuid); }

juce::ValueTree Session::getTerminal (const juce::ValueTree& paneRow)
{
    return paneRow.getChildWithName (jam::Format::toType (Id::plugin)).getChild (0).getChildWithName (jam::Format::toType (Id::terminal));
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

void Session::registerEvents()
{
    events.add (Id::plugin, [this] (const juce::Identifier&, juce::ValueTree& tree)
    {
        if (tree.hasType (jam::Format::toType (Id::pane)) and tree.isAChildOf (state))
        {
            const jam::UUID uuid { tree.getProperty (Id::id) };

            removePlugin (uuid);
            tree.removeProperty (Id::name, nullptr);
            tree.removeChild (tree.getChildWithName (jam::Format::toType (Id::plugin)), nullptr);
            loadPlugin (tree);
        }
    });

    events.add (Id::focus, [this] (const juce::Identifier&, juce::ValueTree& tree)
    {
        if (tree.hasType (jam::Format::toType (Id::pane)) and tree.isAChildOf (state))
        {
            auto terminal { getTerminal (tree) };

            if (terminal.isValid())
            {
                terminal.setProperty (Id::focus, tree.getProperty (Id::focus), nullptr);

                if (static_cast<bool> (tree.getProperty (Id::focus)))
                    sessionsState.setProperty (Id::cwd, terminal.getProperty (Id::cwd), nullptr);
            }
        }
    });

    events.add (Id::cwd, [this] (const juce::Identifier&, juce::ValueTree& tree)
    {
        if (tree.hasType (jam::Format::toType (Id::terminal)) and tree.isAChildOf (state))
        {
            const auto paneRow { tree.getParent().getParent().getParent() };

            if (static_cast<bool> (paneRow.getProperty (Id::focus)))
                sessionsState.setProperty (Id::cwd, tree.getProperty (Id::cwd), nullptr);
        }
    });
}

void Session::valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property)
{
    if (events.contains (property))
        events.get (property, tree);
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

void Session::loadPlugins (const juce::ValueTree& row)
{
    for (const auto child : row)
    {
        if (child.hasType (jam::Format::toType (Id::pane)) and child.hasProperty (Id::plugin))
            loadPlugin (child);

        loadPlugins (child);
    }
}

void Session::addPlugin (juce::ValueTree paneRow, std::unique_ptr<juce::AudioPluginInstance> instance)
{
#if JAM_USING_PROFILER
    const jam::Profiler::Zone zone { Id::graphNode };
#endif
    const jam::UUID uuid { paneRow.getProperty (Id::id) };
    instance->enableAllBuses();

    if (paneRow.hasProperty (Id::state))
    {
        const auto* savedState { paneRow.getProperty (Id::state).getBinaryData() };
        jassert (savedState != nullptr);

        if (savedState != nullptr)
            instance->setStateInformation (savedState->getData(), static_cast<int> (savedState->getSize()));

        paneRow.removeProperty (Id::state, nullptr);
    }

    auto plugin { graph.addNode (std::move (instance)) };
    jassert (plugin != nullptr);

    const auto [entry, inserted] { plugins.try_emplace (uuid, plugin->nodeID) };
    jassert (inserted);

    addConnections (plugin->nodeID);

    auto& processor { getPlugin (uuid) };

    juce::MemoryBlock block;

    if (auto* client { processor.getVSTClient() }; client != nullptr and client->getChunkData (block, false))
    {
        const auto chunk { juce::ValueTree::readFromData (block.getData(), block.getSize()) };

        if (chunk.hasProperty (Id::id))
        {
            juce::ValueTree held { chunk.getType() };
            held.setProperty (Id::id, chunk.getProperty (Id::id), nullptr);
            paneRow.getOrCreateChildWithName (jam::Format::toType (Id::plugin), nullptr).appendChild (held, nullptr);
        }
    }

    paneRow.setProperty (Id::name, processor.getName(), nullptr);
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
    if (plugins.contains (uuid))
    {
        graph.removeNode (plugins.at (uuid));
        plugins.erase (uuid);
    }
}

void Session::setPluginStates (juce::ValueTree sessions) const
{
    for (const auto& [uuid, nodeId] : plugins)
    {
        auto paneRow { jam::Model::getChildWithID (sessions, juce::var (uuid.value)) };
        jassert (paneRow.isValid());

        if (paneRow.isValid())
        {
            juce::MemoryBlock block;
            graph.getNodeForId (nodeId)->getProcessor()->getStateInformation (block);

            paneRow.setProperty (Id::state, juce::var (block), nullptr);
            paneRow.removeChild (paneRow.getChildWithName (jam::Format::toType (Id::plugin)), nullptr);
            paneRow.removeProperty (Id::name, nullptr);
        }
    }
}
