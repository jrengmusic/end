/**
 * @file Main.h
 * @brief Declaration of ENDApplication, the top-level JUCE application object.
 *
 * @see Main.cpp
 */

#pragma once

#include <JuceHeader.h>
#include "Map.h"
#include "MainComponent.h"
#include "AppState.h"
#include "lua/Engine.h"
#include "terminal/action/Action.h"
#include "nexus/Nexus.h"
#include "nexus/Daemon.h"
#include "nexus/Link.h"
#include "terminal/component/TerminalWindow.h"

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
    ENDApplication();

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
    void initialise (const juce::String& commandLine) override;

    /**
     * @brief Destroys the main window and releases all resources.
     *
     * Destruction order:
     * 1. link   — disconnect IPC before sessions die.
     * 2. daemon — stop server.
     * 3. mainWindow — tears down component tree (Display → Processor refs).
     * 4. nexus  — releases all terminal::Session objects.
     *
     * @note MESSAGE THREAD — called once at shutdown.
     */
    void shutdown() override;

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
    void systemRequestedQuit() override;

private:
    /** @brief Application-owned typeface registry and shared glyph atlas. */
    jam::TypefaceResources typefaceResources;

    /** @brief Application-owned style table — self-registers as jam::Stamp::getContext() on construction. */
    jam::Stamp stampContext;

    /** @brief Application-owned grapheme cluster table — self-registers as jam::Grapheme::getContext() on construction. */
    jam::Grapheme graphemeContext;

    /** @brief All Map contexts — must be constructed before luaEngine (parsers use them). */
    Map::Bool boolMap;
    Map::Screen screenMap;
    Map::Cursor cursorMap;
    Map::Gpu gpuMap;
    Map::TabPosition tabPositionMap;
    Map::Renderer rendererMap;
    Map::PaneType paneTypeMap;
    Map::Direction directionMap;
    Map::Position positionMap;
    Map::LinkHandler linkHandlerMap;

    /** @brief Unified Lua config and scripting engine. Must be constructed before appState. */
    lua::Engine luaEngine;

    /** @brief Application-level ValueTree. Must be constructed after luaEngine. */
    AppState appState;

    /** @brief Global action registry. Must be constructed after luaEngine. */
    action::Registry action;

    /** @brief Session pool — owns all terminal::Session objects.
     *  Destroyed after mainWindow — Display must die before Sessions. */
    std::unique_ptr<Nexus> nexus;

    /** @brief The native OS window. Destroyed before nexus (Display → Session dependency). */
    std::unique_ptr<terminal::Window> mainWindow;

    /** @brief IPC server. Non-null in daemon mode only. Destroyed before mainWindow. */
    std::unique_ptr<nexus::Daemon> daemon;

    /** @brief OS-level lock held while connected to a daemon. Auto-releases on quit. */
    std::unique_ptr<juce::InterProcessLock> clientLock;

    /** @brief IPC client connector. Non-null in client mode only. Destroyed first. */
    std::unique_ptr<nexus::Link> link;

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

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ENDApplication)
};
