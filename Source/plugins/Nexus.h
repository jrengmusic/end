#pragma once
#include <JuceHeader.h>
#if JUCE_MAC
#include <CoreFoundation/CoreFoundation.h>
#endif
#include "plugins/Session.h"
#include "config/ConfigDirectory.h"

/**
 * @class Nexus
 * @brief Application root of END: the audio graph, the plugin formats, the
 *        model of the sessions, and the host extensions that the hosted plugins read.
 *
 * One graph and one format manager serve every session. Each session holds the
 * hosted plugin of each of its panes, inside this graph.
 */
class Nexus : public jam::Instance<Nexus>,
              private juce::Thread
{
public:
    /**
     * @brief Registers the host extensions, builds the model, the plugin formats and the audio graph,
     *        then lists the plugin bundle of the binary data when a plugin format reads it.
     *
     * The constructor calls registerEVE(). That call gets the plugin bundle from getOrCreatePlugin(), which unpacks
     * the embedded archive when the plugins stamp is stale or the bundle is absent. On macOS, registerEVE() registers
     * the bundle as an Audio Unit component when the bundle file has the Audio Unit extension.
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
     *
     * The call uses the `at` lookup of the session map, so it fails when no session has the identity.
     *
     * @param sessionUuid  Identity of the session.
     * @return The session.
     */
    Session& getSession (jam::UUID sessionUuid) const;

    /**
     * @brief Returns the focused session, which the `focusedSession` parameter names.
     *
     * The call asserts that the parameter is not zero. The parameter is zero until the first session is created.
     *
     * @return The focused session.
     */
    Session& getActiveSession() const;

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

    /**
     * @brief Unpacks the embedded plugin archive when needed, then returns the plugin bundle that the archive carries.
     *
     * The binary data embeds exactly one plugin archive, of the host platform, because the configure step fails otherwise.
     * The first entry of the archive is the bundle, so the bundle name comes from the archive. The archive is unpacked
     * into the plugins folder of the config when the plugins stamp differs from the plugin hash, which the build derives
     * from the archive files, or when the bundle is absent. The stamp is replaced with the plugin hash only when it
     * differed and the bundle exists.
     *
     * @return The plugin bundle in the plugins folder of the config, or an empty file when the unpack fails or the
     *         binary data embeds no archive.
     */
    juce::File getOrCreatePlugin();

#if JAM_USING_PROFILER
    /**
     * @brief Returns the command-line file of the process, resolved against the working folder.
     * @return The file that the first command-line argument names.
     */
    static juce::File getManifestFile();

    /** @brief Callback that the prompt-painted extension invokes when the profiler prompt is painted. ENDView sets it. */
    std::function<void()> onPromptPainted;
#endif

private:
    void run() override;

    /**
     * @brief Makes the bundle of one embedded plugin archive available in the plugins folder, unpacking the archive only when needed.
     *
     * The archive carries its bundle as the first entry, so the bundle path is known before any unpack. When the archive
     * is offloaded and its bundle exists, the bundle is returned without unpacking. Otherwise every file of the archive is
     * unpacked into the plugins folder, and existing files are overwritten. A debug build asserts when the unpack fails.
     *
     * @param resourceName       Name of the archive resource in the binary data.
     * @param pluginsDirectory   Plugins folder of the config that receives the unpacked files.
     * @param isOffloaded        True when the plugins stamp matches the plugin hash.
     * @return The bundle in pluginsDirectory, or an empty file when the unpack fails.
     */
    static juce::File getOrCreatePlugin (const char* resourceName, const juce::File& pluginsDirectory, bool isOffloaded);

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
     * The CLAP format receives the extensions, the product name, the company name, the host URL, the version
     * and the plugins folder of the config.
     */
    void addFormats();

    /**
     * @brief Adds the EVE plugin description to the known plugin list, when the plugin formats read the EVE file.
     *
     * The description carries the modification time of the EVE file. On macOS, an EVE bundle with the Audio Unit
     * extension is also registered as an audio component.
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
