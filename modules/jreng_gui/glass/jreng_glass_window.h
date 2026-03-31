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
 * @brief DocumentWindow with optional glassmorphism (frosted-glass blur).
 *
 * On macOS, blur is deferred via juce::AsyncUpdater because the window
 * server requires the window to be fully presented before CoreGraphics
 * blur APIs take effect.  On Windows, DWM glass is applied synchronously
 * via @c setGlassEnabled().
 *
 * @par Usage
 * Construct the window, then call @c setGlassEnabled(true) to activate
 * glass, or leave it disabled for a standard opaque window.  On macOS
 * the first-show blur is applied automatically via the async path.
 *
 * @see BackgroundBlur
 */
class GlassWindow
    : public juce::DocumentWindow
#if JUCE_MAC
    , private juce::AsyncUpdater
#endif
{
public:
    GlassWindow (juce::Component* mainComponent,
                 juce::String const& name,
                 juce::Colour colour,
                 float opacity,
                 float blur,
                 bool alwaysOnTop,
                 bool showWindowButtons = true);

    ~GlassWindow() override { setLookAndFeel (nullptr); }

    void closeButtonPressed() override;

    /**
     * @brief Enables or disables glass (blur + transparency).
     *
     * When enabled: window becomes non-opaque and native blur is applied.
     * When disabled: blur is removed and window becomes opaque with a
     * solid background colour — identical to a standard DocumentWindow.
     *
     * On Windows this is the primary entry point for glass lifecycle.
     * On macOS the first-show blur is driven by the async path, but
     * runtime toggling (e.g. GPU ↔ CPU switch) goes through here.
     */
    void setGlassEnabled (bool enabled);

#if JUCE_MAC
    /** @brief One-shot: triggers deferred blur on first visibility. */
    void visibilityChanged() override;
    void handleAsyncUpdate() override;
#endif

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

    /** @brief Blur radius (in points) forwarded to BackgroundBlur::enable(). */
    float blurRadius { 0.0f };

    /** @brief Tint colour (with alpha) forwarded to BackgroundBlur::enable(). */
    juce::Colour tintColour { juce::Colours::transparentBlack };

    /** @brief Opaque background colour used when glass is disabled. */
    juce::Colour windowColour;

    /** @brief When @c false, traffic-light buttons are hidden after blur. */
    bool shouldShowWindowButtons { true };

#if JUCE_MAC
    /** @brief One-shot guard — prevents re-triggering async blur. */
    bool isBlurApplied { false };
#endif

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GlassWindow)
};

/**_____________________________END_OF_NAMESPACE______________________________*/
} /** namespace jreng */
