#pragma once
#include <JuceHeader.h>
#include "config/ConfigModel.h"
#include "end/ENDModel.h"
#include "ui/ENDView.h"
#include "lookAndFeel/ENDLookAndFeel.h"
#include "generated/Generated.h"
#include "plugins/Nexus.h"

class ENDApplication
    : public juce::JUCEApplication
    , private juce::ValueTree::Listener
{
public:
    ENDApplication();
    ~ENDApplication() override;
    const juce::String getApplicationName() override;
    const juce::String getApplicationVersion() override;
    bool moreThanOneInstanceAllowed() override;
    void shutdown() override;
    void systemRequestedQuit() override;

private:
    //==============================================================================
    // Owned global registry aggregate — jam::Bimap<T> owner. Declared before any consumer so the
    // single-global-pointer Instance<T> slot is populated before first use.
    Generated generated;
    //==============================================================================
#if JUCE_DEBUG
    static inline const char* const debugLogFileExtension { ".ode" };

    /** @brief Diagnostic log sink — canonical location: ConfigDirectory::Config::path
     *  (\~/.config/end/end.ode, jam::Format::toFileName), never the launch
     *  cwd — the same deterministic path regardless of how the app was
     *  started (IDE, Finder, terminal), so runtime diagnostics always land
     *  in one known, readable file. */

    jam::debug::Log::Scope logScope { ConfigDirectory::Config::path.getChildFile (
        jam::Format::toFileName (ProjectInfo::projectName, debugLogFileExtension)) };
#endif

#if JAM_USING_PROFILER
    jam::Profiler profiler { jam::Profiler::defaultCapacity };
    std::optional<jam::Profiler::Zone> applicationMembers { std::in_place, Id::applicationMembers };
#endif

    jam::SharedInstance<jam::VulkanShaderFormat> shaderFormat { std::in_place };
    jam::SharedInstance<jam::SharedDocuments> documents { std::in_place };
    ENDModel model;
    ConfigModel config;
    ENDLookAndFeel lookAndFeel;

    //==============================================================================
    /**
     * @brief Settings of END, which hold the window, sessions, and port keys.
     *
     * In server mode the file is files::settingsFileName in the config directory. Otherwise the file is juce::File {}, which
     * names no file: juce::PropertiesFile reads no file and writes none, and the keys live only in memory.
     *
     * getOrCreateWindow() restores the window from the window key of these settings, and destroyWindow() writes the window state to them.
     * host() reads the sessions key, and writes the port key in server mode. shutdown() writes the sessions key.
     */
    juce::PropertiesFile settings { isServer() ? ConfigDirectory::Config::getPath (files::settingsFileName) : juce::File {},
                                    juce::PropertiesFile::Options {} };

    /** @brief Command-line argument that makes the process start as the server. launch() passes it to the process that it starts. */
    static constexpr const char* serverArgument { "--server" };
    /** @brief Suffix that the project name takes to name the lock of the server. */
    static constexpr const char* serverLockSuffix { ".server" };

    /** @brief Inter-process lock that names the one server process. host() takes it in server mode, before the engine and Nexus are built. launch() tests it. */
    juce::InterProcessLock serverLock { juce::String { ProjectInfo::projectName } + serverLockSuffix };
    /** @brief Nexus of the application. host() builds it when the application hosts. */
    std::unique_ptr<Nexus> nexus;
    /** @brief Client connection that launch() opens to the running server. The application owns it, and it is destroyed with the application. */
    std::unique_ptr<jam::ModelConnection> launchConnection;
    /** @brief Window of the application, created by getOrCreateWindow() and destroyed by destroyWindow(). */
    std::unique_ptr<jam::Window> window;

    /** @brief Callbacks for the config properties and tables this application applies, keyed by property, tree type or parent tree type. valueTreePropertyChanged() dispatches through it. */
    ConfigModel::Events events;

    //==============================================================================
    void initialise (const juce::String& commandLine) override;

    /**
     * @brief Starts the server process when no server holds the lock. Otherwise it sends a launch request to the running server, then quits.
     *
     * Without a server, the call releases the lock, starts this executable as a detached process with the server argument,
     * and quits. With a server, the call connects launchConnection to the port that the settings hold, then quits asynchronously.
     */
    void launch();

    /**
     * @brief Answers whether the application runs as the server.
     *
     * initialise(), host(), systemRequestedQuit(), and the settings member all decide the run mode through this call.
     * It reads the server row of the display table of the config on each call, so the answer follows a change of that row.
     *
     * @return True when the server row of the display table is true.
     */
    bool isServer() const noexcept;

    /**
     * @brief Hosts the application as the server or as a client.
     *
     * In server mode, which is the state where isServer() is true, the call takes serverLock before it builds the engine and Nexus.
     * When another process holds the lock, the call quits. Otherwise the call builds the engine and Nexus, then restores the sessions
     * from the settings. It registers the events, writes the port to the settings in server mode, and creates the window.
     */
    void host();

    /**
     * @brief Returns the window, which the call creates when it does not exist.
     *
     * Creating the window makes the ENDView, restores the window state from the settings, shows the window, and applies the
     * background and the post-process chain. The window then listens to the config.
     *
     * @return The window.
     */
    jam::Window& getOrCreateWindow();

    /**
     * @brief Saves the window state to the settings, then destroys the window. The application keeps running.
     *
     * The window stops listening to the config before it is destroyed.
     */
    void destroyWindow();

    /** @brief Creates the engine through jam::VulkanGraphicsEngine::getOrCreate, with the pipeline cache under the
     *  config directory and the GPU setting of the display table, then calls initialiseStyle() of the look and feel.
     *  Called once from initialise(). The background shader and the post-process chain are applied later in
     *  initialise(), after the window exists. */
    void initialiseVulkan();

    void
    valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property) override;

    /**
     * @brief Brings the window to the front when an end row is added under the model root.
     *
     * A second process requests a launch by sending its whole model root through launchConnection. The server attaches
     * that root under its model root as an end row, and this call then shows the window. On macOS, the call also shows the
     * dock icon and makes the process the foreground process.
     *
     * @param parentTree  Tree that gained the child.
     * @param child       Added child.
     */
    void valueTreeChildAdded (juce::ValueTree& parentTree, juce::ValueTree& child) override;

    /** @brief Populates @ref events with the callbacks for the config values this application applies: the always-on-top
     *  and title-bar button settings of the window, the theme, GPU use, background, filter, post-process, and the two
     *  post-process render parameters. */
    void registerEvents();

    /** @brief Compiles the configured background shader and installs it on the view, or clears the background when the GPU is unavailable or no project is named. */
    void setBackground();

    /** @brief Compiles the configured post-process chain and installs it on the engine with the current opacity and resolution scale, or installs no chain when the GPU is unavailable or no project is named. */
    void setPostProcess();

    /** @brief Applies the post-process opacity and resolution scale from config to the chain the engine already holds. */
    void setPostProcessParams();

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ENDApplication)
};
