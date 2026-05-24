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

#include "Main.h"

//==============================================================================

ENDApplication::ENDApplication()
{
    const auto probeResult { jam::GpuProbe::probe() };
    appState.setGpuAvailable (probeResult.isAvailable);
    appState.setRendererType (lua::Engine::getContext()->nexus.gpu);
}

//==============================================================================

void ENDApplication::initialise (const juce::String& commandLine)
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
            info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE | JOB_OBJECT_LIMIT_BREAKAWAY_OK;
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
        const juce::String nexusArg { nexusFlagIndex + 1 < args.size() ? args[nexusFlagIndex + 1] : juce::String() };

        if (nexusArg == "kill" or nexusArg == "kill-all")
        {
            // ---- Ephemeral kill command --------------------------------------
            // Connects to the daemon, sends killDaemon PDU, exits.
            // No window, no nexus, no state.

            static constexpr int killProbeTimeoutMs { 200 };

            // Minimal InterprocessConnection for fire-and-forget PDU send.
            struct KillConn : public juce::InterprocessConnection
            {
                KillConn()
                    : juce::InterprocessConnection (false, nexus::wireMagicHeader)
                {
                }
                void connectionMade() override {}
                void connectionLost() override {}
                void messageReceived (const juce::MemoryBlock&) override {}
            };

            const juce::File nexusDir { lua::Engine::getConfigPath().getChildFile ("nexus") };

            if (nexusArg == "kill")
            {
                const juce::String targetUuid { nexusFlagIndex + 2 < args.size() ? args[nexusFlagIndex + 2]
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
                                conn.sendMessage (nexus::encodePdu (nexus::Message::killDaemon, {}));
                                conn.disconnect();
                            }
                        }
                    }
                }
            }
            else
            {
                // kill-all: scan every .nexus file and send killDaemon to each live daemon.
                const auto nexusFiles { nexusDir.findChildFiles (juce::File::findFiles, false, "*.nexus") };

                for (const auto& nexusFile : nexusFiles)
                {
                    const int port { nexusFile.loadFileAsString().trim().getIntValue() };

                    if (port > 0)
                    {
                        KillConn conn;

                        if (conn.connectToSocket ("127.0.0.1", port, killProbeTimeoutMs))
                        {
                            conn.sendMessage (nexus::encodePdu (nexus::Message::killDaemon, {}));
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
            const juce::String daemonUuid { nexusArg.isNotEmpty() ? nexusArg : juce::Uuid().toString() };
            appState.setInstanceUuid (daemonUuid);
            appState.load();

            // Ensure nexus/ directory exists before the server writes its port.
            appState.getNexusFile().getParentDirectory().createDirectory();

            // Hide dock icon, construct nexus + daemon, attach, start, wire exit callback.
            // No window is created.  The JUCE message loop runs until all sessions exit.
            nexus::Daemon::hideDockIcon();
            nexus = std::make_unique<Nexus>();
            nexus->setMode (Nexus::Mode::daemon);
            daemon = std::make_unique<nexus::Daemon> (*nexus);
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
        if (isWindows11() and appState.getRendererType() == app::RendererType::cpu)
        {
            jam::BackgroundBlur::applyForceEffectRegistry (cfg->display.window.forceDwm);
        }
#endif

        auto* mainComponent { new MainComponent (luaEngine) };
        mainWindow.reset (new terminal::Window (
            mainComponent, cfg->display.window.title, cfg->display.window.alwaysOnTop, cfg->display.window.buttons));

        mainWindow->setGlass (
            cfg->display.window.colour.withAlpha (cfg->display.window.opacity), cfg->display.window.blurRadius);

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
            juce::ValueTree sessionsNode { app::id::SESSIONS };
            appState.getNexusNode().appendChild (sessionsNode, nullptr);
        }
        else
        {
            // Client mode — construct Link, begin connect attempts.
            // Link registers on nexus.events in its constructor.
            // When the sessions PDU arrives, SESSIONS is rewritten and the LOADING
            // op is removed.  MainComponent::valueTreeChildAdded reacts to both.
            nexus->setMode (Nexus::Mode::client);
            link = std::make_unique<nexus::Link>();
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
                content->showReloadMessage();

#if JUCE_WINDOWS
                if (isWindows11() and appState.getRendererType() == app::RendererType::cpu)
                {
                    jam::BackgroundBlur::applyForceEffectRegistry (lua::Engine::getContext()->display.window.forceDwm);
                }
#endif

                const auto* reloadedCfg { lua::Engine::getContext() };
                mainWindow->setGlass (
                    reloadedCfg->display.window.colour.withAlpha (reloadedCfg->display.window.opacity),
                    reloadedCfg->display.window.blurRadius);
            }
        };
    }
}

//==============================================================================

void ENDApplication::shutdown() {}

//==============================================================================

void ENDApplication::systemRequestedQuit()
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

    const juce::File nexusDir { lua::Engine::getConfigPath().getChildFile ("nexus") };

    nexusDir.createDirectory();

    juce::String resolvedUuid;

    const auto nexusFiles { nexusDir.findChildFiles (juce::File::findFiles, false, "*.nexus") };

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
                appState.setValue (app::id::port, port);
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
        nexus::Daemon::spawnDaemon (resolvedUuid);
    }

    return resolvedUuid;
}

//==============================================================================
// This macro generates the main() routine that launches the app.
START_JUCE_APPLICATION (ENDApplication)
