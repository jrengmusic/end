/**
 * @file Main.cpp
 * @brief Application entry point for END (Ephemeral Nexus Display).
 *
 * Defines ENDApplication, the top-level JUCE application object.  It owns the
 * three long-lived singletons that must outlive every other object:
 *
 * - **Config** — Lua config loader; registered as a `jreng::Context<Config>`
 *   singleton so any subsystem can call `Config::getContext()`.
 * - **FontCollection** — pre-loaded font handles shared across the renderer.
 * - **Window** — the native OS window with optional background blur.
 *
 * ### Startup sequence
 * @code
 * Config ctor         → loads ~/.config/end/end.lua
 * AppState ctor       → initDefaults() only (no filesystem access)
 * FontCollection ctor → loads font handles at default size
 * initialise()        → resolves UUID, sets nexus mode, calls appState.load() (reads full state from mode-appropriate file),
 *                       creates Window(new MainComponent()), then Nexus + Daemon/Link
 * @endcode
 *
 * ### Shutdown sequence
 * `systemRequestedQuit()` is called by the OS (Cmd+Q, window close button, or
 * `JUCEApplication::quit()`).  It owns all file decisions: saves state in standalone
 * and nexus-with-sessions modes; deletes both nexus files when no sessions remain.
 *
 * @note The `START_JUCE_APPLICATION` macro at the bottom generates the platform
 *       `main()` / `WinMain()` entry point.
 *
 * @see MainComponent
 * @see Config
 * @see FontCollection
 */

/*
  ==============================================================================

    END - Ephemeral Nexus Display
    GPU-accelerated terminal emulator

    Main.cpp - Application entry point

  ==============================================================================
*/

#include <JuceHeader.h>
#include "MainComponent.h"
#include "AppState.h"
#include "Gpu.h"
#include "config/Config.h"
#include "config/WhelmedConfig.h"
#include "action/Action.h"
#include "nexus/Nexus.h"
#include "interprocess/Daemon.h"
#include "interprocess/Link.h"

#if JUCE_WINDOWS
#include <windows.h>
#include "../modules/jreng_core/utilities/jreng_platform.h"
#pragma comment(lib, "winmm.lib")
extern "C" __declspec (dllimport) unsigned int __stdcall timeBeginPeriod (unsigned int uPeriod);
extern "C" __declspec (dllimport) unsigned int __stdcall timeEndPeriod (unsigned int uPeriod);
#endif

#if JUCE_WINDOWS

/** @brief DWM corner preference value: DWMWCP_ROUND (rounded corners). */
static constexpr DWORD dwmForceEffectEnabled { 2 };

/**
 * @brief Applies or removes the DWM ForceEffectMode registry key.
 *
 * On Windows 11 VMs (detected via software GL renderer), DWM disables
 * rounded corners by default.  Setting ForceEffectMode=2 re-enables them.
 *
 * @param enable  true → set DWORD to 2; false → delete the value.
 *
 * @note Requires elevated privileges (HKEY_LOCAL_MACHINE).
 *       Changes take effect after END restarts.
 */
static void applyForceDwmRegistry (bool enable) noexcept
{
    HKEY hKey { nullptr };
    const auto opened { RegOpenKeyExW (
        HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\Dwm", 0, KEY_SET_VALUE, &hKey) };

    if (opened == ERROR_SUCCESS)
    {
        if (enable)
        {
            DWORD value { dwmForceEffectEnabled };
            RegSetValueExW (
                hKey, L"ForceEffectMode", 0, REG_DWORD, reinterpret_cast<const BYTE*> (&value), sizeof (value));
        }
        else
        {
            RegDeleteValueW (hKey, L"ForceEffectMode");
        }

        RegCloseKey (hKey);
    }
}

#endif

//==============================================================================
/**
 * @class ENDApplication
 * @brief Top-level JUCE application object for END.
 *
 * Inherits `juce::JUCEApplication` and implements the four lifecycle hooks
 * required by the JUCE application model.  Member construction order is
 * significant: `config` must be fully constructed before `fontCollection`
 * (which reads font family from config), and both must exist before
 * `initialise()` creates the window.
 *
 * @par Ownership
 * - `config` and `fontCollection` are value members — they are destroyed last.
 * - `nexus`, `daemon`, `link`, and `mainWindow` are `unique_ptr` members reset
 *   in `shutdown()` in dependency order.
 *
 * @par Thread context
 * All methods are called on the **MESSAGE THREAD** by the JUCE event loop.
 *
 * @see MainComponent
 * @see Config
 */
class ENDApplication : public juce::JUCEApplication
{
public:
    //==============================================================================
    ENDApplication()
    {
        const auto probeResult { Gpu::probe() };
        appState.setGpuAvailable (probeResult.isAvailable);
        appState.setRendererType (Config::getContext()->getString (Config::Key::gpuAcceleration));
    }

    /** @return The human-readable application name from ProjectInfo. */
    const juce::String getApplicationName() override { return ProjectInfo::projectName; }

    /** @return The version string from ProjectInfo (e.g. "1.0.0"). */
    const juce::String getApplicationVersion() override { return ProjectInfo::versionString; }

    /**
     * @return @c true — END supports multiple simultaneous instances.
     * @note Each instance owns its own pty session and window.
     */
    bool moreThanOneInstanceAllowed() override { return true; }

    //==============================================================================
    /**
     * @brief Creates the main window and wires up all subsystems.
     *
     * Called by JUCE after the message loop starts.  Reads window geometry and
     * appearance from Config, then constructs a `jreng::Window` wrapping a
     * freshly allocated `MainComponent`.
     *
     * @param commandLine  The raw command-line string passed to the process.
     *                     Currently unused; reserved for future shell override.
     *
     * @note MESSAGE THREAD — called once at startup.
     *
     * @see Config::Key::windowTitle
     * @see Config::Key::windowColour
     * @see Config::Key::windowOpacity
     * @see Config::Key::windowBlurRadius
     * @see Config::Key::windowAlwaysOnTop
     * @see Config::Key::windowButtons
     */
    void initialise (const juce::String& commandLine) override
    {
        juce::ignoreUnused (commandLine);

#if JUCE_WINDOWS
        // Unlock 1 ms timer resolution so the 8 ms flush timer fires at its
        // intended rate rather than the default 15 ms WM_TIMER granularity.
        // Threat: coarse timer resolution causes cursor-position update latency
        // and visible cursor twitching at nominal 120 Hz.
        timeBeginPeriod (1);
#endif

        const auto args { getCommandLineParameterArray() };
        const int nexusFlagIndex { args.indexOf ("--nexus") };
        const bool isNexusFlag { nexusFlagIndex >= 0 };

        auto* cfg { Config::getContext() };

        if (isNexusFlag)
        {
            // ---- Headless daemon mode ----------------------------------------
            // Parse UUID from the argument immediately following --nexus.
            // Set UUID on AppState before any state file access.
            const juce::String daemonUuid { nexusFlagIndex + 1 < args.size()
                                                ? args[nexusFlagIndex + 1]
                                                : juce::Uuid().toString() };
            appState.setInstanceUuid (daemonUuid);
            appState.load();

            // Ensure nexus/ directory exists before the server writes its port.
            appState.getNexusFile().getParentDirectory().createDirectory();

            // Hide dock icon, construct nexus + daemon, attach, start, wire exit callback.
            // No window is created.  The JUCE message loop runs until all sessions exit.
            Interprocess::Daemon::hideDockIcon();
            nexus = std::make_unique<Nexus>();
            daemon = std::make_unique<Interprocess::Daemon> (*nexus);
            nexus->attach (*daemon);
            daemon->start();

            daemon->onAllSessionsExited = [this]
            {
                appState.deleteNexusFile();
                quit();
            };
        }
        else
        {
            const bool daemonEnabled { cfg->getBool (Config::Key::daemon) };

            if (not daemonEnabled)
            {
                // ---- Single-process mode (nexus = false) --------------------
                // No daemon, no IPC.  Load full state from end.state.
                appState.load();
            }

            if (daemonEnabled)
            {
                // ---- Client mode (nexus = true, no --nexus flag) -------------
                const juce::String resolvedUuid { resolveNexusInstance() };
                appState.setInstanceUuid (resolvedUuid);
                appState.setDaemonMode (true);
                appState.load();
            }

#if JUCE_WINDOWS
            if (isWindows11() and appState.getRendererType() == App::RendererType::cpu)
            {
                applyForceDwmRegistry (cfg->getBool (Config::Key::windowForceDwm));
            }
#endif

            mainWindow.reset (new jreng::Window (new MainComponent (fontRegistry),
                                                      cfg->getString (Config::Key::windowTitle),
                                                      cfg->getBool (Config::Key::windowAlwaysOnTop),
                                                      cfg->getBool (Config::Key::windowButtons)));

            mainWindow->setGlass (cfg->getColour (Config::Key::windowColour),
                                  Gpu::resolveOpacity (cfg->getFloat (Config::Key::windowOpacity)),
                                  cfg->getFloat (Config::Key::windowBlurRadius));

            // JUCE InterprocessConnection manages its own reader thread internally.
            // No startThread() call is needed.

            mainWindow->setVisible (true);

            nexus = std::make_unique<Nexus>();

            if (not daemonEnabled)
            {
                // Standalone mode — MainComponent listeners are now registered.
                // Append SESSIONS child to trigger valueTreeChildAdded → initialiseTabs.
                juce::ValueTree sessionsNode { App::ID::SESSIONS };
                appState.getNexusNode().appendChild (sessionsNode, nullptr);
            }
            else
            {
                // Client mode — construct Link, attach to nexus, begin connect attempts.
                // When the sessions PDU arrives, SESSIONS is rewritten and the LOADING
                // op is removed.  MainComponent::valueTreeChildAdded reacts to both.
                link = std::make_unique<Interprocess::Link>();
                nexus->attach (*link);
                link->beginConnectAttempts();
            }

            juce::MessageManager::callAsync (
                [this]
                {
                    if (auto* content { mainWindow->getContentComponent() })
                        content->grabKeyboardFocus();
                });

            config.onReload = [this]
            {
                if (auto* content { dynamic_cast<MainComponent*> (mainWindow->getContentComponent()) })
                {
                    content->applyConfig();

#if JUCE_WINDOWS
                    if (isWindows11() and appState.getRendererType() == App::RendererType::cpu)
                    {
                        applyForceDwmRegistry (config.getBool (Config::Key::windowForceDwm));
                    }
#endif

                    mainWindow->setGlass (config.getColour (Config::Key::windowColour),
                                          Gpu::resolveOpacity (config.getFloat (Config::Key::windowOpacity)),
                                          config.getFloat (Config::Key::windowBlurRadius));
                }
            };

            whelmedConfig.onReload = [this]
            {
                if (auto* content { dynamic_cast<MainComponent*> (mainWindow->getContentComponent()) })
                    content->applyConfig();
            };
        }
    }

    /**
     * @brief Destroys the main window and releases all resources.
     *
     * Destruction order:
     * 1. link   — disconnect IPC before sessions die.
     * 2. daemon — stop server.
     * 3. mainWindow — tears down component tree (Display → Processor refs).
     * 4. nexus  — releases all Terminal::Session objects.
     *
     * @note MESSAGE THREAD — called once at shutdown.
     */
    void shutdown() override
    {
        link = nullptr;
        daemon = nullptr;
        mainWindow = nullptr;
        nexus = nullptr;

#if JUCE_WINDOWS
        timeEndPeriod (1);
#endif
    }

    //==============================================================================
    /**
     * @brief Handles OS quit requests (Cmd+Q, window close, SIGTERM).
     *
     * Saves window size then quits.  In nexus mode with live sessions, marks the
     * client as disconnected and persists so the next client sees `connected=false`.
     * In nexus mode with no sessions, deletes both `.display` and `.nexus`.
     * In standalone mode, always saves.  Main owns all file I/O decisions.
     * In the byte-forward architecture the GUI process and the daemon process are
     * separate — quitting the GUI does not affect the daemon, which outlives the
     * GUI until its own shell count hits zero.
     *
     * @note MESSAGE THREAD — called by the OS or by `JUCEApplication::quit()`.
     *
     * @see AppState::save
     */
    void systemRequestedQuit() override
    {
        // UI process always quits unconditionally.
        // - nexus = true (client mode): daemon lives on in its own process.
        // - nexus = false (single-process): sessions die with the window by design.
        // - --nexus (daemon mode): OS quit means all sessions should die; message loop
        //   exits via onAllSessionsExited after sessions are destroyed.
        if (mainWindow != nullptr)
        {
            if (auto* content { mainWindow->getContentComponent() })
                appState.setWindowSize (content->getWidth(), content->getHeight());
        }

        if (appState.isDaemonMode())
        {
            const int tabCount { appState.getTabs().getNumChildren() };

            if (tabCount > 0)
            {
                // Sessions alive — mark disconnected and persist so the next client
                // reads connected=false and knows the daemon is free to reconnect.
                appState.setConnected (false);
                appState.save();
            }
            else
            {
                // No sessions — clean up both files.
                appState.getStateFile().deleteFile();
                appState.getNexusFile().deleteFile();
            }
        }
        else
        {
            // Standalone mode — always save on quit.
            appState.save();
        }

        quit();
    }

private:
    /** @brief Lua config singleton. Must be constructed before appState and fontCollection. */
    Config config;

    /** @brief Whelmed Lua config singleton. Must be constructed before appState and fontCollection. */
    Whelmed::Config whelmedConfig;

    /** @brief Application-level ValueTree. Must be constructed after config. */
    AppState appState;

    /** @brief Pre-loaded font handles shared by the renderer. */
    jreng::Typeface::Registry fontRegistry;

    /** @brief Global action registry. Must be constructed after Config. */
    Action::Registry action;

    /**
     * @brief Session pool — owns all Terminal::Session objects.
     *
     * Non-null in all three modes.  Destroyed after mainWindow (declared before
     * mainWindow so member destruction — reverse declaration order — runs it last).
     */
    std::unique_ptr<Nexus> nexus;

    /**
     * @brief IPC server.  Non-null in daemon mode (--nexus flag) only.
     *
     * Destroyed in shutdown() before mainWindow.
     */
    std::unique_ptr<Interprocess::Daemon> daemon;

    /**
     * @brief IPC client connector.  Non-null in client mode (nexus=true, no --nexus flag) only.
     *
     * Destroyed in shutdown() before mainWindow so the link is torn down before
     * component destruction races with incoming IPC callbacks.
     */
    std::unique_ptr<Interprocess::Link> link;

    /** @brief Embedded Display Mono typefaces; held alive for DirectWrite on Windows. */
    struct DisplayMono
    {
        static inline auto book { juce::Typeface::createSystemTypefaceFor (BinaryData::DisplayMonoBook_ttf,
                                                                           BinaryData::DisplayMonoBook_ttfSize) };
        static inline auto medium { juce::Typeface::createSystemTypefaceFor (BinaryData::DisplayMonoMedium_ttf,
                                                                             BinaryData::DisplayMonoMedium_ttfSize) };
        static inline auto bold { juce::Typeface::createSystemTypefaceFor (BinaryData::DisplayMonoBold_ttf,
                                                                           BinaryData::DisplayMonoBold_ttfSize) };
    };

    /** @brief Embedded Display proportional typefaces; held alive for DirectWrite on Windows. */
    struct DisplayProp
    {
        static inline auto book { juce::Typeface::createSystemTypefaceFor (BinaryData::DisplayBook_ttf,
                                                                           BinaryData::DisplayBook_ttfSize) };
        static inline auto medium { juce::Typeface::createSystemTypefaceFor (BinaryData::DisplayMedium_ttf,
                                                                             BinaryData::DisplayMedium_ttfSize) };
        static inline auto bold { juce::Typeface::createSystemTypefaceFor (BinaryData::DisplayBold_ttf,
                                                                           BinaryData::DisplayBold_ttfSize) };
    };

    /** @brief The native OS window; null before initialise() and after shutdown(). */
    std::unique_ptr<jreng::Window> mainWindow;

    /**
     * @brief Scans nexus/*.nexus files to find a live daemon without a connected client.
     *
     * Returns the UUID of the first usable daemon, or spawns a new daemon and returns
     * its fresh UUID if no usable daemon is found.  Deletes stale .nexus/.display file
     * pairs where the daemon process is no longer alive.
     *
     * @return UUID string to use for this client session.
     * @note MESSAGE THREAD.
     */
    juce::String resolveNexusInstance();
};

//==============================================================================

/**
 * @brief Scans nexus/*.nexus files to find a live daemon without a connected client.
 *
 * For each .nexus file: reads the connected flag from the corresponding .display file
 * (skips if connected=true), probes the port, and returns that UUID if the daemon is alive.
 * Deletes stale .nexus/.display pairs where the daemon is dead.
 * If no usable daemon is found, spawns a fresh one and returns its UUID.
 *
 * @return UUID string to use for this client session.
 * @note MESSAGE THREAD.
 */
juce::String ENDApplication::resolveNexusInstance()
{
    static constexpr int nexusProbeTimeoutMs { 200 };

    const juce::File nexusDir {
        juce::File::getSpecialLocation (juce::File::userHomeDirectory)
            .getChildFile (".config/end/nexus")
    };

    nexusDir.createDirectory();

    juce::String resolvedUuid;

    const auto nexusFiles { nexusDir.findChildFiles (
        juce::File::findFiles, false, "*.nexus") };

    for (int i { 0 }; resolvedUuid.isEmpty() and i < nexusFiles.size(); ++i)
    {
        const juce::File& nexusFile { nexusFiles.getReference (i) };

        // UUID is the filename stem: <uuid>.nexus
        const juce::String candidateUuid { nexusFile.getFileNameWithoutExtension() };

        // Check <uuid>.display for the connected flag (client-owned file).
        // If connected=true another client owns this daemon — skip.
        const juce::File stateFile { nexusDir.getChildFile (candidateUuid + ".display") };

        bool connected { false };

        if (stateFile.existsAsFile())
        {
            if (auto xml { juce::parseXML (stateFile) })
            {
                auto parsed { juce::ValueTree::fromXml (*xml) };

                if (parsed.isValid() and parsed.getType() == App::ID::END)
                    connected = static_cast<bool> (parsed.getProperty (App::ID::connected, false));
            }
        }

        if (not connected)
        {
            // Read port as plain text from <uuid>.nexus (daemon-owned file).
            const int port { nexusFile.loadFileAsString().getIntValue() };

            bool daemonAlive { false };

            if (port > 0)
            {
                juce::StreamingSocket probe;

                if (probe.connect ("127.0.0.1", port, nexusProbeTimeoutMs))
                {
                    probe.close();
                    daemonAlive = true;
                }
            }

            if (daemonAlive)
            {
                // Store the port in-memory so Link can connect.
                appState.setInstanceUuid (candidateUuid);
                appState.get().setProperty (App::ID::port, port, nullptr);
                resolvedUuid = candidateUuid;
            }
            else
            {
                // Stale files — daemon is dead.  Delete both.
                nexusFile.deleteFile();
                stateFile.deleteFile();
            }
        }
    }

    if (resolvedUuid.isEmpty())
    {
        // No usable daemon found — generate a fresh UUID and spawn a daemon.
        resolvedUuid = juce::Uuid().toString();
        Interprocess::Daemon::spawnDaemon (resolvedUuid);
    }

    return resolvedUuid;
}

//==============================================================================
// This macro generates the main() routine that launches the app.
START_JUCE_APPLICATION (ENDApplication)
