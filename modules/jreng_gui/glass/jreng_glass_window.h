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
 *     false,  // alwaysOnTop
 *     true);  // showWindowButtons
 * win->setGlass (juce::Colours::black, 0.6f, 20.0f);
 * win->setVisible (true);
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
 * blur APIs take effect.  On Windows, glass and DWM rounded corners are
 * applied synchronously on first visibility.
 *
 * @par Usage
 * Construct the window, call setGlass() to configure colour/opacity/blur,
 * then call setVisible(true).  opacity < 1.0 activates glass on first show.
 * opacity >= 1.0 produces a standard opaque window.
 * First-show blur is applied automatically on both platforms.
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
                 bool alwaysOnTop,
                 bool showWindowButtons = true);

    ~GlassWindow() override { setLookAndFeel (nullptr); }

    void closeButtonPressed() override;

    /**
     * @brief Sets glass properties — blur, tint, and opacity.
     *
     * When enabled: updates stored values, makes window non-opaque,
     * and applies native blur via BackgroundBlur.
     * When disabled: removes blur and restores opaque background.
     */
    void setGlass (juce::Colour colour, float opacity, float blur);

    /** @brief One-shot: triggers deferred blur on first visibility. */
    void visibilityChanged() override;
#if JUCE_MAC
    void handleAsyncUpdate() override;
#endif

#if JUCE_WINDOWS
    /** @brief Middle-click anywhere initiates a window drag (Windows). */
    void mouseDown (const juce::MouseEvent& event) override;
    /** @brief Continues the middle-click window drag (Windows). */
    void mouseDrag (const juce::MouseEvent& event) override;
#endif

private:
#if JUCE_WINDOWS
    struct WindowsTitleBarLookAndFeel : public juce::LookAndFeel_V4
    {
        void
        drawDocumentWindowTitleBar (juce::DocumentWindow&, juce::Graphics&, int, int, int, int, const juce::Image*, bool)
            override
        {
        }
    };

    WindowsTitleBarLookAndFeel windowsTitleBar;
#endif
    /** @brief Blur radius (in points) forwarded to BackgroundBlur::enable(). */
    float blurRadius { 0.0f };

    /** @brief Tint colour (with alpha) forwarded to BackgroundBlur::enable(). */
    juce::Colour tintColour { juce::Colours::black };

    /** @brief Opaque background colour used when glass is disabled. */
    juce::Colour windowColour { juce::Colours::black };

    /** @brief When @c false, traffic-light buttons are hidden after blur. */
    bool shouldShowWindowButtons { true };

    /** @brief One-shot guard — prevents re-triggering async blur. */
    bool isBlurApplied { false };

#if JUCE_WINDOWS
    /** @brief Handles middle-click window dragging. */
    juce::ComponentDragger windowDragger;
#endif

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GlassWindow)
};

/**_____________________________END_OF_NAMESPACE______________________________*/
} /** namespace jreng */

