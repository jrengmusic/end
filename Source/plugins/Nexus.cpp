#include "plugins/Nexus.h"
#if JUCE_MAC
#include <AudioToolbox/AudioToolbox.h>
#endif

static decltype (juce::ComponentPeer::externalKeyPressedCallback) keyPressedCallback { [] (const juce::KeyPress& key) -> bool
{
    if (auto* window { juce::TopLevelWindow::getActiveTopLevelWindow() })
    {
        if (auto* peer { window->getPeer() })
            return peer->handleKeyPress (key);
    }

    return false;
} };

static decltype (juce::ComponentPeer::externalMouseEventCallback) mouseEventCallback { [] (juce::MouseInputSource::InputSourceType type, juce::Point<float> position, juce::ModifierKeys modifiers, float pressure, float orientation, juce::int64 time, juce::PenDetails pen, int touchIndex)
{
    if (auto* window { juce::TopLevelWindow::getActiveTopLevelWindow() })
    {
        if (auto* peer { window->getPeer() })
            peer->handleMouseEvent (type, peer->globalToLocal (position), modifiers, pressure, orientation, time, pen, touchIndex);
    }
} };

#if JAM_USING_PROFILER
static void (*promptPaintedCallback)() { [] { Nexus::getInstance()->onPromptPainted(); } };
static void (*manifestFileGetter) (char*, size_t) { [] (char* hostFile, size_t numBytes) { Nexus::getManifestFile().getFullPathName().copyToUTF8 (hostFile, numBytes); } };
#endif

Nexus::Nexus()
{
    registerExtensions();

    model.getOrCreateChildWithName (jam::Format::toType (Id::window));

    auto sessionsTree { model.getOrCreateChildWithName (jam::Format::toType (Id::sessions)) };
    model.createAndAddParameter<jam::Parameter<int64_t>> (
        sessionsTree, Id::focusedSession, jam::UUID::none().value);

    addFormats();
    deviceManager.initialiseWithDefaultDevices (2, 2);
    player.setProcessor (&graph);
    audioInputId = graph.addNode (std::make_unique<juce::AudioProcessorGraph::AudioGraphIOProcessor> (
        juce::AudioProcessorGraph::AudioGraphIOProcessor::audioInputNode))->nodeID;
    audioOutputId = graph.addNode (std::make_unique<juce::AudioProcessorGraph::AudioGraphIOProcessor> (
        juce::AudioProcessorGraph::AudioGraphIOProcessor::audioOutputNode))->nodeID;
    midiInputId = graph.addNode (std::make_unique<juce::AudioProcessorGraph::AudioGraphIOProcessor> (
        juce::AudioProcessorGraph::AudioGraphIOProcessor::midiInputNode))->nodeID;
    deviceManager.addAudioCallback (&player);
    registerEVE();
}

void Nexus::registerExtensions()
{
    extensions.try_emplace (Id::enaxContextFactory, &juce::ComponentPeer::externalContextFactory);
    extensions.try_emplace (Id::enaxPeerDeleted, &juce::ComponentPeer::externalPeerDeletedCallback);
    extensions.try_emplace (Id::enaxKeyPressed, &keyPressedCallback);
    extensions.try_emplace (Id::enaxMouseEvent, &mouseEventCallback);
#if JAM_USING_PROFILER
    extensions.try_emplace (Id::enaxPromptPainted, &promptPaintedCallback);
    extensions.try_emplace (Id::enaxManifestFile, &manifestFileGetter);
#endif
}

void Nexus::addFormats()
{
    juce::addDefaultFormatsToManager (formatManager);
    formatManager.addFormat (std::make_unique<jam::ClapPluginFormat> (
        extensions,
        juce::String { ProjectInfo::projectName },
        juce::String { ProjectInfo::companyName },
        juce::String { hostUrl },
        juce::String { ProjectInfo::versionString },
        ConfigDirectory::Config::getPath (Id::plugins).getFullPathName()));
}

void Nexus::registerEVE()
{
    const auto eve { getOrCreatePlugin() };

    if (auto* format { Session::getPluginFormat (formatManager, eve) })
    {
        juce::PluginDescription description {};
        description.pluginFormatName = format->getName();
        description.fileOrIdentifier = eve.getFullPathName();
        description.lastFileModTime = eve.getLastModificationTime();
        knownPluginList.addType (description);
    }

#if JUCE_MAC
    if (eve.hasFileExtension (files::audioUnitExtension))
        registerAudioUnit (addBundle (eve));
#endif
}

void Nexus::addPluginModule (const juce::File& file)
{
    if (pluginModules.addIfNotAlreadyThere (file.getFullPathName()))
    {
#if JUCE_MAC
        addBundle (file);
#endif

#if JUCE_WINDOWS
        libraries.add (std::make_unique<juce::DynamicLibrary> (file.getFullPathName()));
#endif
    }
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
        sessions.try_emplace (sessionUuid, std::make_unique<Session> (sessionUuid, model, model.getChildWithName (jam::Format::toType (Id::sessions)), graph, audioInputId, audioOutputId, midiInputId, formatManager, knownPluginList));
    jassert (inserted);
    auto& [key, session] = *entry;

    auto sessionsTree { model.getChildWithName (jam::Format::toType (Id::sessions)) };
    sessionsTree.appendChild (session->state, nullptr);

    auto* focusedSessionParameter { model.getParameter<jam::Parameter<int64_t>> (
        jam::Format::toType (Id::sessions), Id::focusedSession) };
    jassert (focusedSessionParameter != nullptr);

    if (jam::UUID { focusedSessionParameter->getValue() } == jam::UUID::none())
        focusedSessionParameter->setValue (sessionUuid.value);

    return *session;
}

Session& Nexus::getSession (jam::UUID sessionUuid) const { return *sessions.at (sessionUuid); }

Session& Nexus::getActiveSession() const
{
    auto* focusedSessionParameter { model.getParameter<jam::Parameter<int64_t>> (
        jam::Format::toType (Id::sessions), Id::focusedSession) };
    jassert (focusedSessionParameter != nullptr);

    // Threat: called before the first session exists, while Id::focusedSession holds its 0 default.
    jassert (jam::UUID { focusedSessionParameter->getValue() } != jam::UUID::none());

    return getSession (jam::UUID (focusedSessionParameter->getValue()));
}

const void* Nexus::getExtension (const char* id) const
{
    if (auto found { extensions.find (juce::String (id)) }; found != extensions.end())
    {
        const auto& [key, extension] { *found };
        return extension;
    }

    return nullptr;
}

#if JAM_USING_PROFILER
juce::File Nexus::getManifestFile()
{
    return juce::File::getCurrentWorkingDirectory().getChildFile (juce::JUCEApplicationBase::getCommandLineParameterArray().getReference (0));
}
#endif

static const juce::String pluginsHash { CAST_PLUGINS_HASH };

void Nexus::offloadPlugins()
{
    const auto pluginsDirectory { jam::File::getOrCreateDirectory (ConfigDirectory::Config::path, Id::plugins) };

    for (int resourceIndex { 0 }; resourceIndex < BinaryData::namedResourceListSize; ++resourceIndex)
    {
        const char* resourceName { BinaryData::namedResourceList[resourceIndex] };
        const auto original { juce::File::createFileWithoutCheckingPath (
            BinaryData::getNamedResourceOriginalFilename (resourceName)) };

        if (original.hasFileExtension (files::archiveExtension))
        {
            int resourceSize { 0 };
            const char* resourceData { BinaryData::getNamedResource (resourceName, resourceSize) };
            juce::MemoryInputStream stream { resourceData, static_cast<size_t> (resourceSize), false };
            juce::ZipFile zip { stream };

            for (int entryIndex { 0 }; entryIndex < zip.getNumEntries(); ++entryIndex)
                zip.uncompressEntry (entryIndex, pluginsDirectory, true);
        }
    }

    pluginsDirectory.getChildFile (files::pluginsStamp).replaceWithText (pluginsHash);
}

juce::File Nexus::getOrCreatePlugin()
{
    const auto plugin { ConfigDirectory::Config::getPath (Id::plugins).getChildFile (files::evePlugin) };
    const auto stamp { ConfigDirectory::Config::getPath (Id::plugins).getChildFile (files::pluginsStamp) };

    if (stamp.loadFileAsString().compare (pluginsHash) != 0 or not plugin.exists())
        offloadPlugins();

    return plugin;
}

#if JUCE_MAC
static OSType getOSType (CFDictionaryRef entry, CFStringRef key)
{
    return juce::ByteOrder::bigEndianInt (
        juce::String::fromCFString (static_cast<CFStringRef> (CFDictionaryGetValue (entry, key))).toRawUTF8());
}

static void registerAudioComponent (CFBundleRef bundle, CFDictionaryRef entry)
{
    const auto factoryName { static_cast<CFStringRef> (CFDictionaryGetValue (entry, CFSTR ("factoryFunction"))) };
    const auto factory { reinterpret_cast<AudioComponentFactoryFunction> (
        CFBundleGetFunctionPointerForName (bundle, factoryName)) };
    jassert (factory != nullptr);

    const AudioComponentDescription description {
        getOSType (entry, CFSTR ("type")),
        getOSType (entry, CFSTR ("subtype")),
        getOSType (entry, CFSTR ("manufacturer")),
        0,
        0 };
    const auto name { static_cast<CFStringRef> (CFDictionaryGetValue (entry, CFSTR ("name"))) };
    UInt32 version { 0 };
    CFNumberGetValue (static_cast<CFNumberRef> (CFDictionaryGetValue (entry, CFSTR ("version"))),
                      kCFNumberSInt32Type,
                      &version);

    AudioComponentRegister (&description, name, version, factory);
}

CFBundleRef Nexus::addBundle (const juce::File& pluginFile)
{
    const std::unique_ptr<std::remove_pointer_t<CFURLRef>, decltype (&CFRelease)> url {
        CFURLCreateFromFileSystemRepresentation (
            kCFAllocatorDefault,
            reinterpret_cast<const UInt8*> (pluginFile.getFullPathName().toRawUTF8()),
            static_cast<CFIndex> (pluginFile.getFullPathName().getNumBytesAsUTF8()),
            true),
        CFRelease };
    jassert (url != nullptr);

    bundles.emplace_back (CFBundleCreate (kCFAllocatorDefault, url.get()), CFRelease);
    jassert (bundles.back() != nullptr);

    return bundles.back().get();
}

void Nexus::registerAudioUnit (CFBundleRef bundle)
{
    const auto audioComponents { static_cast<CFArrayRef> (
        CFBundleGetValueForInfoDictionaryKey (bundle, CFSTR ("AudioComponents"))) };
    jassert (audioComponents != nullptr);

    for (CFIndex i { 0 }; i < CFArrayGetCount (audioComponents); ++i)
        registerAudioComponent (bundle, static_cast<CFDictionaryRef> (CFArrayGetValueAtIndex (audioComponents, i)));
}
#endif
