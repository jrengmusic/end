/**
 * @file MainComponent.h
 * @brief Root JUCE component that hosts the terminal UI.
 *
 * MainComponent is the content component of the application's `GlassWindow`.
 * It owns a `Terminal::Tabs` child that manages multiple terminal sessions,
 * and paints the window background with the configured colour and opacity.
 *
 * ### Responsibilities
 * - Sets the initial window size from `Config::Key::windowWidth/Height`.
 * - Registers a close callback with `jreng::BackgroundBlur` so that window
 *   dimensions are persisted to `state.lua` when the native close button is
 *   pressed (in addition to the Cmd+Q path handled by ENDApplication).
 * - Delegates all keyboard, mouse, and terminal I/O to `Terminal::Tabs`.
 * - Serves as an ApplicationCommandTarget, owning the ApplicationCommandManager
 *   and KeyBinding for command dispatch.
 *
 * @par Thread context
 * All methods are called on the **MESSAGE THREAD**.
 *
 * @see Terminal::Tabs
 * @see Config
 * @see ENDApplication::systemRequestedQuit
 * @see KeyBinding
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
#include "component/Tabs.h"
#include "config/Config.h"
#include "config/KeyBinding.h"

/**
 * @class MainComponent
 * @brief Root content component of the END application window.
 *
 * Placed inside `jreng::GlassWindow` by `ENDApplication::initialise()`.
 * Owns the `Terminal::Tabs` container and paints the translucent background
 * layer that shows through the native window blur effect.
 *
 * Inherits `juce::ApplicationCommandTarget` to handle application-wide commands
 * (copy, paste, quit, close/new tab, navigate tabs, reload, zoom) via the
 * JUCE command manager.
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
 * @see KeyBinding
 */
class MainComponent
    : public juce::Component
    , public juce::ApplicationCommandTarget
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

    //==============================================================================
    // juce::ApplicationCommandTarget overrides

    juce::ApplicationCommandTarget* getNextCommandTarget() override;

    void getAllCommands (juce::Array<juce::CommandID>& commands) override;

    void getCommandInfo (juce::CommandID commandID, juce::ApplicationCommandInfo& result) override;

    bool perform (const juce::ApplicationCommandTarget::InvocationInfo& info) override;

private:
    juce::ApplicationCommandManager commandManager;
    KeyBinding keyBinding { commandManager };
    jreng::GLRenderer glRenderer;
    std::unique_ptr<Terminal::Tabs> tabs;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
