/**
 * @file MessageOverlay.h
 * @brief Transient overlay component for status messages.
 *
 * MessageOverlay is a non-interactive, semi-transparent overlay that appears
 * briefly over the application to communicate transient information:
 *
 * - **Config reload** — "RELOADED" shown for 1 second after Cmd+R.
 * - **Config errors** — multi-line error/warning text shown for 5 seconds.
 * - **Window size** — shown on resize.
 *
 * ### Fade animation
 * Visibility transitions use `jreng::Animator::toggleFade()` for smooth
 * fade-in / fade-out.  A `juce::Timer` triggers the fade-out after the
 * configured display duration.
 *
 * ### Mouse passthrough
 * `setInterceptsMouseClicks(false, false)` ensures the overlay never captures
 * mouse events, so selection and scrolling work normally while it is visible.
 *
 * @note All methods are called on the **MESSAGE THREAD**.
 *
 * @see MainComponent
 * @see Config::Key::overlayFamily
 * @see Config::Key::overlaySize
 * @see Config::Key::overlayColour
 */

#pragma once
#include <JuceHeader.h>
#include "../config/Config.h"

/**
 * @class MessageOverlay
 * @brief Semi-transparent overlay for transient status messages.
 *
 * Inherits `juce::Component` for rendering and `juce::Timer` (private) for
 * the auto-hide delay.  All display logic is inline; there is no separate .cpp.
 *
 * @par Thread context
 * **MESSAGE THREAD** — all public methods.
 *
 * @see MainComponent
 */
class MessageOverlay
    : public juce::Component
    , private juce::Timer
{
public:
    /**
     * @brief Constructs MessageOverlay: sets non-opaque, disables mouse interception.
     *
     * The component starts hidden (`addChildComponent` in MainComponent).
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
     * @brief Shows an arbitrary message for a configurable duration.
     *
     * Fades in immediately, then starts the auto-hide timer for
     * @p durationMs milliseconds.
     *
     * @param text        The message to display (may be multi-line).
     * @param durationMs  How long to show the message before fading out.
     *                    Defaults to `messageDelayMs` (5000 ms).
     * @note MESSAGE THREAD.
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
     * @param g  JUCE graphics context for this paint pass.
     * @note MESSAGE THREAD.
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
    /** @brief The text currently displayed by the overlay. */
    juce::String message;

    /**
     * @brief Auto-hide timer callback: stops the timer and fades out.
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

    /** @brief Default display duration for showMessage() in milliseconds. */
    static constexpr int messageDelayMs { 5000 };

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MessageOverlay)
};
