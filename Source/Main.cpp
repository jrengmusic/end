/**
 * @file Main.cpp
 * @brief Application entry point for END (Ephemeral Nexus Display).
 *
 * Defines ENDApplication, the top-level JUCE application object.  It owns the
 * three long-lived singletons that must outlive every other object:
 *
 * - **lua::Engine** — unified Lua config and scripting engine; registered as a
 *   `jam::Context<lua::Engine>` singleton so any subsystem can call
 *   `lua::Engine::getContext()`.
 * - **FontCollection** — pre-loaded font handles shared across the renderer.
 * - **Window** — the native OS window with optional background blur.
 *
 * ### Startup sequence
 * @code
 * lua::Engine ctor    → loads ~/.config/end/end.lua (requires nexus, display, whelmed, keys, popups, actions modules)
 * AppState ctor       → initDefaults() only (no filesystem access)
 * FontCollection ctor → loads font handles at default size
 * initialise()        → resolves UUID, sets nexus mode, loads state (daemon: full via appState.load(); standalone: window size only),
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
 * @see lua::Engine
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
#include "lua/Engine.h"
#include "action/Action.h"
#include "nexus/Nexus.h"
#include "interprocess/Daemon.h"
#include "interprocess/Link.h"
#include "component/TerminalWindow.h"

#if JUCE_WINDOWS
#include <jam_core/utilities/jam_platform.h>
#endif

//==============================================================================
/**
 * @class ENDApplication
 * @brief Top-level JUCE application object for END.
 *
 * Inherits `juce::JUCEApplication` and implements the four lifecycle hooks
 * required by the JUCE application model.  Member construction order is
 * significant: `luaEngine` must be fully constructed before `appState`
 * (which reads font family and window dims from luaEngine), and both must
 * exist before `initialise()` creates the window.
 *
 * @par Ownership
 * - `luaEngine` and `appState` are value members — they are destroyed last.
 * - `nexus`, `daemon`, `link`, and `mainWindow` are `unique_ptr` members reset
 *   in `shutdown()` in dependency order.
 *
 * @par Thread context
 * All methods are called on the **MESSAGE THREAD** by the JUCE event loop.
 *
 * @see MainComponent
 * @see lua::Engine
 */
class ENDApplication : public juce::JUCEApplication
{
public:
    //==============================================================================
    ENDApplication()
    {
        const auto probeResult { jam::GpuProbe::probe() };
        appState.setGpuAvailable (probeResult.isAvailable);
        appState.setRendererType (lua::Engine::getContext()->nexus.gpu);
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
     * appearance from Config, then constructs a `jam::Window` wrapping a
     * freshly allocated `MainComponent`.
     *
     * @param commandLine  The raw command-line string passed to the process.
     *                     Currently unused; reserved for future shell override.
     *
     * @note MESSAGE THREAD — called once at startup.
     *
     * @see lua::Engine::Display::Window
     */
    void initialise (const juce::String& commandLine) override
    {
        juce::ignoreUnused (commandLine);

#if JUCE_WINDOWS
        // Safety net: create a Job Object with KILL_ON_JOB_CLOSE so that all
        // child processes (shell, OpenConsole.exe from ConPTY) are killed when
        // this process exits — even on crash.  The daemon has its own Job Object
        // via Daemon::installPlatformProcessCleanup(); this covers the GUI
        // (standalone and client) process.  Handle intentionally not stored —
        // the OS closes it on process exit, which triggers the kill.
        {
            HANDLE job { CreateJobObject (nullptr, nullptr) };

            if (job != nullptr)
            {
                JOBOBJECT_EXTENDED_LIMIT_INFORMATION info {};
                info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE
                                                      | JOB_OBJECT_LIMIT_BREAKAWAY_OK;
                SetInformationJobObject (job, JobObjectExtendedLimitInformation, &info, sizeof (info));
                AssignProcessToJobObject (job, GetCurrentProcess());
            }
        }
#endif

        const auto args { getCommandLineParameterArray() };
        const int nexusFlagIndex { args.indexOf ("--nexus") };
        const bool isNexusFlag { nexusFlagIndex >= 0 };

        const auto* cfg { lua::Engine::getContext() };

        if (isNexusFlag)
        {
            const juce::String nexusArg { nexusFlagIndex + 1 < args.size()
                                              ? args[nexusFlagIndex + 1]
                                              : juce::String() };

            if (nexusArg == "kill" or nexusArg == "kill-all")
            {
                // ---- Ephemeral kill command --------------------------------------
                // Connects to the daemon, sends killDaemon PDU, exits.
                // No window, no nexus, no state.

                static constexpr int killProbeTimeoutMs { 200 };

                // Minimal InterprocessConnection for fire-and-forget PDU send.
                struct KillConn : public juce::InterprocessConnection
                {
                    KillConn() : juce::InterprocessConnection (false, Interprocess::wireMagicHeader) {}
                    void connectionMade() override {}
                    void connectionLost() override {}
                    void messageReceived (const juce::MemoryBlock&) override {}
                };

                const juce::File nexusDir {
                    juce::File::getSpecialLocation (juce::File::userHomeDirectory)
                        .getChildFile (".config/end/nexus")
                };

                if (nexusArg == "kill")
                {
                    const juce::String targetUuid { nexusFlagIndex + 2 < args.size()
                                                        ? args[nexusFlagIndex + 2]
                                                        : juce::String() };

                    if (targetUuid.isNotEmpty())
                    {
                        const juce::File nexusFile { nexusDir.getChildFile (targetUuid + ".nexus") };

                        if (nexusFile.existsAsFile())
                        {
                            const int port { nexusFile.loadFileAsString().trim().getIntValue() };

                            if (port > 0)
                            {
                                KillConn conn;

                                if (conn.connectToSocket ("127.0.0.1", port, killProbeTimeoutMs))
                                {
                                    conn.sendMessage (Interprocess::encodePdu (Interprocess::Message::killDaemon, {}));
                                    conn.disconnect();
                                }
                            }
                        }
                    }
                }
                else
                {
                    // kill-all: scan every .nexus file and send killDaemon to each live daemon.
                    const auto nexusFiles { nexusDir.findChildFiles (
                        juce::File::findFiles, false, "*.nexus") };

                    for (const auto& nexusFile : nexusFiles)
                    {
                        const int port { nexusFile.loadFileAsString().trim().getIntValue() };

                        if (port > 0)
                        {
                            KillConn conn;

                            if (conn.connectToSocket ("127.0.0.1", port, killProbeTimeoutMs))
                            {
                                conn.sendMessage (Interprocess::encodePdu (Interprocess::Message::killDaemon, {}));
                                conn.disconnect();
                            }
                        }
                    }
                }

                quit();
            }
            else
            {
                // ---- Headless daemon mode ----------------------------------------
                // nexusArg is the UUID.
                const juce::String daemonUuid { nexusArg.isNotEmpty()
                                                    ? nexusArg
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
        }
        else
        {
            const bool daemonEnabled { cfg->nexus.daemon };

            if (not daemonEnabled)
            {
                // ---- Single-process mode (nexus = false) --------------------
                // No daemon, no IPC.  Standalone persists only window size
                // via loadWindowState/saveWindowState (window.state).
                if (cfg->display.window.saveSize)
                    appState.loadWindowState();
            }

            if (daemonEnabled)
            {
                // ---- Client mode (nexus = true, no --nexus flag) -------------
                const juce::String resolvedUuid { resolveNexusInstance() };
                appState.setInstanceUuid (resolvedUuid);
                appState.setDaemonMode (true);

                const bool hadState { appState.getStateFile().existsAsFile() };
                appState.load();

                if (not hadState and cfg->display.window.saveSize)
                    appState.loadWindowState();
            }

#if JUCE_WINDOWS
            if (isWindows11() and appState.getRendererType() == App::RendererType::cpu)
            {
                jam::BackgroundBlur::applyForceEffectRegistry (cfg->display.window.forceDwm);
            }
#endif

            auto* mainComponent { new MainComponent (luaEngine) };
            mainWindow.reset (new Terminal::Window (mainComponent,
                                                 cfg->display.window.title,
                                                 cfg->display.window.alwaysOnTop,
                                                 cfg->display.window.buttons));

            mainWindow->setGlass (cfg->display.window.colour
                                      .withAlpha (cfg->display.window.opacity),
                                  cfg->display.window.blurRadius);

            // P: applyConfig fires here — after Window exists — so that
            // dynamic_cast<jam::Window*>(getTopLevelComponent()) inside
            // MainComponent::setRenderer succeeds.
            mainComponent->applyConfig();

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

            luaEngine.onReload = [this]
            {
                if (auto* content { dynamic_cast<MainComponent*> (mainWindow->getContentComponent()) })
                {
                    content->applyConfig();

#if JUCE_WINDOWS
                    if (isWindows11() and appState.getRendererType() == App::RendererType::cpu)
                    {
                        jam::BackgroundBlur::applyForceEffectRegistry (
                            lua::Engine::getContext()->display.window.forceDwm);
                    }
#endif

                    const auto* reloadedCfg { lua::Engine::getContext() };
                    mainWindow->setGlass (reloadedCfg->display.window.colour
                                              .withAlpha (reloadedCfg->display.window.opacity),
                                          reloadedCfg->display.window.blurRadius);
                }
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
    }

    //==============================================================================
    /**
     * @brief Handles OS quit requests (Cmd+Q, window close, SIGTERM).
     *
     * Saves window size then quits.  In nexus mode with live sessions, persists
     * UI state so the next client can restore window and tab layout.
     * In nexus mode with no sessions, deletes both `.display` and `.nexus`.
     * In standalone mode, only window size persists (via `saveWindowState`).
     * Sessions die with the window by design.  Main owns all file I/O decisions.
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
            if (lua::Engine::getContext()->display.window.saveSize)
                appState.saveWindowState();
        }

        if (appState.isDaemonMode())
        {
            const int tabCount { appState.getTabs().getNumChildren() };

            if (tabCount > 0)
            {
                // Sessions alive — persist UI state (window, tabs) so the next
                // client can restore layout.  The InterProcessLock auto-releases
                // on quit, signalling the daemon is free to reconnect.
                appState.save();
            }
            else
            {
                // No sessions — clean up both files.
                appState.getStateFile().deleteFile();
                appState.getNexusFile().deleteFile();
            }
        }

        quit();
    }

private:
    /** @brief Unified Lua config and scripting engine. Must be constructed before appState. */
    lua::Engine luaEngine;

    /** @brief Application-level ValueTree. Must be constructed after luaEngine. */
    AppState appState;

    /** @brief Global action registry. Must be constructed after luaEngine. */
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

    /**
     * @brief OS-level lock held while this client is connected to a daemon.
     *
     * Lock name = daemon's UUID.  Acquired in resolveNexusInstance() when claiming
     * a daemon, held for the process lifetime.  Auto-releases on crash or quit.
     * Replaces the crash-unsafe `connected` XML property in `.display`.
     */
    std::unique_ptr<juce::InterProcessLock> clientLock;

    /** @brief The native OS window; null before initialise() and after shutdown(). */
    std::unique_ptr<Terminal::Window> mainWindow;

    /**
     * @brief Scans nexus/\*.nexus files to find a live unclaimed daemon.
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
 * @brief Scans nexus/\*.nexus files to find a live unclaimed daemon.
 *
 * For each .nexus file: tries an InterProcessLock on the UUID (skips if another client
 * holds the lock), probes the port, and returns that UUID if the daemon is alive.
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
        const juce::String candidateUuid { nexusFile.getFileNameWithoutExtension() };
        const juce::File stateFile { nexusDir.getChildFile (candidateUuid + ".display") };

        // Try to claim this daemon via OS-level lock.
        // Lock name = UUID.  If another client holds it, skip.
        auto candidateLock { std::make_unique<juce::InterProcessLock> (candidateUuid) };

        if (candidateLock->enter (0))
        {
            // Lock acquired — no other client owns this daemon.
            // Probe the TCP port to check if the daemon process is alive.
            const int port { nexusFile.loadFileAsString().trim().getIntValue() };
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
                // Claim succeeded — keep the lock for the process lifetime.
                clientLock = std::move (candidateLock);
                appState.setInstanceUuid (candidateUuid);
                appState.get().setProperty (App::ID::port, port, nullptr);
                resolvedUuid = candidateUuid;
            }
            else
            {
                // Stale files — daemon is dead.  Release lock (goes out of scope) and delete.
                nexusFile.deleteFile();
                stateFile.deleteFile();
            }
        }
    }

    if (resolvedUuid.isEmpty())
    {
        // No usable daemon found — generate a fresh UUID, claim it, and spawn.
        resolvedUuid = juce::Uuid().toString();
        clientLock = std::make_unique<juce::InterProcessLock> (resolvedUuid);
        clientLock->enter (0);
        Interprocess::Daemon::spawnDaemon (resolvedUuid);
    }

    return resolvedUuid;
}

//==============================================================================
// This macro generates the main() routine that launches the app.
START_JUCE_APPLICATION (ENDApplication)
