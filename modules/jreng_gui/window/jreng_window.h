/**
 * @file jreng_window.h
 * @brief JUCE DocumentWindow with glassmorphism (frosted-glass) visual effect.
 *
 * @par Overview
 * Window wraps juce::DocumentWindow and applies a native macOS background
 * blur via BackgroundBlur after the window becomes visible.  Window chrome
 * (title hiding, style mask, traffic-light buttons) is configured synchronously
 * in visibilityChanged() to eliminate the native titlebar flash on first show.
 * Blur itself is deferred through juce::AsyncUpdater so that the native peer is
 * guaranteed to be fully presented before CoreGraphics blur APIs are called.
 *
 * @par Usage
 * @code
 * auto* win = new jreng::Window (
 *     std::make_unique<MainComponent>().release(),
 *     "My App",
 *     false,  // alwaysOnTop
 *     true);  // showWindowButtons
 * win->setGlass (juce::Colours::black.withAlpha (0.6f), 20.0f);
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
 * @class Window
 * @brief DocumentWindow with optional glassmorphism (frosted-glass blur).
 *
 * On macOS, window chrome (title hiding, style mask, traffic-light buttons)
 * is configured synchronously on first visibility via
 * BackgroundBlur::configureWindowChrome() to prevent the native titlebar
 * flash.  Blur is deferred via juce::AsyncUpdater because the window server
 * requires the window to be fully presented before CoreGraphics blur APIs
 * take effect.  On Windows, glass and DWM rounded corners are applied
 * synchronously on first visibility.
 *
 * @par Usage
 * Construct the window, call setGlass() to configure colour/blur,
 * then call setVisible(true).  colour.getFloatAlpha() < 1.0 activates glass
 * on first show.  Alpha >= 1.0 produces a standard opaque window.
 * First-show blur is applied automatically on both platforms.
 *
 * @see BackgroundBlur
 */
class Window
    : public juce::DocumentWindow
    , public juce::ComponentBoundsConstrainer
#if JUCE_MAC
    , private juce::AsyncUpdater
#endif
{
public:
    Window (juce::Component* mainComponent,
            juce::String const& name,
            bool alwaysOnTop,
            bool showWindowButtons = true);

    ~Window() override;

    void resized() override;

    void closeButtonPressed() override;

    /**
     * @brief Sets glass properties — colour (with alpha as tint) and blur radius.
     *
     * When enabled: updates stored values, makes window non-opaque,
     * and applies native blur via BackgroundBlur.
     * When disabled: removes blur and restores opaque background.
     * Alpha < 1.0 enables glass; alpha >= 1.0 produces opaque window.
     */
    void setGlass (juce::Colour colour, float blur);

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

    /**
     * @brief Tells the window whether the GPU renderer is active.
     *
     * Must be called by the application layer before setVisible(true).
     * On Windows, visibilityChanged() reads this flag to select the glass
     * strategy: GPU path uses BackgroundBlur (DWM acrylic); CPU path uses
     * child-HWND compositing (Step 4).
     *
     * @param isGpu  True when the OpenGL renderer is active.
     */
    void setGpuRenderer (bool isGpu) noexcept;

    /** @brief Returns true while the user is actively resizing the window. */
    bool isUserResizing() const noexcept { return userResizing; }

    void resizeStart() override;
    void resizeEnd() override;

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

    /** @brief True when the OpenGL renderer is active — governs Windows glass strategy. */
    bool gpuRenderer { true };

    /** @brief True while the user is actively dragging a resize handle. */
    bool userResizing { false };

#if JUCE_WINDOWS
    /** @brief Handles middle-click window dragging. */
    juce::ComponentDragger windowDragger;
#endif

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Window)
};

/**_____________________________END_OF_NAMESPACE______________________________*/
} /** namespace jreng */

