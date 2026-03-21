/**
 * @file MainComponent.h
 * @brief Root JUCE component that hosts the terminal UI.
 *
 * MainComponent is the content component of the application's `GlassWindow`.
 * It owns a `Terminal::Tabs` child that manages multiple terminal sessions,
 * and paints the window background with the configured colour and opacity.
 *
 * ### Responsibilities
 * - Sets the initial window size from `AppState` (persisted in `state.xml`).
 * - Registers a close callback with `jreng::BackgroundBlur` so that window
 *   dimensions are persisted to `AppState` when the native close button is
 *   pressed (in addition to the Cmd+Q path handled by ENDApplication).
 * - Delegates all keyboard, mouse, and terminal I/O to `Terminal::Tabs`.
 * - Registers all user-performable action callbacks with `Terminal::Action`.
 *
 * @par Thread context
 * All methods are called on the **MESSAGE THREAD**.
 *
 * @see Terminal::Tabs
 * @see Config
 * @see ENDApplication::systemRequestedQuit
 * @see Terminal::Action
 */

/*
  ==============================================================================

    END - Ephemeral Nexus Display
    Main application component

    MainComponent.h - Main application content component

  ==============================================================================
*/

#pragma once
#include <JuceHeader.h>
#include "AppState.h"
#include "component/LookAndFeel.h"
#include "component/MessageOverlay.h"
#include "component/Popup.h"
#include "component/Tabs.h"
#include "config/Config.h"
#include "terminal/action/Action.h"
#include "terminal/action/ActionList.h"
#include "terminal/rendering/Fonts.h"
#include "terminal/selection/SelectionOverlay.h"

/**
 * @class MainComponent
 * @brief Root content component of the END application window.
 *
 * Placed inside `jreng::GlassWindow` by `ENDApplication::initialise()`.
 * Owns the `Terminal::Tabs` container and paints the translucent background
 * layer that shows through the native window blur effect.
 *
 * @par Layout
 * `resized()` gives the full local bounds to `Terminal::Tabs`; each terminal
 * applies its own insets and title-bar offset internally.
 *
 * @par Background painting
 * `paint()` fills with `backgroundColour.withAlpha(opacity)`.  The native blur
 * layer beneath the window provides the frosted-glass effect; this fill sets
 * the tint colour and transparency.
 *
 * @see Terminal::Tabs
 * @see Config::Key::windowColour
 * @see Config::Key::windowOpacity
 * @see Terminal::Action
 */
class MainComponent : public juce::Component
{
public:
    /** @brief Constructs the component, creates Terminal::Tabs, sets initial size. */
    MainComponent();

    /** @brief Clears the BackgroundBlur close callback before destruction. */
    ~MainComponent() override;

    /**
     * @brief Fills the full bounds to Terminal::Tabs.
     * @note MESSAGE THREAD — called by JUCE layout system on every resize.
     */
    void resized() override;

    /**
     * @brief Rebuilds actions, applies config to tabs, LookAndFeel, and orientation.
     *
     * Called by `Config::onReload` (wired in Main.cpp) after Config reloads
     * `end.lua`.  Also called once from the constructor for initial setup.
     *
     * @note MESSAGE THREAD.
     * @see Config::onReload
     */
    void applyConfig();

private:
    /**
     * @brief Registers all user-performable actions with `Terminal::Action`.
     *
     * Clears existing actions, registers all fixed actions and popup actions
     * from Config, then rebuilds the key map.
     *
     * @note MESSAGE THREAD.
     * @see Terminal::Action
     */
    void registerActions();

    /** @brief Cached context references; resolved once, used everywhere. */
    Config& config { *Config::getContext() };
    AppState& appState { *AppState::getContext() };

    /** @brief Application-wide LookAndFeel; set as default, inherited by all children. */
    Terminal::LookAndFeel terminalLookAndFeel;

    /** @brief Shared OpenGL renderer; attached to this component, renders all GL children. */
    jreng::GLRenderer glRenderer;

    /** @brief Global font context; provides font metrics and shaping for all terminals. */
    Fonts fonts { config.getString (Config::Key::fontFamily), config.getFloat (Config::Key::fontSize) };

    /** @brief Tabbed terminal container; owns all Terminal::Component instances. */
    std::unique_ptr<Terminal::Tabs> tabs;

    /** @brief Transient overlay for grid-size and status messages. */
    std::unique_ptr<MessageOverlay> messageOverlay;

    /** @brief Status bar overlay; shown during any active modal state (selection, open-file, etc.). */
    Terminal::StatusBarOverlay statusBarOverlay;

    /** @brief Modal popup dialog; shows content in a glass window. */
    Terminal::Popup popup;

    /** @brief Command palette glass window; created on demand. */
    std::unique_ptr<Terminal::ActionList> actionList;

#if JUCE_WINDOWS
    /** @brief Fires when the native scale factor changes.
     *  Updates AppState and resets zoom on all terminals. */
    juce::NativeScaleFactorNotifier scaleNotifier
    {
        this,
        [this] (float)
        {
            if (tabs != nullptr)
                tabs->resetZoom();
        }
    };
#endif

    /**
     * @brief Creates Terminal::Tabs, attaches GL renderer, wires repaint callback, restores tabs.
     * @note MESSAGE THREAD.
     * @see Terminal::Tabs
     * @see glRenderer
     * @see AppState
     */
    void initialiseTabs();

    //==============================================================================
    /**
     * @brief Exits selection mode on the active terminal if it is currently modal.
     *
     * Called before tab and pane switches so that vim-style selection mode does
     * not persist on a terminal that is no longer focused.
     *
     * @note MESSAGE THREAD.
     */
    void exitActiveTerminalSelectionMode() noexcept;

    /**
     * @brief Creates MessageOverlay, shows startup errors if any.
     * @note MESSAGE THREAD.
     * @see MessageOverlay
     * @see Config::getLoadError()
     */
    void initialiseMessageOverlay();

    /**
     * @brief Computes grid dimensions from font metrics and window bounds, displays "cols * rows" overlay on resize.
     * @note MESSAGE THREAD — called from resized().
     * @see MessageOverlay
     * @see fonts
     */
    void showMessageOverlay();

    //==============================================================================
// #if JUCE_DEBUG
//     jreng::debug::Widget debug { this, false };
// #endif
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
