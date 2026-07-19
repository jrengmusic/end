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

        extensions.try_emplace (juce::String { jam::ClapServices::extensionId }, &services);
        juce::addDefaultFormatsToManager (formatManager);
        formatManager.addFormat (std::make_unique<jam::ClapPluginFormat> (
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

    /** @brief Fills services from the host's live Vulkan/glyph state.
     *
     *  Must be called after jam::VulkanEngine and its interning tables are
     *  constructed — resolves stale/null Services otherwise. */
    void initialiseServices()
    {
        services.vulkanEngine       = jam::VulkanEngine::getInstance();
        services.glyphAtlas         = jam::GlyphAtlas::getInstance();
        services.typeface           = jam::Typeface::getInstance();
        services.stamp              = jam::Stamp::getInstance();
        services.grapheme           = jam::Grapheme::getInstance();
        services.link               = jam::Link::getInstance();
        services.contextFactory     = juce::ComponentPeer::externalContextFactory;
        services.cachedImageFactory = juce::Component::externalCachedImageFactory;
    }

    Session& getSession (jam::UUID sessionUuid) { return *sessions.at (sessionUuid); }

    void removeSession (jam::UUID sessionUuid)
    {
        auto sessionsTree { model.getChildWithName (Id::toType (Id::sessions)) };
        sessionsTree.removeChild (sessions.at (sessionUuid)->state, nullptr);
        sessions.erase (sessionUuid);
    }

    void createPlugin (const juce::String& pluginId,
                       std::function<void (std::unique_ptr<juce::AudioPluginInstance>)> callback)
    {
        if (pluginId.isNotEmpty())
        {
            for (auto* format : formatManager.getFormats())
            {
                if (auto* clapFormat { dynamic_cast<jam::ClapPluginFormat*> (format) })
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
                            formatManager.createPluginInstanceAsync (
                                *description, virtualSampleRate, virtualBlockSize,
                                [cb = std::move (callback)] (std::unique_ptr<juce::AudioPluginInstance> instance,
                                                              const juce::String&)
                                {
                                    cb (std::move (instance));
                                });

                            return;
                        }
                    }
                }
            }
        }

        callback (nullptr);
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

        if (auto* plugin { dynamic_cast<jam::ClapPluginInstance*> (&processor) })
            clock->onThreadExit = [plugin] { plugin->stopProcessing(); };

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

private:
    static constexpr double virtualSampleRate { 48000.0 };
    static constexpr int virtualBlockSize { 512 };
    static constexpr int blockPeriodMilliseconds { static_cast<int> (1000.0 * virtualBlockSize / virtualSampleRate) };
    static constexpr const char* hostUrl { "https://jrengmusic.com" };

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
                wait (blockPeriodMilliseconds);

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

    jam::ClapServices services;
    juce::AudioPluginFormatManager formatManager;
    juce::AudioProcessorGraph graph;
    juce::AudioProcessorPlayer player;
    juce::AudioDeviceManager deviceManager;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Nexus)
};
