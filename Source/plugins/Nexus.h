#pragma once
#include <JuceHeader.h>
#if JUCE_MAC
#include <CoreFoundation/CoreFoundation.h>
#endif
#include "plugins/Session.h"
#include "config/ConfigDirectory.h"

struct Nexus : jam::Instance<Nexus>
{
    Nexus();

    ~Nexus();

    Session& createSession();

    Session& getSession (jam::UUID sessionUuid);

    void removeSession (jam::UUID sessionUuid);

    Session& getActiveSession();

    const void* getExtension (const char* id) const;

private:
    void offloadPlugins();

#if JUCE_MAC
    void registerAudioUnit (const juce::File& componentFile);
#endif

    static constexpr const char* hostUrl { ProjectInfo::productWebsite };

#if JUCE_MAC
    std::vector<std::unique_ptr<std::remove_pointer_t<CFBundleRef>, decltype (&CFRelease)>> bundles;
#endif

    ENDModel model;

    jam::HashMap<jam::UUID, std::unique_ptr<Session>> sessions;
    jam::HashMap<juce::String, const void*> extensions;

    juce::AudioPluginFormatManager formatManager;
    juce::AudioProcessorGraph graph;
    juce::AudioProcessorGraph::NodeID audioInputNodeId;
    juce::AudioProcessorGraph::NodeID audioOutputNodeId;
    juce::AudioProcessorGraph::NodeID midiInputNodeId;
    juce::AudioProcessorPlayer player;
    juce::AudioDeviceManager deviceManager;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Nexus)
};
