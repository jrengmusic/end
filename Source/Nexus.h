#pragma once
#include <JuceHeader.h>
#include "end/Session.h"

struct Nexus : jam::Instance<Nexus>
{
    struct VirtualClock
    {
        jam::VirtualDevice device { jam::VirtualDeviceType::virtualDeviceTypeName,
                                    jam::VirtualDeviceType::virtualDeviceTypeName };
        juce::AudioProcessorPlayer player;
    };

    Nexus()
    {
        model.getOrCreateChildWithName (IDtype::window);

        auto sessionsTree { model.getOrCreateChildWithName (IDtype::sessions) };
        model.createAndAddParameter<jam::Parameter<int64_t>> (
            sessionsTree, ID::focusedSession, int64_t { 0 });
        model.createAndAddParameter<jam::Parameter<int64_t>> (
            sessionsTree, jam::ID::focusedPane, int64_t { 0 });

        model.getOrCreateChildWithName (IDtype::overlay);

        extensions.try_emplace (juce::String { jam::clap::Services::extensionId }, &services);
        juce::addDefaultFormatsToManager (formatManager);
        formatManager.addFormat (std::make_unique<jam::clap::PluginFormat>());
        deviceManager.initialiseWithDefaultDevices (2, 2);
        player.setProcessor (&graph);
        deviceManager.addAudioCallback (&player);
    }

    ~Nexus()
    {
        deviceManager.removeAudioCallback (&player);
        player.setProcessor (nullptr);
    }

    Session& createSession()
    {
        jam::UUID sessionUuid;
        auto [entry, inserted] = sessions.try_emplace (
            sessionUuid, std::make_unique<Session> (sessionUuid, model));
        jassert (inserted);
        auto& [key, session] = *entry;

        auto sessionsTree { model.getChildWithName (IDtype::sessions) };
        sessionsTree.appendChild (session->state, nullptr);

        auto* focusedSessionParameter { model.getParameter<jam::Parameter<int64_t>> (
            IDtype::sessions, ID::focusedSession) };
        jassert (focusedSessionParameter != nullptr);

        if (focusedSessionParameter->getValue() == 0)
            focusedSessionParameter->setValue (sessionUuid.value);

        return *session;
    }

    Session& getSession (jam::UUID sessionUuid) { return *sessions.at (sessionUuid); }

    void removeSession (jam::UUID sessionUuid)
    {
        auto sessionsTree { model.getChildWithName (IDtype::sessions) };
        sessionsTree.removeChild (sessions.at (sessionUuid)->state, nullptr);
        sessions.erase (sessionUuid);
    }

    std::unique_ptr<juce::AudioPluginInstance> createPlugin (const juce::String& pluginId)
    {
        if (pluginId.isNotEmpty())
        {
            juce::OwnedArray<juce::PluginDescription> descriptions;

            for (auto* format : formatManager.getFormats())
            {
                const auto files { format->searchPathsForPlugins (format->getDefaultLocationsToSearch(), true) };

                for (const auto& file : files)
                    format->findAllTypesForFile (descriptions, file);
            }

            for (auto* description : descriptions)
            {
                if (description->uniqueId == pluginId.hashCode())
                {
                    juce::String errorMessage;
                    return formatManager.createPluginInstance (
                        *description, virtualSampleRate, virtualBlockSize, errorMessage);
                }
            }
        }

        return nullptr;
    }

    Session& getActiveSession()
    {
        auto* focusedSessionParameter { model.getParameter<jam::Parameter<int64_t>> (
            IDtype::sessions, ID::focusedSession) };
        jassert (focusedSessionParameter != nullptr);

        // Threat: called before ENDApplication's bootstrap createSession() —
        // ID::focusedSession is still its 0 (no-session) seed value.
        jassert (focusedSessionParameter->getValue() != 0);

        return getSession (jam::UUID (focusedSessionParameter->getValue()));
    }

    VirtualClock& createVirtualClock (jam::UUID uuid, juce::AudioProcessor& processor)
    {
        auto [entry, inserted] = virtualClocks.try_emplace (uuid, std::make_unique<VirtualClock>());
        jassert (inserted);
        auto& [key, clock] = *entry;

        clock->player.setProcessor (&processor);
        clock->device.open ({}, {}, virtualSampleRate, clock->device.getDefaultBufferSize());
        clock->device.start (&clock->player);

        return *clock;
    }

    void removeVirtualClock (jam::UUID uuid)
    {
        virtualClocks.at (uuid)->device.stop();
        virtualClocks.at (uuid)->player.setProcessor (nullptr);
        virtualClocks.erase (uuid);
    }

    void pump (jam::UUID uuid, int numSamples) { virtualClocks.at (uuid)->device.clock (numSamples); }

private:
    ENDModel model;

    jam::HashMap<jam::UUID, std::unique_ptr<Session>> sessions;

    jam::clap::Services services;
    jam::HashMap<juce::String, const void*> extensions;
    juce::AudioPluginFormatManager formatManager;
    juce::AudioProcessorGraph graph;
    juce::AudioProcessorPlayer player;
    juce::AudioDeviceManager deviceManager;
    jam::HashMap<jam::UUID, std::unique_ptr<VirtualClock>> virtualClocks;
    static constexpr double virtualSampleRate { 48000.0 };
    static constexpr int virtualBlockSize { 512 };

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Nexus)
};
