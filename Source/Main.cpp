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
 * AppState ctor      → loads ~/.config/end/state.xml (or creates defaults)
 * FontCollection ctor → loads font handles at default size
 * initialise()       → creates GlassWindow(new MainComponent())
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
#include "config/Config.h"
#include "terminal/rendering/FontCollection.h"

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
    ENDApplication() = default;

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
        auto* cfg { Config::getContext() };

        mainWindow.reset (new jreng::GlassWindow (
            new MainComponent(),
            cfg->getString (Config::Key::windowTitle),
            cfg->getColour (Config::Key::windowColour),
            cfg->getFloat (Config::Key::windowOpacity),
            cfg->getFloat (Config::Key::windowBlurRadius),
            cfg->getBool (Config::Key::windowAlwaysOnTop),
            cfg->getBool (Config::Key::windowButtons)));
    }

    /**
     * @brief Destroys the main window and releases all resources.
     *
     * Resetting `mainWindow` triggers the full component teardown chain:
     * GlassWindow → MainComponent → Terminal::Component → Session / Screen.
     *
     * @note MESSAGE THREAD — called once at shutdown.
     */
    void shutdown() override { mainWindow = nullptr; }

    //==============================================================================
    /**
     * @brief Handles OS quit requests (Cmd+Q, window close, SIGTERM).
     *
     * Saves the application state to `state.xml` via `AppState` before
     * calling `quit()`, so the next launch restores the same layout.
     *
     * @note MESSAGE THREAD — called by the OS or by `JUCEApplication::quit()`.
     *
     * @see AppState::save
     */
    void systemRequestedQuit() override
    {
        if (mainWindow != nullptr)
        {
            if (auto* content { mainWindow->getContentComponent() })
            {
                appState.setWindowSize (content->getWidth(), content->getHeight());
            }
        }

        appState.save();
        quit();
    }

private:
    /** @brief Lua config singleton. Must be constructed before appState and fontCollection. */
    Config config;

    /** @brief Application-level ValueTree. Must be constructed after config. */
    AppState appState;

    /** @brief Pre-loaded font handles shared by the renderer. */
    FontCollection fontCollection;

    /** @brief Embedded Display Mono typefaces; held alive for DirectWrite on Windows. */
    struct DisplayMono
    {
        static inline auto book   { juce::Typeface::createSystemTypefaceFor (BinaryData::DisplayMonoBook_ttf,   BinaryData::DisplayMonoBook_ttfSize) };
        static inline auto medium { juce::Typeface::createSystemTypefaceFor (BinaryData::DisplayMonoMedium_ttf, BinaryData::DisplayMonoMedium_ttfSize) };
        static inline auto bold   { juce::Typeface::createSystemTypefaceFor (BinaryData::DisplayMonoBold_ttf,   BinaryData::DisplayMonoBold_ttfSize) };
    };

    /** @brief The native OS window; null before initialise() and after shutdown(). */
    std::unique_ptr<jreng::GlassWindow> mainWindow;
};

//==============================================================================
// This macro generates the main() routine that launches the app.
START_JUCE_APPLICATION (ENDApplication)
