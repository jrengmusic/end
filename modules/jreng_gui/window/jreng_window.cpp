/**
 * @file jreng_window.cpp
 * @brief Implementation of Window — glassmorphism DocumentWindow.
 *
 * @par macOS
 * Window chrome (title hiding, style mask, traffic-light buttons) is applied
 * synchronously in visibilityChanged() via BackgroundBlur::configureWindowChrome()
 * to eliminate the native titlebar flash on first show.  Blur itself is still
 * deferred via AsyncUpdater because the window server requires the window to be
 * fully presented before CoreGraphics blur APIs take effect.
 * A one-shot guard (@c isBlurApplied) prevents re-triggering on subsequent
 * visibility changes.
 *
 * @par Windows
 * Glass and DWM rounded corners are applied synchronously on first
 * visibility.  No AsyncUpdater needed.
 *
 * @see jreng_window.h
 * @see BackgroundBlur
 */

#if JUCE_WINDOWS
#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")

// Named constants for DWM window attribute IDs and values.
// DWMWA_WINDOW_CORNER_PREFERENCE = 33, DWMWCP_ROUND = 2.
static constexpr DWORD dwmWindowCornerPreferenceId { 33 };
static constexpr DWORD dwmCornerRoundValue { 2 };
#endif

namespace jreng
{
/*____________________________________________________________________________*/

Window::Window (juce::Component* mainComponent,
                juce::String const& name,
                bool alwaysOnTop,
                bool showWindowButtons)
    : juce::DocumentWindow (name,
                            juce::Colours::transparentBlack,
#if JUCE_WINDOWS
                            showWindowButtons ? juce::DocumentWindow::allButtons : 0)
#else
                            juce::DocumentWindow::allButtons)
#endif
    , shouldShowWindowButtons (showWindowButtons)
{
#if JUCE_WINDOWS
    setUsingNativeTitleBar (showWindowButtons);
    if (not showWindowButtons)
    {
        setTitleBarHeight (0);
        windowsTitleBar.setColour (
            juce::DocumentWindow::backgroundColourId, findColour (juce::DocumentWindow::backgroundColourId));
        setLookAndFeel (&windowsTitleBar);
    }
#else
    setUsingNativeTitleBar (true);
#endif
    setOpaque (true);
    setContentOwned (std::move (mainComponent), true);
    setAlwaysOnTop (alwaysOnTop);
#if JUCE_IOS || JUCE_ANDROID
    setFullScreen (true);
#elif JUCE_WINDOWS
    setResizable (true, false);
#else
    setResizable (true, true);
#endif
    setConstrainer (this);
    centreWithSize (getWidth(), getHeight());
#if JUCE_WINDOWS
    addMouseListener (this, true);
#endif
}

Window::~Window() { setLookAndFeel (nullptr); }

void Window::closeButtonPressed() { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }

// =============================================================================
// Renderer type
// =============================================================================

void Window::setGpuRenderer (bool isGpu) noexcept
{
    gpuRenderer = isGpu;
}

// =============================================================================
// Glass API
// =============================================================================

void Window::setGlass (juce::Colour colour, float blur)
{
    windowColour = colour;
    tintColour = colour; // colour already carries alpha
    blurRadius = blur;

    if (colour.getFloatAlpha() < 1.0f)
    {
        setOpaque (false);
        setBackgroundColour (juce::Colours::transparentBlack);

        if (isBlurApplied)
        {
            BackgroundBlur::enable (this, blurRadius, tintColour);
        }
    }
    else
    {
        if (isBlurApplied)
            BackgroundBlur::disable (this);

        setOpaque (true);
        setBackgroundColour (windowColour);
    }
}

// =============================================================================
// Deferred first-show blur
// =============================================================================

void Window::visibilityChanged()
{
    if (not isBlurApplied)
    {
        isBlurApplied = true;

#if JUCE_MAC
        BackgroundBlur::configureWindowChrome (this, shouldShowWindowButtons);
        triggerAsyncUpdate();
#elif JUCE_WINDOWS
        if (auto* peer { getPeer() })
        {
            auto hwnd { static_cast<HWND> (peer->getNativeHandle()) };
            DWORD cornerPreference { dwmCornerRoundValue };
            DwmSetWindowAttribute (hwnd, dwmWindowCornerPreferenceId, &cornerPreference, sizeof (cornerPreference));
        }

        if (gpuRenderer)
        {
            setGlass (tintColour, blurRadius);
        }
        else
        {
            // CPU renderer: solid opaque background, no glass blur
            setOpaque (true);
            setBackgroundColour (windowColour);
        }
#endif
    }
}

#if JUCE_MAC

void Window::handleAsyncUpdate()
{
    setGlass (tintColour, blurRadius);

    if (not juce::Process::isForegroundProcess())
        juce::Process::makeForegroundProcess();
}

#endif

// =============================================================================
// Layout
// =============================================================================

void Window::resized()
{
    juce::DocumentWindow::resized();
}

// =============================================================================
// Resize tracking
// =============================================================================

void Window::resizeStart()
{
    userResizing = true;
}

void Window::resizeEnd()
{
    userResizing = false;
}

// =============================================================================
// Windows: middle-click drag
// =============================================================================

#if JUCE_WINDOWS
void Window::mouseDown (const juce::MouseEvent& event)
{
    if (event.mods.isMiddleButtonDown())
        windowDragger.startDraggingComponent (this, event);
}

void Window::mouseDrag (const juce::MouseEvent& event)
{
    if (event.mods.isMiddleButtonDown())
        windowDragger.dragComponent (this, event, nullptr);
}
#endif

/**_____________________________END_OF_NAMESPACE______________________________*/
} /** namespace jreng */

