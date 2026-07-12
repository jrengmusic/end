/**
 * @file MessageOverlay.h
 * @brief Transient overlay component for status messages.
 *
 * MessageOverlay is a non-interactive, semi-transparent overlay that appears
 * briefly over the application to communicate transient text status:
 *
 * - **Config reload** — success or error text written to the overlay's own
 *   @c ID::message property (owned via @c jam::Model::Component),
 *   shown for the default duration (5000 ms) via View::valueTreePropertyChanged.
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
 * `juce::Label::backgroundColourId`; text uses
 * `juce::Label::textColourId`.
 *
 * @note All methods are called on the **MESSAGE THREAD**.
 *
 * @see ENDView
 * @see ConfigModel
 * @see ENDLookAndFeel
 */

#pragma once
#include <JuceHeader.h>
#include "Identifier.h"
#include "../Bimap.h"
#include "../config/ConfigModel.h"
#include "../lookAndFeel/ENDLookAndFeel.h"

/** @brief Background fill alpha [0, 1]; applied on top of the window content. */
static constexpr float backgroundAlpha { 0.8f };

/** @brief Padding in pixels applied to the text bounds. */
static constexpr int textPadding { 20 };

/** @brief Maximum number of text lines rendered by drawFittedText(). */
static constexpr int maxLines { 20 };

/** @brief Length in pixels of a bracket-style endcap, centred on the axis line. */
static constexpr float bracketEndcapLength { 8.0f };

static void drawMessageOverlay (juce::Graphics& g,
                                juce::Component& overlay,
                                juce::Rectangle<int> bounds,
                                const juce::String& message,
                                int splitLine = -1,
                                bool splitVertical = false)
{
    const auto family { ConfigModel::getInstance()->getValue (jam::IDtype::overlay, ID::fontFamily).toString() };
    const auto size { static_cast<float> (ConfigModel::getInstance()->getValue (jam::IDtype::overlay, ID::fontSize)) };
    const juce::Font font { juce::FontOptions (family, size, juce::Font::plain) };

    const auto background { overlay.findColour (juce::Label::backgroundColourId).withAlpha (backgroundAlpha) };
    const auto foreground { overlay.findColour (juce::Label::textColourId) };

    const auto lineStyle { OverlayAxisLine::get (
        ConfigModel::getInstance()->getValue (IDtype::pane, ID::splitLine).toString()) };

    g.setColour (background);
    g.fillRect (bounds);
    g.setFont (font);
    g.setColour (foreground);

    if (splitLine >= 0)
    {
        float dashLengths[] { 6.0f, 4.0f };
        const auto numDashes { static_cast<int> (std::size (dashLengths)) };

        if (splitVertical)
        {
            const auto x { static_cast<float> (splitLine) };
            const auto top { static_cast<float> (bounds.getY()) };
            const auto bottom { static_cast<float> (bounds.getBottom()) };

            if (lineStyle == OverlayAxisLine::dash)
            {
                g.drawDashedLine ({ x, top, x, bottom }, dashLengths, numDashes);
            }
            else if (lineStyle == OverlayAxisLine::bracket)
            {
                const auto capTop { top + static_cast<float> (textPadding) };
                const auto capBottom { bottom - static_cast<float> (textPadding) };

                g.drawLine ({ x, capTop, x, capBottom });
                g.drawLine ({ x - bracketEndcapLength * 0.5f, capTop, x + bracketEndcapLength * 0.5f, capTop });
                g.drawLine ({ x - bracketEndcapLength * 0.5f, capBottom, x + bracketEndcapLength * 0.5f, capBottom });
            }
            else
            {
                g.drawLine ({ x, top, x, bottom });
            }
        }
        else
        {
            const auto y { static_cast<float> (splitLine) };
            const auto left { static_cast<float> (bounds.getX()) };
            const auto right { static_cast<float> (bounds.getRight()) };

            if (lineStyle == OverlayAxisLine::dash)
            {
                g.drawDashedLine ({ left, y, right, y }, dashLengths, numDashes);
            }
            else if (lineStyle == OverlayAxisLine::bracket)
            {
                const auto capLeft { left + static_cast<float> (textPadding) };
                const auto capRight { right - static_cast<float> (textPadding) };

                g.drawLine ({ capLeft, y, capRight, y });
                g.drawLine ({ capLeft, y - bracketEndcapLength * 0.5f, capLeft, y + bracketEndcapLength * 0.5f });
                g.drawLine ({ capRight, y - bracketEndcapLength * 0.5f, capRight, y + bracketEndcapLength * 0.5f });
            }
            else
            {
                g.drawLine ({ left, y, right, y });
            }
        }
    }

    if (splitLine >= 0 and message.contains (" | "))
    {
        const auto first { message.upToFirstOccurrenceOf (" | ", false, false) };
        const auto second { message.fromFirstOccurrenceOf (" | ", false, false) };

        const auto region1 { splitVertical ? bounds.withRight (splitLine) : bounds.withBottom (splitLine) };
        const auto region2 { splitVertical ? bounds.withLeft (splitLine) : bounds.withTop (splitLine) };

        g.drawFittedText (first, region1.reduced (textPadding), juce::Justification::centred, maxLines);
        g.drawFittedText (second, region2.reduced (textPadding), juce::Justification::centred, maxLines);
    }
    else
    {
        g.drawFittedText (message, bounds.reduced (textPadding), juce::Justification::centred, maxLines);
    }
}

/**
 * @class MessageOverlay
 * @brief Semi-transparent overlay for transient status messages (text-only mode).
 *
 * Inherits `juce::Component` (rendering) and `jam::Model::Component`
 * (owned ValueTree state, adopting Nexus's own OVERLAY node) — and
 * `juce::Timer` (private) for the auto-hide delay. All display logic is
 * inline; there is no separate .cpp.
 *
 * @par Thread context
 * **MESSAGE THREAD** — all public methods.
 *
 * @see ENDView
 */
class MessageOverlay
    : public juce::Component
    , public jam::Model::Component<MessageOverlay>
    , private juce::Timer
{
public:
    /**
     * @brief Constructs MessageOverlay: adopts Nexus's own OVERLAY node, sets
     * non-opaque, disables mouse interception.
     *
     * The component starts hidden (`addChildComponent` in the parent).
     * Visibility is managed entirely by `jam::Animator::toggleFade()`.
     * Call registerParameters() once this component is parented.
     *
     * @param m            Shared jam::Model that owns the application state tree.
     * @param overlayState Nexus's own OVERLAY node to adopt as @c state.
     * @note MESSAGE THREAD.
     */
    MessageOverlay (jam::Model& m, juce::ValueTree overlayState)
        : jam::Model::Component<MessageOverlay> (m, overlayState)
    {
        setOpaque (false);
        setInterceptsMouseClicks (false, false);
    }

    /** @brief Default destructor. */
    ~MessageOverlay() override = default;

    /**
     * @brief Registers the ID::message ParameterText on the overlay's state.
     *
     * @c state is already parented in the model tree at construction time —
     * this class adopts Nexus's own pre-bootstrapped OVERLAY node — so this
     * may be called any time after construction.
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
     * Background colour from `juce::Label::backgroundColourId`,
     * text colour from `juce::Label::textColourId`.
     *
     * @param g  JUCE graphics context for this paint pass.
     * @note MESSAGE THREAD.
     */
    void paint (juce::Graphics& g) override
    {
        drawMessageOverlay (g, *this, getLocalBounds(), message);
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
    /** @brief Fade-in duration in milliseconds. */
    static constexpr int fadeInMs { 60 };

    /** @brief Display duration for plain messages in milliseconds. */
    static constexpr int messageDelayMs { 0 };

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MessageOverlay)
};
