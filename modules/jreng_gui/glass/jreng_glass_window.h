/**
 * @file jreng_glass_window.h
 * @brief JUCE DocumentWindow with glassmorphism (frosted-glass) visual effect.
 *
 * @par Overview
 * GlassWindow wraps juce::DocumentWindow and applies a native macOS background
 * blur via BackgroundBlur after the window becomes visible.  Blur application
 * is deferred through juce::AsyncUpdater so that the native peer is guaranteed
 * to exist before any Objective-C / CoreGraphics calls are made.
 *
 * @par Usage
 * @code
 * auto* win = new jreng::GlassWindow (
 *     std::make_unique<MainComponent>().release(),
 *     "My App",
 *     juce::Colours::black,
 *     0.6f,   // opacity
 *     20.0f,  // blur radius
 *     false,  // alwaysOnTop
 *     true);  // showWindowButtons
 * @endcode
 *
 * @note macOS only — BackgroundBlur is a no-op on other platforms.
 *
 * @see BackgroundBlur
 * @see GlassComponent
 */

namespace jreng
{
/*____________________________________________________________________________*/

/**
 * @class GlassWindow
 * @brief DocumentWindow that applies a native background blur on first show.
 *
 * Inherits juce::DocumentWindow for standard windowing behaviour and
 * juce::AsyncUpdater (private) to safely defer blur application until the
 * native peer is ready.
 *
 * @par Lifecycle
 * 1. Constructor configures the window (title bar, opacity, resizability).
 * 2. On first @c visibilityChanged(), @c triggerAsyncUpdate() is called.
 * 3. @c handleAsyncUpdate() calls BackgroundBlur::apply() and, optionally,
 *    BackgroundBlur::hideWindowButtons().
 *
 * @see BackgroundBlur::apply()
 * @see BackgroundBlur::hideWindowButtons()
 */
class GlassWindow
    : public juce::DocumentWindow
    , private juce::AsyncUpdater
{
public:
    /**
     * @brief Constructs and displays a glass-effect window.
     *
     * Sets up the native title bar, content component, opacity, and schedules
     * the background blur for deferred application.
     *
     * @param mainComponent  Heap-allocated component to own as window content.
     *                       Ownership is transferred via @c setContentOwned().
     * @param name           Window title string shown in the title bar.
     * @param colour         Base background colour; alpha is overridden by
     *                       @p opacity.
     * @param opacity        Alpha value [0, 1] applied to @p colour.  Values
     *                       below 1.0 allow the blur to show through.
     * @param blur           Blur radius in points passed to BackgroundBlur.
     *                       Typical range: 10–40.
     * @param alwaysOnTop    When @c true the window floats above all others.
     * @param showWindowButtons  When @c false the close/minimise/zoom traffic-
     *                           light buttons are hidden after blur is applied.
     *
     * @note On iOS / Android the window is set to full-screen; on desktop it
     *       is resizable and centred.
     *
     * @see BackgroundBlur::apply()
     * @see BackgroundBlur::hideWindowButtons()
     */
    GlassWindow (juce::Component* mainComponent,
                 juce::String const& name,
                 juce::Colour colour,
                 float opacity,
                 float blur,
                 bool alwaysOnTop,
                 bool showWindowButtons = true);

    /**
     * @brief Requests application quit when the window close button is pressed.
     *
     * Delegates to juce::JUCEApplication::systemRequestedQuit(), which allows
     * the application to veto the quit if needed.
     *
     * @note Override in a subclass if different close behaviour is required.
     */
    void closeButtonPressed() override;

    /**
     * @brief Triggers deferred blur application on first visibility change.
     *
     * Called by JUCE whenever the window's visibility changes.  If the blur
     * has not yet been applied, @c triggerAsyncUpdate() is called so that
     * @c handleAsyncUpdate() runs on the message thread after the native peer
     * is fully initialised.
     *
     * @note Subsequent visibility changes are ignored once @c blurApplied is
     *       @c true.
     *
     * @see handleAsyncUpdate()
     */
    void visibilityChanged() override;

    /**
     * @brief Applies the background blur and optional button hiding.
     *
     * Executed on the JUCE message thread after @c triggerAsyncUpdate().
     * Calls BackgroundBlur::apply() with the stored @c blurRadius.  If blur
     * succeeds and @c shouldShowWindowButtons is @c false, calls
     * BackgroundBlur::hideWindowButtons().  Also ensures the process is in the
     * foreground.
     *
     * @note This method is private to juce::AsyncUpdater; do not call it
     *       directly.
     *
     * @see BackgroundBlur::apply()
     * @see BackgroundBlur::hideWindowButtons()
     */
    void handleAsyncUpdate() override;


#if JUCE_WINDOWS
    /** @brief Meta+click anywhere drags the window (no title bar on Windows). */
    WindowControlKind findControlAtPoint (juce::Point<float> pt) const override;
#endif

private:
#if JUCE_WINDOWS
    struct TransparentTitleBarLookAndFeel : public juce::LookAndFeel_V4
    {
        void drawDocumentWindowTitleBar (juce::DocumentWindow&, juce::Graphics&,
                                        int, int, int, int,
                                        const juce::Image*, bool) override {}
    };

    TransparentTitleBarLookAndFeel transparentTitleBarLnf;
#endif

    /** @brief Blur radius (in points) forwarded to BackgroundBlur::apply(). */
    float blurRadius { 0.0f };

    /** @brief Tint colour (with alpha) forwarded to BackgroundBlur::apply(). */
    juce::Colour tintColour { juce::Colours::transparentBlack };

    /** @brief Guards against applying the blur more than once. */
    bool blurApplied { false };

    /** @brief When @c false, traffic-light buttons are hidden after blur. */
    bool shouldShowWindowButtons { true };

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GlassWindow)
};

/**_____________________________END_OF_NAMESPACE______________________________*/
} /** namespace jreng */
