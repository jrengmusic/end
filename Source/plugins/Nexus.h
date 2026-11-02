#pragma once
#include <JuceHeader.h>
#if JUCE_MAC
#include <CoreFoundation/CoreFoundation.h>
#endif
#include "plugins/Session.h"
#include "config/ConfigDirectory.h"

/**
 * @struct Nexus
 * @brief Application root of END: the audio graph, the plugin formats, the
 *        model of the sessions, and the host extensions that the hosted plugins read.
 *
 * One graph and one format manager serve every session. Each session holds the
 * hosted plugin of each of its panes, inside this graph.
 */
struct Nexus : jam::Instance<Nexus>
{
    /**
     * @brief Registers the host extensions, builds the model, the plugin formats and the audio graph,
     *        then offloads the plugins of the binary data.
     *
     * On macOS, the constructor registers the Audio Unit of the EVE plugin when that plugin is a component.
     */
    Nexus();

    /** @brief Removes the audio callback and clears the processor of the player. */
    ~Nexus();

    /**
     * @brief Creates a session with a new identity, adds its row to the sessions tree,
     *        and makes it the focused session when none is focused.
     * @return The new session.
     */
    Session& createSession();

    /**
     * @brief Returns the session with an identity.
     * @param sessionUuid  Identity of the session. The session must exist.
     * @return The session.
     */
    Session& getSession (jam::UUID sessionUuid);

    /**
     * @brief Removes the row of a session from the sessions tree and erases the session.
     * @param sessionUuid  Identity of the session to remove. The session must exist.
     */
    void removeSession (jam::UUID sessionUuid);

    /**
     * @brief Returns the focused session, which the `focusedSession` parameter names.
     *
     * The parameter must hold a session identity. It is zero only before the first session is created.
     *
     * @return The focused session.
     */
    Session& getActiveSession();

    /**
     * @brief Returns the host extension with an identifier.
     * @param id  Identifier of the extension.
     * @return The extension, or nullptr when the host has no extension with that identifier.
     */
    const void* getExtension (const char* id) const;

private:
    /**
     * @brief Unpacks the plugin archives of the binary data into the plugins folder once per plugin hash.
     */
    void offloadPlugins();

#if JUCE_MAC
    /**
     * @brief Loads the bundle of an Audio Unit component and registers each audio component that the bundle declares.
     * @param componentFile  Component bundle file.
     */
    void registerAudioUnit (const juce::File& componentFile);
#endif

    /** @brief Website of the product. The CLAP format receives it as the host URL. */
    static constexpr const char* hostUrl { ProjectInfo::productWebsite };

#if JUCE_MAC
    /** @brief Bundles of the registered Audio Unit components. They stay alive as long as Nexus exists. Each is released with CFRelease. */
    std::vector<std::unique_ptr<std::remove_pointer_t<CFBundleRef>, decltype (&CFRelease)>> bundles;
#endif

    /** @brief Model that holds the window row and the sessions tree. The sessions tree holds the focusedSession parameter. */
    ENDModel model;

    /** @brief Sessions keyed by identity. Each session is owned here. */
    jam::HashMap<jam::UUID, std::unique_ptr<Session>> sessions;
    /** @brief Host extensions keyed by identifier. Each value points to the callback or getter that the host reads. */
    jam::HashMap<juce::String, const void*> extensions;

    /** @brief Plugin formats: the default JUCE formats and the CLAP format. */
    juce::AudioPluginFormatManager formatManager;
    /** @brief Audio graph that holds the input and output processors and every hosted plugin. */
    juce::AudioProcessorGraph graph;
    /** @brief Identity of the audio input processor in the graph. */
    juce::AudioProcessorGraph::NodeID audioInputId;
    /** @brief Identity of the audio output processor in the graph. */
    juce::AudioProcessorGraph::NodeID audioOutputId;
    /** @brief Identity of the MIDI input processor in the graph. */
    juce::AudioProcessorGraph::NodeID midiInputId;
    /** @brief Audio callback that plays the graph on the device. */
    juce::AudioProcessorPlayer player;
    /** @brief Audio device manager. It opens the default devices with two inputs and two outputs. */
    juce::AudioDeviceManager deviceManager;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Nexus)
};
