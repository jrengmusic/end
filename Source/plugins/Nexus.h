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
     *        offloads the plugins of the binary data, then lists the EVE plugin.
     *
     * The constructor calls registerEVE(). On macOS, registerEVE() registers the EVE bundle as an Audio Unit
     * component when the EVE file has the component extension.
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

    /**
     * @brief Holds the bundle or the library of an ENAX plugin module for the lifetime of Nexus, once per file.
     *
     * A file that pluginModules already lists is ignored. On macOS, the bundle is created and held in bundles.
     * On Windows, the library is opened and held in libraries. EditorView calls this when the plugin of an
     * ENAX pane loads.
     *
     * @param file  Plugin module file.
     */
    void addPluginModule (const juce::File& file);

#if JAM_USING_PROFILER
    static juce::File getManifestFile();

    /** @brief Callback that the prompt-painted extension invokes when the profiler prompt is painted. ENDView sets it. */
    std::function<void()> onPromptPainted;
#endif

private:
    /**
     * @brief Unpacks the plugin archives of the binary data into the plugins folder once per plugin hash.
     */
    void offloadPlugins();

#if JUCE_MAC
    /**
     * @brief Registers each audio component that the bundle of an Audio Unit component declares.
     * @param bundle  Bundle of the component. The bundle is owned by `bundles`.
     */
    void registerAudioUnit (CFBundleRef bundle);

    /**
     * @brief Creates the bundle of a plugin file and holds it for the lifetime of Nexus.
     *
     * The bundle is owned by bundles. The call asserts when the bundle cannot be created.
     *
     * @param pluginFile  Plugin bundle file.
     * @return The bundle, owned by bundles.
     */
    CFBundleRef addBundle (const juce::File& pluginFile);
#endif

    /**
     * @brief Registers the host callbacks and getters that the hosted plugins read through the host extensions.
     *
     * The registry holds the context factory, the peer deleted callback, the key press callback and the mouse event callback.
     * With the profiler, it also holds the prompt-painted callback and the manifest file getter.
     */
    void registerExtensions();

    /**
     * @brief Adds the default JUCE plugin formats and the CLAP format to the format manager.
     *
     * The CLAP format receives the extensions, the product name, the company name, the host URL and the version.
     */
    void addFormats();

    /**
     * @brief Lists the EVE plugin in the known plugin list, so that its first load runs no scan.
     *
     * The listed description carries the modification time of the EVE file. A file that is listed with an unchanged
     * modification time is not tested again. On macOS, an EVE bundle with the Audio Unit extension is also registered
     * as an audio component.
     */
    void registerEVE();

    /** @brief Website of the product. The CLAP format receives it as the host URL. */
    static constexpr const char* hostUrl { ProjectInfo::productWebsite };

#if JUCE_MAC
    /**
     * @brief Bundles of the registered Audio Unit components and of the ENAX plugin modules.
     *
     * The bundles stay alive as long as Nexus exists. Each is released with CFRelease.
     */
    std::vector<std::unique_ptr<std::remove_pointer_t<CFBundleRef>, decltype (&CFRelease)>> bundles;
#endif

#if JUCE_WINDOWS
    /** @brief Libraries of the ENAX plugin modules, opened by addPluginModule() and held for the lifetime of Nexus. */
    juce::OwnedArray<juce::DynamicLibrary> libraries;
#endif

    /** @brief Paths of the ENAX plugin modules that addPluginModule() has taken. Each path is held once. */
    juce::StringArray pluginModules;

    /** @brief Model that holds the window row and the sessions tree. The sessions tree holds the focusedSession parameter. */
    ENDModel model;

    /** @brief Host extensions keyed by identifier. Each value points to the callback or getter that the host reads. */
    jam::HashMap<juce::String, const void*> extensions;

    /** @brief Plugin formats: the default JUCE formats and the CLAP format. */
    juce::AudioPluginFormatManager formatManager;
    /** @brief Known plugin list. Each session scans its plugin file into it, and registerEVE() lists the EVE plugin in it at construction. */
    juce::KnownPluginList knownPluginList;
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

    /**
     * @brief Sessions keyed by identity. Each session is owned here.
     *
     * The member is declared after `graph`. A session holds a PluginState that refers to a plugin instance the graph owns.
     * Members are destroyed in the reverse order of their declaration, so the sessions and their PluginState states are
     * released before the graph releases the instances.
     */
    jam::HashMap<jam::UUID, std::unique_ptr<Session>> sessions;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Nexus)
};
