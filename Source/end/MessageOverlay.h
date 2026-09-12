/**
 * @file MessageOverlay.h
 * @brief Transient overlay component for status messages.
 *
 * MessageOverlay is a non-interactive, semi-transparent overlay that appears
 * briefly over the application to communicate transient text status:
 *
 * - **Config reload** — success or error text written to the overlay's own
 *   @c Id::message property (owned via @c jam::Model::Component),
 *   shown through the fade cycle via View::valueTreePropertyChanged.
 * - **Arbitrary messages** — multi-line text shown via showMessage().
 *
 * ### Fade animation
 * Visibility transitions use `jam::Animator::toggleFade()` for smooth
 * fade-in / fade-out. The animator's own fade durations define the visible
 * window — no timer, no hold state.
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
#include "generated/Generated.h"
#include "config/ConfigModel.h"
#include "lookAndFeel/ENDLookAndFeel.h"

/** @brief Sentinel splitLine value for drawMessageOverlay() — draws no axis line. */
static constexpr int noSplitLine { -1 };

/**
 * @brief Paints a semi-transparent overlay background and centred message
 *        text, with an optional split axis line between two message regions.
 *
 * When splitLine is noSplitLine, message is drawn as a single centred block.
 * Otherwise message is split on " | " into two halves, each drawn in its own
 * region on either side of the axis line at splitLine.
 *
 * @param g             Graphics context for this paint pass.
 * @param overlay       Component whose colours (background/text) are used.
 * @param bounds        Area to fill and paint text within.
 * @param message       Text to display, optionally two halves joined by " | ".
 * @param splitLine     Axis line position in pixels, or noSplitLine for none.
 * @param splitVertical True for a vertical axis line, false for horizontal.
 */
void drawMessageOverlay (juce::Graphics& g,
                         juce::Component& overlay,
                         juce::Rectangle<int> bounds,
                         const juce::String& message,
                         int splitLine = noSplitLine,
                         bool splitVertical = false);

/**
 * @class MessageOverlay
 * @brief Semi-transparent overlay for transient status messages (text-only mode).
 *
 * Inherits `juce::Component` (rendering) and `jam::Model::Component`
 * (owned ValueTree state, adopting the Model's own OVERLAY row). Its paint()
 * delegates to the free function drawMessageOverlay(), defined in
 * MessageOverlay.cpp.
 *
 * @par Thread context
 * **MESSAGE THREAD** — all public methods.
 *
 * @see ENDView
 */
class MessageOverlay
    : public juce::Component
    , public jam::Model::Component<MessageOverlay>
{
public:
    /**
     * @brief Constructs MessageOverlay: adopts Nexus's own OVERLAY row, sets
     * non-opaque, disables mouse interception.
     *
     * The component starts hidden (`addChildComponent` in the parent).
     * Visibility is managed entirely by `jam::Animator::toggleFade()`.
     * Call registerParameters() once this component is parented.
     *
     * @param m            Shared jam::Model that owns the application state tree.
     * @param overlayState Nexus's own OVERLAY row to adopt as @c state.
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
     * @brief Attaches to the Id::message ParameterText on the overlay's state.
     *
     * ENDModel registers Id::message at construction — the owner-established
     * invariant this class trusts. @c state is already parented in the model
     * tree at construction time — this class adopts Nexus's own
     * pre-bootstrapped OVERLAY row — so this may be called any time after
     * construction.
     *
     * @note MESSAGE THREAD — called once during View construction.
     */
    void registerParameters()
    {
        auto* messageParam { model.getParameter<jam::ParameterText> (
            Id::toType (Id::overlay), Id::message) };
        jassert (messageParam != nullptr);

        if (messageParam != nullptr)
        {
            parameterAttachments.add (
                std::make_unique<jam::Model::ParameterAttachment> (*messageParam,
                                                                   [this] (const juce::var& v)
                                                                   {
                                                                       showMessage (v.toString());
                                                                   }));
        }
    }

    /**
     * @brief Shows a message, then fades it back out.
     *
     * Fades in, lifts to the sibling z-order front, and requests the fade-out —
     * the animator's own fade durations define the visible window.
     *
     * @param text  The message to display (may be multi-line).
     * @note MESSAGE THREAD.
     */
    void showMessage (const juce::String& text)
    {
        message = text;
        repaint();
        jam::Animator::toggleFade (this, true, fadeInMs);
        toFront (false);
        jam::Animator::toggleFade (this, false);
    }

    /**
     * @brief Paints the semi-transparent background and centred message text.
     *
     * Font is read from the config tree on every paint pass (hot-reload safe).
     * Background colour from `juce::Label::backgroundColourId`,
     * text colour from `juce::Label::textColourId`. The message text itself is
     * the display copy showMessage() wrote from the parameter attachment's
     * delivered value — no re-read of state on every paint.
     *
     * @param g  JUCE graphics context for this paint pass.
     * @note MESSAGE THREAD.
     */
    void paint (juce::Graphics& g) override
    {
        drawMessageOverlay (g, *this, getLocalBounds(), message);
    }

private:
    //==============================================================================
    /** @brief RAII parameter attachment for Id::message — delivers changes to showMessage. */
    jam::Owner<jam::Model::ParameterAttachment> parameterAttachments;

    /** @brief Display copy of the current message, written once by showMessage()
     *  from the parameter attachment's delivered value; read by paint(). */
    juce::String message;

    //==============================================================================
    /** @brief Fade-in duration in milliseconds. */
    static constexpr int fadeInMs { 60 };

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MessageOverlay)
};
