/**
 * @file MessageOverlay.h
 * @brief Transient overlay component for grid-size and status messages.
 *
 * MessageOverlay is a non-interactive, semi-transparent overlay that appears
 * briefly over the terminal grid to communicate transient information:
 *
 * - **Grid size** — shown on every resize (e.g. "80 col * 24 row").
 * - **Config reload** — "RELOADED" shown for 1 second after Cmd+R.
 * - **Config errors** — multi-line error/warning text shown for 5 seconds.
 *
 * ### Fade animation
 * Visibility transitions use `jreng::Animator::toggleFade()` for smooth
 * fade-in / fade-out.  A `juce::Timer` triggers the fade-out after the
 * configured display duration.
 *
 * ### Zoom suppression
 * `TerminalComponent` sets `zoomInProgress = true` before calling `resized()`
 * during zoom operations, which prevents `resized()` from calling `show()`.
 * This avoids a distracting grid-size flash during zoom.
 *
 * ### Mouse passthrough
 * `setInterceptsMouseClicks(false, false)` ensures the overlay never captures
 * mouse events, so selection and scrolling work normally while it is visible.
 *
 * @note All methods are called on the **MESSAGE THREAD**.
 *
 * @see TerminalComponent::resized
 * @see TerminalComponent::keyPressed
 * @see Config::Key::overlayFamily
 * @see Config::Key::overlaySize
 * @see Config::Key::overlayColour
 */

#pragma once
#include <JuceHeader.h>
#include "../config/Config.h"

/**
 * @class MessageOverlay
 * @brief Semi-transparent overlay for transient terminal status messages.
 *
 * Inherits `juce::Component` for rendering and `juce::Timer` (private) for
 * the auto-hide delay.  All display logic is inline; there is no separate .cpp.
 *
 * @par Display modes
 * | Method          | Content                        | Default duration |
 * |-----------------|--------------------------------|------------------|
 * | `show()`        | Grid size ("80 col * 24 row")  | 1000 ms          |
 * | `showMessage()` | Arbitrary text (errors, etc.)  | 5000 ms          |
 *
 * @par Thread context
 * **MESSAGE THREAD** — all public methods.
 *
 * @see TerminalComponent
 */
class MessageOverlay
    : public juce::Component
    , private juce::Timer
{
public:
    /**
     * @brief Constructs MessageOverlay: sets non-opaque, disables mouse interception.
     *
     * The component starts hidden (`addChildComponent` in TerminalComponent).
     * Visibility is managed entirely by `jreng::Animator::toggleFade()`.
     *
     * @note MESSAGE THREAD.
     */
    MessageOverlay()
    {
        setOpaque (false);
        setInterceptsMouseClicks (false, false);
    }

    /**
     * @brief Updates the stored grid dimensions and triggers a repaint.
     *
     * Called by `TerminalComponent::resized()` after every layout pass so that
     * the next `show()` call displays the current grid size.
     *
     * @param rows  Current number of visible grid rows.
     * @param cols  Current number of visible grid columns.
     * @note MESSAGE THREAD.
     */
    void setGridSize (int rows, int cols)
    {
        numRows = rows;
        numCols = cols;
        repaint();
    }

    /**
     * @brief Shows the grid-size message for the default display duration.
     *
     * Formats the message as "N col * M row", fades in, then starts the
     * auto-hide timer for `delayMs` milliseconds.
     *
     * @note MESSAGE THREAD — called from TerminalComponent::resized().
     * @see setGridSize
     */
    void show()
    {
        message = juce::String (numCols) + " col * " + juce::String (numRows) + " row";
        jreng::Animator::toggleFade (this, true, fadeInMs);
        startTimer (delayMs);
    }

    /**
     * @brief Shows an arbitrary message for a configurable duration.
     *
     * Used for config reload confirmation ("RELOADED") and multi-line error
     * messages from the Lua config loader.  Fades in immediately, then starts
     * the auto-hide timer for @p durationMs milliseconds.
     *
     * @param text        The message to display (may be multi-line).
     * @param durationMs  How long to show the message before fading out.
     *                    Defaults to `messageDelayMs` (5000 ms).
     * @note MESSAGE THREAD — called from TerminalComponent::keyPressed() on Cmd+R,
     *       and asynchronously at startup if the config has errors.
     */
    void showMessage (const juce::String& text, int durationMs = messageDelayMs)
    {
        message = text;
        repaint();
        jreng::Animator::toggleFade (this, true, fadeInMs);
        startTimer (durationMs);
    }

    /**
     * @brief Paints the semi-transparent background and centred message text.
     *
     * Fills the component with the window background colour at `backgroundAlpha`
     * opacity, then draws `message` centred in the reduced bounds using the
     * overlay font family, size, and colour from Config.
     *
     * @param g  JUCE graphics context for this paint pass.
     * @note MESSAGE THREAD.
     *
     * @see Config::Key::overlayFamily
     * @see Config::Key::overlaySize
     * @see Config::Key::overlayColour
     * @see Config::Key::windowColour
     */
    void paint (juce::Graphics& g) override
    {
        const auto* cfg { Config::getContext() };
        const auto bgColour { cfg->getColour (Config::Key::windowColour) };
        g.fillAll (bgColour.withAlpha (backgroundAlpha));
        g.setFont (juce::FontOptions (cfg->getString (Config::Key::overlayFamily),
                                      cfg->getFloat (Config::Key::overlaySize),
                                      juce::Font::plain));
        g.setColour (cfg->getColour (Config::Key::overlayColour));
        g.drawFittedText (message, getLocalBounds().reduced (textPadding), juce::Justification::centred, maxLines);
    }

private:
    /** @brief Current number of visible grid rows; updated by setGridSize(). */
    int numRows { 0 };

    /** @brief Current number of visible grid columns; updated by setGridSize(). */
    int numCols { 0 };

    /** @brief The text currently displayed by the overlay. */
    juce::String message;

    /**
     * @brief Auto-hide timer callback: stops the timer and fades out.
     *
     * Called by JUCE after the display duration elapses.  Triggers a fade-out
     * animation via `jreng::Animator::toggleFade()`.
     *
     * @note MESSAGE THREAD.
     */
    void timerCallback() override
    {
        stopTimer();
        jreng::Animator::toggleFade (this, false);
    }

    //==============================================================================
    /** @brief Background fill alpha [0, 1]; applied on top of the window blur. */
    static constexpr float backgroundAlpha { 0.8f };

    /** @brief Padding in pixels applied to the text bounds on all sides. */
    static constexpr int textPadding { 20 };

    /** @brief Maximum number of text lines rendered by drawFittedText(). */
    static constexpr int maxLines { 20 };

    /** @brief Fade-in duration in milliseconds. */
    static constexpr int fadeInMs { 60 };

    /** @brief Display duration for grid-size messages in milliseconds. */
    static constexpr int delayMs { 1000 };

    /** @brief Default display duration for showMessage() in milliseconds. */
    static constexpr int messageDelayMs { 5000 };

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MessageOverlay)
};
