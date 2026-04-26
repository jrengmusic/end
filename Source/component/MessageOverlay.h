/**
 * @file MessageOverlay.h
 * @brief Transient overlay component for status messages.
 *
 * MessageOverlay is a non-interactive, semi-transparent overlay that appears
 * briefly over the application to communicate transient information:
 *
 * - **Config reload** — "RELOADED" shown for 1 second after Cmd+R.
 * - **Config errors** — multi-line error/warning text shown for 5 seconds.
 * - **Window size** — ruler overlay shown on resize (see `showResize()`).
 *
 * ### Resize ruler
 * `showResize()` switches the overlay into ruler mode.  `paintRulers()` draws
 * two crossed ruler lines — horizontal at 2/3 from the top (1/3 from the
 * bottom) and vertical at 2/3 from the left (1/3 from the right) — placing
 * them near the bottom-right resize handle.  Each ruler is a `juce::Path`
 * stroke with perpendicular end ticks and an inline label (`--- 80 col ---`,
 * `--- 24 row ---`) with a gap cut out of the line behind the text.
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
 * @note All methods are called on the **MESSAGE THREAD**.
 *
 * @see MainComponent
 * @see lua::Engine::display.overlay.family
 * @see lua::Engine::display.overlay.size
 * @see lua::Engine::display.overlay.colour
 */

#pragma once
#include <JuceHeader.h>
#include "../lua/Engine.h"

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
     * Visibility is managed entirely by `jam::Animator::toggleFade()`.
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
     * @note Calls `toFront(false)` after fade-in to guarantee sibling z-order lift
     *       (CPU mode respects sibling order literally; GPU composites via GL).
     */
    void showMessage (const juce::String& text, int durationMs = messageDelayMs)
    {
        resizeMode = false;
        message = text;
        repaint();
        jam::Animator::toggleFade (this, true, fadeInMs);
        toFront (false);
        startTimer (durationMs);
    }

    /**
     * @brief Shows the resize ruler overlay for 1 second.
     *
     * Switches the overlay into ruler mode: `paint()` calls `paintRulers()`
     * instead of drawing centred text.  The rulers are inset by the terminal
     * grid padding so they align with the actual grid edges rather than the
     * raw window edge.
     *
     * @param cols    Current terminal column count.
     * @param rows    Current terminal row count.
     * @param padTop    Grid padding — top edge in logical pixels.
     * @param padRight  Grid padding — right edge in logical pixels.
     * @param padBottom Grid padding — bottom edge in logical pixels.
     * @param padLeft   Grid padding — left edge in logical pixels.
     * @note MESSAGE THREAD.
     * @note Calls `toFront(false)` after fade-in to guarantee sibling z-order lift
     *       (CPU mode respects sibling order literally; GPU composites via GL).
     */
    void showResize (int cols, int rows, int padTop, int padRight, int padBottom, int padLeft)
    {
        resizeMode = true;
        resizeCols = cols;
        resizeRows = rows;
        resizePadTop = padTop;
        resizePadRight = padRight;
        resizePadBottom = padBottom;
        resizePadLeft = padLeft;
        repaint();
        jam::Animator::toggleFade (this, true, fadeInMs);
        toFront (false);
        startTimer (resizeDelayMs);
    }

    /**
     * @brief Dispatches to `paintRulers()` or plain centred text depending on mode.
     *
     * @param g  JUCE graphics context for this paint pass.
     * @note MESSAGE THREAD.
     */
    void paint (juce::Graphics& g) override
    {
        const auto* cfg { lua::Engine::getContext() };
        const auto bgColour { cfg->display.window.colour };
        const auto fgColour { cfg->display.overlay.colour };
        const juce::FontOptions font { juce::FontOptions()
                                           .withName (cfg->display.overlay.family)
                                           .withPointHeight (cfg->display.overlay.size) };

        g.fillAll (bgColour.withAlpha (backgroundAlpha));
        g.setFont (font);
        g.setColour (fgColour);

        if (resizeMode)
            paintRulers (g,
                         getLocalBounds(),
                         resizeCols,
                         resizeRows,
                         resizePadTop,
                         resizePadRight,
                         resizePadBottom,
                         resizePadLeft,
                         font,
                         fgColour);
        else
            g.drawFittedText (message, getLocalBounds().reduced (textPadding), juce::Justification::centred, maxLines);
    }

private:
    /**
     * @brief Draws the crossed ruler overlay for the resize indicator.
     *
     * Renders two rulers near the bottom-right corner (where the OS resize
     * handle lives).  The rulers are inset by the terminal grid padding so
     * they span exactly the live grid area rather than the raw window edge:
     * - **Horizontal ruler** at y = 2/3 of grid height: spans full grid width,
     *   label `N col` centred inline with a gap cut from the line.
     * - **Vertical ruler** at x = 2/3 of grid width: spans full grid height,
     *   label `N row` centred horizontally on the line with a gap cut from the
     *   line.
     *
     * Each ruler consists of:
     * 1. Two `juce::Path` strokes flanking the label gap.
     * 2. A short perpendicular tick at each end of the ruler.
     * 3. The label text drawn into the gap.
     *
     * @param g          Graphics context.
     * @param bounds     Component local bounds (full overlay area).
     * @param cols       Column count to display.
     * @param rows       Row count to display.
     * @param padTop     Grid padding — top edge in logical pixels.
     * @param padRight   Grid padding — right edge in logical pixels.
     * @param padBottom  Grid padding — bottom edge in logical pixels.
     * @param padLeft    Grid padding — left edge in logical pixels.
     * @param fontOptions  Font options used for label text (same as overlay font).
     * @param colour     Stroke and text colour.
     */
    static void paintRulers (juce::Graphics& g,
                             juce::Rectangle<int> bounds,
                             int cols,
                             int rows,
                             int padTop,
                             int padRight,
                             int padBottom,
                             int padLeft,
                             const juce::FontOptions& fontOptions,
                             juce::Colour colour)
    {
        g.setColour (colour);

        // Grid area — inset by padding so rulers align with actual grid edges.
        const auto grid { bounds.reduced (0, 0)
                              .withTrimmedTop (padTop)
                              .withTrimmedRight (padRight)
                              .withTrimmedBottom (padBottom)
                              .withTrimmedLeft (padLeft) };

        const float x0 { static_cast<float> (grid.getX()) };
        const float y0 { static_cast<float> (grid.getY()) };
        const float w { static_cast<float> (grid.getWidth()) };
        const float h { static_cast<float> (grid.getHeight()) };

        // Ruler positions: 2/3 from top-left = 1/3 from bottom-right.
        const float hY { y0 + h * 2.0f / 3.0f };// horizontal ruler y
        const float vX { x0 + w * 2.0f / 3.0f };// vertical ruler x

        const float tickLen { 6.0f };// perpendicular end-tick half-length
        const float stroke { 1.0f };// line stroke width
        const float labelGap { 6.0f };// space between line end and label edge

        // ── Horizontal ruler ─────────────────────────────────────────────────
        {
            const juce::String label { juce::String (cols) + " col" };
            const juce::Font juceFont { fontOptions };
            const float labelW { juce::TextLayout::getStringWidth (juceFont, label) };
            const float totalGap { labelW + labelGap * 2.0f };
            const float midX { x0 + w / 2.0f };
            const float gapL { midX - totalGap / 2.0f };// line ends here
            const float gapR { midX + totalGap / 2.0f };// line resumes here
            const float x1 { x0 + w };// right edge of grid

            // Left segment + left tick
            juce::Path left;
            left.startNewSubPath (x0, hY);
            left.lineTo (gapL, hY);
            left.startNewSubPath (x0, hY - tickLen);
            left.lineTo (x0, hY + tickLen);
            g.strokePath (left, juce::PathStrokeType (stroke));

            // Right segment + right tick
            juce::Path right;
            right.startNewSubPath (gapR, hY);
            right.lineTo (x1, hY);
            right.startNewSubPath (x1, hY - tickLen);
            right.lineTo (x1, hY + tickLen);
            g.strokePath (right, juce::PathStrokeType (stroke));

            // Label centred in the gap
            g.drawText (label,
                        juce::Rectangle<float> (gapL, hY - juceFont.getHeight() / 2.0f, totalGap, juceFont.getHeight()),
                        juce::Justification::centred,
                        false);
        }

        // ── Vertical ruler ───────────────────────────────────────────────────
        {
            const juce::Font juceFont { fontOptions };
            const juce::String label { juce::String (rows) + " row" };
            const float labelH { juceFont.getHeight() };
            const float totalGap { labelH + labelGap * 2.0f };
            const float midY { y0 + h / 2.0f };
            const float gapT { midY - totalGap / 2.0f };// line ends here
            const float gapB { midY + totalGap / 2.0f };// line resumes here
            const float y1 { y0 + h };// bottom edge of grid

            // Top segment + top tick
            juce::Path top;
            top.startNewSubPath (vX, y0);
            top.lineTo (vX, gapT);
            top.startNewSubPath (vX - tickLen, y0);
            top.lineTo (vX + tickLen, y0);
            g.strokePath (top, juce::PathStrokeType (stroke));

            // Bottom segment + bottom tick
            juce::Path bottom;
            bottom.startNewSubPath (vX, gapB);
            bottom.lineTo (vX, y1);
            bottom.startNewSubPath (vX - tickLen, y1);
            bottom.lineTo (vX + tickLen, y1);
            g.strokePath (bottom, juce::PathStrokeType (stroke));

            // Label horizontal (no rotation), centred on vX and midY in the gap
            const float labelW { juce::TextLayout::getStringWidth (juceFont, label) };
            g.drawText (label,
                        juce::Rectangle<float> (vX - labelW / 2.0f, gapT, labelW, totalGap),
                        juce::Justification::centred,
                        false);
        }
    }

    //==============================================================================
    /** @brief Auto-hide timer callback: stops the timer and fades out. */
    void timerCallback() override
    {
        stopTimer();
        jam::Animator::toggleFade (this, false);
    }

    //==============================================================================
    /** @brief The text currently displayed in plain message mode. */
    juce::String message;

    /** @brief True while the overlay is in resize-ruler mode. */
    bool resizeMode { false };

    /** @brief Column count shown by the resize ruler. */
    int resizeCols { 0 };

    /** @brief Row count shown by the resize ruler. */
    int resizeRows { 0 };

    /** @brief Grid padding top — passed to paintRulers() to inset the ruler bounds. */
    int resizePadTop { 0 };

    /** @brief Grid padding right — passed to paintRulers() to inset the ruler bounds. */
    int resizePadRight { 0 };

    /** @brief Grid padding bottom — passed to paintRulers() to inset the ruler bounds. */
    int resizePadBottom { 0 };

    /** @brief Grid padding left — passed to paintRulers() to inset the ruler bounds. */
    int resizePadLeft { 0 };

    //==============================================================================
    /** @brief Background fill alpha [0, 1]; applied on top of the window blur. */
    static constexpr float backgroundAlpha { 0.8f };

    /** @brief Padding in pixels applied to the text bounds in plain message mode. */
    static constexpr int textPadding { 20 };

    /** @brief Maximum number of text lines rendered by drawFittedText(). */
    static constexpr int maxLines { 20 };

    /** @brief Fade-in duration in milliseconds. */
    static constexpr int fadeInMs { 60 };

    /** @brief Display duration for plain messages in milliseconds. */
    static constexpr int messageDelayMs { 5000 };

    /** @brief Display duration for the resize ruler in milliseconds. */
    static constexpr int resizeDelayMs { 1000 };

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MessageOverlay)
};
