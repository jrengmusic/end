#pragma once
#include <JuceHeader.h>
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

private:
    void offloadPlugins();

    static constexpr const char* hostUrl { ProjectInfo::productWebsite };

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
