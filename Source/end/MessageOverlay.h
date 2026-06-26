/**
 * @file MessageOverlay.h
 * @brief Transient overlay component for status messages.
 *
 * MessageOverlay is a non-interactive, semi-transparent overlay that appears
 * briefly over the application to communicate transient text status:
 *
 * - **Config reload** — success or error text written to the overlay's own
 *   @c ID::message property (owned as a @c jam::Model::Component), shown for
 *   the default duration (5000 ms) via View::valueTreePropertyChanged.
 * - **Arbitrary messages** — multi-line text shown via showMessage().
 *
 * ### Fade animation
 * Visibility transitions use `jam::Animator::toggleFade()` for smooth
 * fade-in / fade-out.  A `juce::Timer` triggers the fade-out after the
 * configured display duration.
 *
 * ### Mouse passthrough
 * `setInterceptsMouseClicks(false, false)` ensures the overlay never captures
 * mouse events, so selection and scrolling work normally while it is visible.
 *
 * ### Font and colour
 * Font is read from the config tree (theme > overlay: family + size) on every
 * paint pass — hot-reload is free.  Background uses
 * `juce::ResizableWindow::backgroundColourId`; text uses
 * `juce::Label::textColourId`.
 *
 * @note All methods are called on the **MESSAGE THREAD**.
 *
 * @see end::View
 * @see config::Model
 * @see end::LookAndFeel
 */

#pragma once
#include <JuceHeader.h>
#include "Identifier.h"
#include "../config/Config.h"
#include "../lookAndFeel/LookAndFeel.h"

namespace end
{
/*____________________________________________________________________________*/

/**
 * @class MessageOverlay
 * @brief Semi-transparent overlay for transient status messages (text-only mode).
 *
 * Inherits `juce::Component` for rendering, `jam::Model::Component` for owned
 * ValueTree state, and `juce::Timer` (private) for the auto-hide delay.
 * All display logic is inline; there is no separate .cpp.
 *
 * @par Thread context
 * **MESSAGE THREAD** — all public methods.
 *
 * @see end::View
 */
class MessageOverlay
    : public juce::Component
    , public jam::Model::Component
    , private juce::Timer
{
public:
    /**
     * @brief Constructs MessageOverlay: binds model state, sets non-opaque, disables mouse interception.
     *
     * The component starts hidden (`addChildComponent` in the parent).
     * Visibility is managed entirely by `jam::Animator::toggleFade()`.
     * Call registerParameters() after Attachment is constructed in View.
     *
     * @param m  Shared jam::Model that owns the application state tree.
     * @note MESSAGE THREAD.
     */
    explicit MessageOverlay (jam::Model& m)
        : jam::Model::Component (m, IDtype::overlay)
    {
        setOpaque (false);
        setInterceptsMouseClicks (false, false);
    }

    /** @brief Default destructor. */
    ~MessageOverlay() override = default;

    /**
     * @brief Registers the ID::message ParameterText on the overlay's state.
     *
     * Must be called after the Attachment for MessageOverlay is constructed in
     * View, so the state is already parented in the model tree before the
     * adapter is registered.
     *
     * @note MESSAGE THREAD — called once during View construction.
     */
    void registerParameters()
    {
        auto& messageParam { model.createAndAddParameter<jam::ParameterText> (
            state, ID::message, juce::String {}, 4096) };

        parameterAttachments.add (
            std::make_unique<jam::Model::ParameterAttachment> (messageParam,
                                                               [this] (const juce::var& v)
                                                               {
                                                                   showMessage (v.toString());
                                                               }));
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
     * @note Calls `toFront(false)` after fade-in to guarantee sibling z-order lift.
     */
    void showMessage (const juce::String& text, int durationMs = messageDelayMs)
    {
        message = text;
        repaint();
        jam::Animator::toggleFade (this, true, fadeInMs);
        toFront (false);
        startTimer (durationMs);
    }

    /**
     * @brief Paints the semi-transparent background and centred message text.
     *
     * Font is read from the config tree on every paint pass (hot-reload safe).
     * Background colour from `juce::ResizableWindow::backgroundColourId`,
     * text colour from `juce::Label::textColourId`.
     *
     * @param g  JUCE graphics context for this paint pass.
     * @note MESSAGE THREAD.
     */
    void paint (juce::Graphics& g) override
    {
        auto& cfg { *config::Model::getInstance() };
        auto family { cfg.getValue (jam::IDtype::overlay, ID::fontFamily).toString() };
        auto size { static_cast<float> (cfg.getValue (jam::IDtype::overlay, ID::fontSize)) };
        juce::Font font { juce::FontOptions (family, size, juce::Font::plain) };

        auto bgColour { findColour (juce::Label::backgroundColourId) };
        auto fgColour { findColour (juce::Label::textColourId) };

        g.fillAll (bgColour.withAlpha (backgroundAlpha));
        g.setFont (font);
        g.setColour (fgColour);
        g.drawFittedText (
            message, getLocalBounds().reduced (textPadding), juce::Justification::centred, maxLines);
    }

private:
    /** @brief Auto-hide timer callback: stops the timer and fades out. */
    void timerCallback() override
    {
        stopTimer();
        jam::Animator::toggleFade (this, false);
    }

    //==============================================================================
    /** @brief The text currently displayed. */
    juce::String message;

    /** @brief RAII parameter attachment for ID::message — delivers changes to showMessage. */
    jam::Owner<jam::Model::ParameterAttachment> parameterAttachments;

    //==============================================================================
    /** @brief Background fill alpha [0, 1]; applied on top of the window content. */
    static constexpr float backgroundAlpha { 0.8f };

    /** @brief Padding in pixels applied to the text bounds. */
    static constexpr int textPadding { 20 };

    /** @brief Maximum number of text lines rendered by drawFittedText(). */
    static constexpr int maxLines { 20 };

    /** @brief Fade-in duration in milliseconds. */
    static constexpr int fadeInMs { 60 };

    /** @brief Display duration for plain messages in milliseconds. */
    static constexpr int messageDelayMs { 0 };

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MessageOverlay)
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace end
