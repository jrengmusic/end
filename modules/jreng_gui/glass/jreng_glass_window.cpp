/**
 * @file jreng_glass_window.cpp
 * @brief Implementation of GlassWindow — glassmorphism DocumentWindow.
 *
 * @par Blur Deferral Strategy
 * The native peer (NSWindow) is not available at construction time.  JUCE
 * creates it lazily when the component first becomes visible.  By hooking
 * @c visibilityChanged() and posting an async update we guarantee that
 * @c getPeer() returns a valid pointer when BackgroundBlur::apply() is called.
 *
 * @see jreng_glass_window.h
 * @see BackgroundBlur
 */

#include "MainComponent.h"
namespace jreng
{
/*____________________________________________________________________________*/

/**
 * @brief Constructs the glass window and makes it visible.
 *
 * Initialisation order:
 * 1. Base DocumentWindow constructed with native title bar and @p colour
 *    pre-multiplied by @p opacity.
 * 2. @c blurRadius and @c shouldShowWindowButtons stored for later use.
 * 3. Window configured: native title bar, content owned, always-on-top,
 *    non-opaque (required for blur transparency).
 * 4. Platform-specific sizing: full-screen on mobile, resizable on desktop.
 * 5. @c setVisible(true) triggers @c visibilityChanged() → async blur.
 *
 * @param mainComponent  Content component; ownership transferred.
 * @param name           Window title.
 * @param colour         Background colour (alpha overridden by @p opacity).
 * @param opacity        Window background alpha [0, 1].
 * @param blur           Blur radius forwarded to BackgroundBlur::apply().
 * @param alwaysOnTop    Float window above all others when @c true.
 * @param showWindowButtons  Hide traffic-light buttons when @c false.
 */
GlassWindow::GlassWindow (juce::Component* mainComponent,
                          juce::String const& name,
                          juce::Colour colour,
                          float opacity,
                          float blur,
                          bool alwaysOnTop,
                          bool showWindowButtons)
    : juce::DocumentWindow (name, juce::Colours::transparentBlack,
#if JUCE_WINDOWS
        showWindowButtons ? juce::DocumentWindow::allButtons : 0)
#else
        juce::DocumentWindow::allButtons)
#endif
    , blurRadius (blur)
    , tintColour (colour.withAlpha (opacity))
    , shouldShowWindowButtons (showWindowButtons)
{
#if JUCE_WINDOWS
    setUsingNativeTitleBar (showWindowButtons);
    if (not showWindowButtons)
    {
        setTitleBarHeight (10);
        setLookAndFeel (&transparentTitleBarLnf);
    }
#else
    setUsingNativeTitleBar (true);
#endif
    setOpaque (false);
    setContentOwned (std::move (mainComponent), true);
    setAlwaysOnTop (alwaysOnTop);
#if JUCE_IOS || JUCE_ANDROID
    setFullScreen (true);
#elif JUCE_WINDOWS
    setResizable (true, false);
#else
    setResizable (true, true);
#endif
    centreWithSize (getWidth(), getHeight());
    setVisible (true);
}

/**
 * @brief Forwards close-button press to the application quit mechanism.
 *
 * Calls juce::JUCEApplication::systemRequestedQuit() which dispatches
 * @c JUCEApplication::systemRequestedQuit() giving the app a chance to
 * cancel (e.g. unsaved-changes dialog).
 */
void GlassWindow::closeButtonPressed() { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }

/**
 * @brief Schedules blur application on first visibility change.
 *
 * Guards with @c blurApplied so the async update is only triggered once.
 * Subsequent show/hide cycles are ignored.
 */
void GlassWindow::visibilityChanged()
{
    if (not blurApplied)
        triggerAsyncUpdate();
}

/**
 * @brief Applies blur (and optional button hiding) on the message thread.
 *
 * Execution flow:
 * 1. BackgroundBlur::apply() attempts CoreGraphics blur; falls back to
 *    NSVisualEffectView if unavailable.  Result stored in @c blurApplied.
 * 2. If blur succeeded and @c shouldShowWindowButtons is @c false, the
 *    native close/minimise/zoom buttons are hidden.
 * 3. Ensures the process is in the foreground so the window receives focus.
 *
 * @note Called automatically by JUCE's AsyncUpdater mechanism; never call
 *       directly.
 */
void GlassWindow::handleAsyncUpdate()
{
    blurApplied = BackgroundBlur::apply (this, blurRadius, tintColour);

    if (blurApplied and not shouldShowWindowButtons)
        BackgroundBlur::hideWindowButtons (this);

    if (not juce::Process::isForegroundProcess())
        juce::Process::makeForegroundProcess();
}

/**_____________________________END_OF_NAMESPACE______________________________*/
} /** namespace jreng */
