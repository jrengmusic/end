#pragma once
#include <JuceHeader.h>
#include "end/Session.h"

struct Nexus : jam::Instance<Nexus>
{
    Nexus()
    {
        model.getOrCreateChildWithName (Id::toType (Id::window));

        auto sessionsTree { model.getOrCreateChildWithName (Id::toType (Id::sessions)) };
        model.createAndAddParameter<jam::Parameter<int64_t>> (
            sessionsTree, Id::focusedSession, int64_t { 0 });
        model.createAndAddParameter<jam::Parameter<int64_t>> (
            sessionsTree, Id::focusedPane, int64_t { 0 });

        model.getOrCreateChildWithName (Id::toType (Id::overlay));

        extensions.try_emplace (juce::String { jam::clap::Services::extensionId }, &services);
        juce::addDefaultFormatsToManager (formatManager);
        formatManager.addFormat (std::make_unique<jam::clap::PluginFormat> (
            extensions,
            juce::String { ProjectInfo::projectName },
            juce::String { ProjectInfo::companyName },
            juce::String { hostUrl },
            juce::String { ProjectInfo::versionString }));
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
        auto [entry, inserted] =
            sessions.try_emplace (sessionUuid, std::make_unique<Session> (sessionUuid, model));
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

    Session& getSession (jam::UUID sessionUuid) { return *sessions.at (sessionUuid); }

    void removeSession (jam::UUID sessionUuid)
    {
        auto sessionsTree { model.getChildWithName (Id::toType (Id::sessions)) };
        sessionsTree.removeChild (sessions.at (sessionUuid)->state, nullptr);
        sessions.erase (sessionUuid);
    }

    std::unique_ptr<juce::AudioPluginInstance> createPlugin (const juce::String& pluginId)
    {
        if (pluginId.isNotEmpty())
        {
            for (auto* format : formatManager.getFormats())
            {
                if (auto* clapFormat { dynamic_cast<jam::clap::PluginFormat*> (format) })
                {
                    juce::OwnedArray<juce::PluginDescription> descriptions;

                    const auto searchPaths { clapFormat->getDefaultLocationsToSearch() };
                    const auto files { clapFormat->searchPathsForPlugins (searchPaths, true) };

                    for (const auto& file : files)
                        clapFormat->findAllTypesForFile (descriptions, file);

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
            }
        }

        return nullptr;
    }

    Session& getActiveSession()
    {
        auto* focusedSessionParameter { model.getParameter<jam::Parameter<int64_t>> (
            Id::toType (Id::sessions), Id::focusedSession) };
        jassert (focusedSessionParameter != nullptr);

        // Threat: called before ENDApplication's bootstrap createSession() —
        // Id::focusedSession is still its 0 (no-session) seed value.
        jassert (focusedSessionParameter->getValue() != 0);

        return getSession (jam::UUID (focusedSessionParameter->getValue()));
    }

    void createVirtualClock (jam::UUID uuid, juce::AudioProcessor& processor)
    {
        auto [entry, inserted] = virtualClocks.try_emplace (uuid, std::make_unique<VirtualClock>());
        jassert (inserted);
        auto& [key, clock] = *entry;

        clock->player.setProcessor (&processor);
        clock->device.open ({}, {}, virtualSampleRate, clock->device.getDefaultBufferSize());
        clock->device.start (&clock->player);
        clock->blockSize = virtualBlockSize;

        if (auto* plugin { dynamic_cast<jam::clap::PluginInstance*> (&processor) })
        {
            plugin->onRequestProcess = [this, uuid] { pump (uuid); };
            clock->onThreadExit = [plugin] { plugin->stopProcessing(); };
        }

        clock->startThread();
    }

    void removeVirtualClock (jam::UUID uuid)
    {
        if (virtualClocks.contains (uuid))
        {
            auto& clock { *virtualClocks.at (uuid) };
            clock.signalThreadShouldExit();
            clock.notify();
            clock.waitForThreadToExit (-1);
            clock.device.stop();
            clock.player.setProcessor (nullptr);
            virtualClocks.erase (uuid);
        }
    }

    void pump (jam::UUID uuid)
    {
        virtualClocks.at (uuid)->notify();
    }

private:
    struct VirtualClock : juce::Thread
    {
        VirtualClock() : juce::Thread ("VirtualClock") {}

        ~VirtualClock() override
        {
            signalThreadShouldExit();
            notify();
            waitForThreadToExit (-1);
        }

        void run() override
        {
            for (;;)
            {
                wait (-1);

                if (threadShouldExit())
                {
                    if (onThreadExit)
                        onThreadExit();

                    return;
                }

                device.clock (blockSize);
            }
        }

        jam::VirtualDevice device { jam::VirtualDeviceType::virtualDeviceTypeName,
                                    jam::VirtualDeviceType::virtualDeviceTypeName };
        juce::AudioProcessorPlayer player;
        std::function<void()> onThreadExit;
        int blockSize { 0 };
    };

    ENDModel model;

    jam::HashMap<jam::UUID, std::unique_ptr<Session>> sessions;
    jam::HashMap<jam::UUID, std::unique_ptr<VirtualClock>> virtualClocks;
    jam::HashMap<juce::String, const void*> extensions;

    jam::clap::Services services;
    juce::AudioPluginFormatManager formatManager;
    juce::AudioProcessorGraph graph;
    juce::AudioProcessorPlayer player;
    juce::AudioDeviceManager deviceManager;
    static constexpr double virtualSampleRate { 48000.0 };
    static constexpr int virtualBlockSize { 512 };
    static constexpr const char* hostUrl { "https://jrengmusic.com" };

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Nexus)
};
