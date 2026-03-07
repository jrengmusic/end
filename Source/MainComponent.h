/**
 * @file MainComponent.h
 * @brief Root JUCE component that hosts the terminal UI.
 *
 * MainComponent is the content component of the application's `GlassWindow`.
 * It owns a single `TerminalComponent` child that fills the entire client area,
 * and paints the window background with the configured colour and opacity.
 *
 * ### Responsibilities
 * - Sets the initial window size from `Config::Key::windowWidth/Height`.
 * - Registers a close callback with `jreng::BackgroundBlur` so that window
 *   dimensions are persisted to `state.lua` when the native close button is
 *   pressed (in addition to the Cmd+Q path handled by ENDApplication).
 * - Delegates all keyboard, mouse, and terminal I/O to `TerminalComponent`.
 *
 * @par Thread context
 * All methods are called on the **MESSAGE THREAD**.
 *
 * @see TerminalComponent
 * @see Config
 * @see ENDApplication::systemRequestedQuit
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
#include "component/TerminalComponent.h"
#include "config/Config.h"

/**
 * @class MainComponent
 * @brief Root content component of the END application window.
 *
 * Placed inside `jreng::GlassWindow` by `ENDApplication::initialise()`.
 * Owns the `TerminalComponent` and paints the translucent background layer
 * that shows through the native window blur effect.
 *
 * @par Layout
 * `resized()` gives the full local bounds to `TerminalComponent`; the terminal
 * itself applies its own insets and title-bar offset internally.
 *
 * @par Background painting
 * `paint()` fills with `backgroundColour.withAlpha(opacity)`.  The native blur
 * layer beneath the window provides the frosted-glass effect; this fill sets
 * the tint colour and transparency.
 *
 * @see TerminalComponent
 * @see Config::Key::windowColour
 * @see Config::Key::windowOpacity
 */
class MainComponent : public juce::Component
{
public:
    /** @brief Constructs the component, creates TerminalComponent, sets initial size. */
    MainComponent();

    /** @brief Clears the BackgroundBlur close callback before destruction. */
    ~MainComponent() override;

    /**
     * @brief Fills the full bounds to TerminalComponent.
     * @note MESSAGE THREAD — called by JUCE layout system on every resize.
     */
    void resized() override;

private:
    /** @brief The terminal UI; fills the entire client area. */
    std::unique_ptr<TerminalComponent> terminal;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
