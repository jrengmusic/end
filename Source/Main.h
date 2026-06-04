#pragma once
#include <JuceHeader.h>
#include "config/Config.h"
#include "EndView.h"

namespace end
{
/*____________________________________________________________________________*/

class Application
    : public juce::JUCEApplication
    , public jam::File::Watcher::Listener
{
public:
    Application();
    /** @return The human-readable application name from ProjectInfo. */
    const juce::String getApplicationName() override;
    /** @return The version string from ProjectInfo (e.g. "1.0.0"). */
    const juce::String getApplicationVersion() override;

    /**
     * @return @c true — END supports multiple simultaneous instances.
     * @note Each instance owns its own pty session and window.
     */
    bool moreThanOneInstanceAllowed() override;
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
     * @see AppModel::save
     */
    void systemRequestedQuit() override;

private:
    void fileChanged (const juce::File& file, jam::File::Watcher::Event event) override;

    //==============================================================================
    config::Model config;
    std::unique_ptr<jam::Window> window;

    //==============================================================================
#if JUCE_DEBUG
    /** @brief Diagnostic log scope — constructed first, destroyed last.
     *  @note TEMP diagnostic — re-introduced for assertion hunt; remove after fix (Track 1 Step 10 cleanup target). */
    jam::debug::Log::Scope logScope { juce::File::getCurrentWorkingDirectory().getChildFile (
        jam::Text::toFileName (ProjectInfo::projectName, ".ode")) };
#endif

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Application)
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace end
