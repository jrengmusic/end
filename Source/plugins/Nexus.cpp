#include "plugins/Nexus.h"

Nexus::Nexus()
{
    model.getOrCreateChildWithName (Id::toType (Id::window));

    auto sessionsTree { model.getOrCreateChildWithName (Id::toType (Id::sessions)) };
    model.createAndAddParameter<jam::Parameter<int64_t>> (
        sessionsTree, Id::focusedSession, int64_t { 0 });
    model.createAndAddParameter<jam::Parameter<int64_t>> (
        sessionsTree, Id::focusedPane, int64_t { 0 });

    juce::addDefaultFormatsToManager (formatManager);
    formatManager.addFormat (std::make_unique<jam::ClapPluginFormat> (
        extensions,
        juce::String { ProjectInfo::projectName },
        juce::String { ProjectInfo::companyName },
        juce::String { hostUrl },
        juce::String { ProjectInfo::versionString }));
    deviceManager.initialiseWithDefaultDevices (2, 2);
    player.setProcessor (&graph);
    audioInputNodeId = graph.addNode (std::make_unique<juce::AudioProcessorGraph::AudioGraphIOProcessor> (
        juce::AudioProcessorGraph::AudioGraphIOProcessor::audioInputNode))->nodeID;
    audioOutputNodeId = graph.addNode (std::make_unique<juce::AudioProcessorGraph::AudioGraphIOProcessor> (
        juce::AudioProcessorGraph::AudioGraphIOProcessor::audioOutputNode))->nodeID;
    midiInputNodeId = graph.addNode (std::make_unique<juce::AudioProcessorGraph::AudioGraphIOProcessor> (
        juce::AudioProcessorGraph::AudioGraphIOProcessor::midiInputNode))->nodeID;
    deviceManager.addAudioCallback (&player);
    offloadPlugins();
}

Nexus::~Nexus()
{
    deviceManager.removeAudioCallback (&player);
    player.setProcessor (nullptr);
}

Session& Nexus::createSession()
{
    jam::UUID sessionUuid;
    auto [entry, inserted] =
        sessions.try_emplace (sessionUuid, std::make_unique<Session> (sessionUuid, model, graph, audioInputNodeId, audioOutputNodeId, midiInputNodeId, formatManager));
    jassert (inserted);
    auto& [key, session] = *entry;

    auto sessionsTree { model.getChildWithName (Id::toType (Id::sessions)) };
    sessionsTree.appendChild (session->state, nullptr);

    auto* focusedSessionParameter { model.getParameter<jam::Parameter<int64_t>> (
        Id::toType (Id::sessions), Id::focusedSession) };
    jassert (focusedSessionParameter != nullptr);

    if (focusedSessionParameter->getValue() == 0)
        focusedSessionParameter->setValue (sessionUuid.value);

    return *session;
}

Session& Nexus::getSession (jam::UUID sessionUuid) { return *sessions.at (sessionUuid); }

void Nexus::removeSession (jam::UUID sessionUuid)
{
    auto sessionsTree { model.getChildWithName (Id::toType (Id::sessions)) };
    sessionsTree.removeChild (sessions.at (sessionUuid)->state, nullptr);
    sessions.erase (sessionUuid);
}

Session& Nexus::getActiveSession()
{
    auto* focusedSessionParameter { model.getParameter<jam::Parameter<int64_t>> (
        Id::toType (Id::sessions), Id::focusedSession) };
    jassert (focusedSessionParameter != nullptr);

    // Threat: called before ENDApplication's bootstrap createSession() —
    // Id::focusedSession is still its 0 (no-session) seed value.
    jassert (focusedSessionParameter->getValue() != 0);

    return getSession (jam::UUID (focusedSessionParameter->getValue()));
}

void Nexus::offloadPlugins()
{
    const auto pluginsDirectory { jam::File::getOrCreateDirectory (ConfigDirectory::Config::path, Id::plugins) };

    for (int resourceIndex { 0 }; resourceIndex < BinaryData::namedResourceListSize; ++resourceIndex)
    {
        const char* resourceName { BinaryData::namedResourceList[resourceIndex] };
        const auto original { juce::File::createFileWithoutCheckingPath (
            BinaryData::getNamedResourceOriginalFilename (resourceName)) };

        if (original.hasFileExtension ("zip"))
        {
            int resourceSize { 0 };
            const char* resourceData { BinaryData::getNamedResource (resourceName, resourceSize) };
            juce::MemoryInputStream stream { resourceData, static_cast<size_t> (resourceSize), false };
            juce::ZipFile zip { stream };

            for (int entryIndex { 0 }; entryIndex < zip.getNumEntries(); ++entryIndex)
            {
                const auto* entry { zip.getEntry (entryIndex) };
                const auto target { pluginsDirectory.getChildFile (entry->filename) };

                if (not target.existsAsFile() or target.getSize() != entry->uncompressedSize)
                    zip.uncompressEntry (entryIndex, pluginsDirectory, true);
            }
        }
    }
}
