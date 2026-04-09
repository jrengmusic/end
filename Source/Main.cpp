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
 * - **GlassWindow** — the native OS window with optional background blur.
 *
 * ### Startup sequence
 * @code
 * Config ctor        → loads ~/.config/end/end.lua
 * AppState ctor      → initialises defaults (no filesystem access)
 * FontCollection ctor → loads font handles at default size
 * initialise()       → sets nexus mode, loads state.xml if nexus=true,
 *                      creates GlassWindow(new MainComponent())
 * @endcode
 *
 * ### Shutdown sequence
 * `systemRequestedQuit()` is called by the OS (Cmd+Q, window close button, or
 * `JUCEApplication::quit()`).  Before quitting it snapshots the current window
 * dimensions into `state.xml` via `AppState` so the next launch restores the same size.
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
#include "nexus/Session.h"
#include "nexus/Server.h"
#include "nexus/Log.h"
#include "nexus/NexusDaemon.h"

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
 * - `mainWindow` is a `unique_ptr` reset in `shutdown()`.
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
     * appearance from Config, then constructs a `jreng::GlassWindow` wrapping a
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
        const bool isNexusMode { args.contains ("--nexus") };

        auto* cfg { Config::getContext() };

        if (isNexusMode)
        {
            // ---- Headless daemon mode ----------------------------------------
            // Hide dock icon, start IPC server, wire exit callback, and return.
            // No window is created.  The JUCE message loop runs until all sessions exit.
            Nexus::initLog ("end-nexus-daemon.log");
            Nexus::logLine ("daemon: after initLog, calling hideDockIcon");
            Nexus::hideDockIcon();
            Nexus::logLine ("daemon: hideDockIcon done, constructing Session (DaemonTag)");

            nexus = std::make_unique<Nexus::Session> (Nexus::Session::DaemonTag{});
            Nexus::logLine ("daemon: Session (daemon mode) constructed, wiring onAllSessionsExited");

            nexus->onAllSessionsExited = [this]
            {
                quit();
            };
            Nexus::logLine ("daemon: onAllSessionsExited wired, entering message loop");
        }
        else
        {
            Nexus::initLog ("end-nexus-client.log");

            const bool nexusEnabled { cfg->getBool (Config::Key::nexus) };

            if (not nexusEnabled)
            {
                // ---- Single-process mode (nexus = false) --------------------
                // No daemon, no IPC.  Sessions die with the window.
                // Host is alive in-process for session ownership; server is NOT started.
            }

#if JUCE_WINDOWS
            if (isWindows11() and appState.getRendererType() == App::RendererType::cpu)
            {
                applyForceDwmRegistry (cfg->getBool (Config::Key::windowForceDwm));
            }
#endif

            // Nexus mode: set optimistically — window appears before connection settles.
            appState.setNexusMode (nexusEnabled);

            mainWindow.reset (new jreng::GlassWindow (new MainComponent (fontRegistry),
                                                      cfg->getString (Config::Key::windowTitle),
                                                      cfg->getBool (Config::Key::windowAlwaysOnTop),
                                                      cfg->getBool (Config::Key::windowButtons)));

            mainWindow->setGlass (cfg->getColour (Config::Key::windowColour),
                                  Gpu::resolveOpacity (cfg->getFloat (Config::Key::windowOpacity)),
                                  cfg->getFloat (Config::Key::windowBlurRadius));

            // JUCE InterprocessConnection manages its own reader thread internally.
            // No startThread() call is needed.

            mainWindow->setVisible (true);

            if (not nexusEnabled)
            {
                // MainComponent listeners are now registered.  Session ctor creates the
                // PROCESSORS ValueTree child, which fires valueTreeChildAdded and
                // naturally triggers onNexusConnected() on the live listener.
                nexus = std::make_unique<Nexus::Session>();
            }

            if (nexusEnabled)
            {
                // ---- Client mode (nexus = true, no --nexus flag) -------------
                // Window is up immediately.  Session(ClientTag) ctor adds a LOADING child
                // which fires valueTreeChildAdded and shows the loaderOverlay automatically.
                // If no daemon lockfile exists, spawn one first; then connect asynchronously.
                const juce::File lockfilePath { Nexus::Server::getLockfile() };

                bool daemonAlive { false };

                if (lockfilePath.existsAsFile())
                {
                    const int port { lockfilePath.loadFileAsString().trim().getIntValue() };

                    if (port > 0)
                    {
                        juce::StreamingSocket probe;

                        if (probe.connect ("127.0.0.1", port, 200))
                        {
                            probe.close();
                            daemonAlive = true;
                        }
                    }
                }

                if (daemonAlive)
                {
                    Nexus::logLine ("ENDApplication: existing nexus daemon is alive, connecting");
                }
                else
                {
                    if (lockfilePath.existsAsFile())
                    {
                        Nexus::logLine ("ENDApplication: stale lockfile detected, deleting and spawning daemon");
                        lockfilePath.deleteFile();
                    }
                    else
                    {
                        Nexus::logLine ("ENDApplication: no lockfile, spawning daemon");
                    }

                    Nexus::spawnDaemon();
                }

                // Session (ClientTag) constructs Client internally, adds a nexus-connect LOADING op,
                // and begins connect attempts.  When processorList arrives, PROCESSORS is rewritten
                // and the LOADING op is removed.  MainComponent::valueTreeChildAdded reacts to both.
                nexus = std::make_unique<Nexus::Session> (Nexus::Session::ClientTag{});
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
     * Resetting `mainWindow` triggers the full component teardown chain:
     * GlassWindow → MainComponent → Terminal::Display → Session / Screen.
     *
     * @note MESSAGE THREAD — called once at shutdown.
     */
    void shutdown() override
    {
        mainWindow = nullptr;

        // Session dtor handles client disconnect and server stop.
        nexus = nullptr;

        // Destroy the FileLogger before JUCE's leak detector runs.
        Nexus::shutdownLog();

#if JUCE_WINDOWS
        timeEndPeriod (1);
#endif
    }

    //==============================================================================
    /**
     * @brief Handles OS quit requests (Cmd+Q, window close, SIGTERM).
     *
     * Saves `state.xml` (nexus mode only) and quits.  In the byte-forward
     * architecture, the GUI process and the daemon process are separate —
     * quitting the GUI does not affect the daemon, which outlives the GUI
     * until its own shell count hits zero.
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
            {
                appState.setWindowSize (content->getWidth(), content->getHeight());
            }
        }

        appState.save (appState.isNexusMode());
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

    /** @brief In-process session pool. Non-null in daemon mode and single-process mode. Null in client mode. */
    std::unique_ptr<Nexus::Session> nexus;

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
    std::unique_ptr<jreng::GlassWindow> mainWindow;
};

//==============================================================================
// This macro generates the main() routine that launches the app.
START_JUCE_APPLICATION (ENDApplication)
