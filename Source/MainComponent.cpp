/**
 * @file MainComponent.cpp
 * @brief Implementation of the root application content component.
 *
 * Constructs the `TerminalComponent`, sets the initial window size from
 * persisted state, and registers a close callback so that window dimensions
 * are saved when the native close button is pressed.
 *
 * @see MainComponent
 * @see TerminalComponent
 * @see Config
 */

/*
  ==============================================================================

    END - Ephemeral Nexus Display
    Main application component

    MainComponent.cpp - Main application content component

  ==============================================================================
*/

#include "MainComponent.h"

/**
 * @brief Constructs MainComponent.
 *
 * Construction order:
 * 1. `setOpaque(true)` — tells JUCE the component paints its full bounds,
 *    avoiding unnecessary parent repaints.
 * 2. Creates `TerminalComponent` and makes it visible.
 * 3. Reads `windowWidth` / `windowHeight` from Config (which already applied
 *    `state.lua` overrides) and calls `setSize()`.
 * 4. Registers a `BackgroundBlur` close callback that persists the current
 *    window size to `state.lua` when the native close button is clicked.
 *
 * @note MESSAGE THREAD — called from ENDApplication::initialise().
 */
MainComponent::MainComponent()
{
    setOpaque (true);

    terminal = std::make_unique<TerminalComponent>();
    addAndMakeVisible (terminal.get());

    const auto* cfg { Config::getContext() };
    setSize (cfg->getInt (Config::Key::windowWidth),
             cfg->getInt (Config::Key::windowHeight));

    jreng::BackgroundBlur::setCloseCallback (
        [this]()
        {
            Config::getContext()->saveWindowSize (getWidth(), getHeight());
        });
}

/**
 * @brief Fills the full bounds to TerminalComponent.
 *
 * The terminal applies its own internal insets (horizontal/vertical padding
 * and optional title-bar offset), so we simply hand it the entire local area.
 *
 * @note MESSAGE THREAD — called by JUCE on every resize event.
 */
void MainComponent::resized()
{
    terminal->setBounds (getLocalBounds());
}

/**
 * @brief Paints the translucent background tint.
 *
 * Fills the component with `backgroundColour` at `opacity` alpha.  The native
 * blur layer beneath the window provides the frosted-glass effect; this fill
 * sets the tint colour and transparency level.
 *
 * @param g  JUCE graphics context for this paint pass.
 * @note MESSAGE THREAD — called by JUCE on every repaint.
 */
void MainComponent::paint (juce::Graphics& g) { g.fillAll (backgroundColour.withAlpha (opacity)); }

/**
 * @brief Clears the BackgroundBlur close callback.
 *
 * Passing `nullptr` prevents the lambda (which captures `this`) from being
 * invoked after this component has been destroyed.
 *
 * @note MESSAGE THREAD — called during window teardown.
 */
MainComponent::~MainComponent()
{
    jreng::BackgroundBlur::setCloseCallback (nullptr);
}
