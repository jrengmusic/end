/**
 * @file jreng_glass_window.cpp
 * @brief Implementation of GlassWindow — glassmorphism DocumentWindow.
 *
 * @par macOS
 * Blur is deferred via AsyncUpdater because the window server requires
 * the window to be fully presented before CoreGraphics blur APIs work.
 * A one-shot guard (@c isBlurApplied) prevents re-triggering
 * on subsequent visibility changes.
 *
 * @par Windows
 * Glass and DWM rounded corners are applied synchronously on first
 * visibility.  No AsyncUpdater needed.
 *
 * @see jreng_glass_window.h
 * @see BackgroundBlur
 */

#include "MainComponent.h"

#if JUCE_WINDOWS
#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")
#endif

namespace jreng
{
/*____________________________________________________________________________*/

GlassWindow::GlassWindow (juce::Component* mainComponent,
                          juce::String const& name,
                          bool alwaysOnTop,
                          bool showWindowButtons)
    : juce::DocumentWindow (name,
                            juce::Colours::black,
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
        setLookAndFeel (&transparentTitleBarLnf);
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
    centreWithSize (getWidth(), getHeight());
}

void GlassWindow::closeButtonPressed() { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }

// =============================================================================
// Glass API
// =============================================================================

void GlassWindow::setGlass (juce::Colour colour, float opacity, float blur)
{
    windowColour = colour;
    tintColour = colour.withAlpha (opacity);
    blurRadius = blur;

    if (opacity < 1.0f)
    {
        setOpaque (false);
        setBackgroundColour (juce::Colours::transparentBlack);

        if (isBlurApplied)
        {
            BackgroundBlur::enable (this, blurRadius, tintColour);

            if (not shouldShowWindowButtons)
                BackgroundBlur::hideWindowButtons (this);
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

void GlassWindow::visibilityChanged()
{
    if (not isBlurApplied)
    {
        isBlurApplied = true;

#if JUCE_MAC
        triggerAsyncUpdate();
#elif JUCE_WINDOWS
        setGlass (windowColour, tintColour.getFloatAlpha(), blurRadius);

        if (auto* peer { getPeer() })
        {
            auto hwnd { static_cast<HWND> (peer->getNativeHandle()) };
            DWORD cornerPreference { 2 };  // DWMWCP_ROUND
            DwmSetWindowAttribute (hwnd, 33, &cornerPreference, sizeof (cornerPreference));
        }
#endif
    }
}

#if JUCE_MAC

void GlassWindow::handleAsyncUpdate()
{
    isBlurApplied = true;
    setGlass (windowColour, tintColour.getFloatAlpha(), blurRadius);

    if (not juce::Process::isForegroundProcess())
        juce::Process::makeForegroundProcess();
}

#endif

// =============================================================================
// Windows: meta-drag
// =============================================================================

#if JUCE_WINDOWS
auto GlassWindow::findControlAtPoint (juce::Point<float> pt) const -> WindowControlKind
{
    if ((GetAsyncKeyState (VK_LWIN) | GetAsyncKeyState (VK_RWIN)) & 0x8000)
        return WindowControlKind::caption;

    return DocumentWindow::findControlAtPoint (pt);
}
#endif

/**_____________________________END_OF_NAMESPACE______________________________*/
} /** namespace jreng */

