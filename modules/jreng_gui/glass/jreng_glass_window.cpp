/**
 * @file jreng_glass_window.cpp
 * @brief Implementation of GlassWindow — glassmorphism DocumentWindow.
 *
 * @par macOS
 * Blur is deferred via AsyncUpdater because the window server requires
 * the window to be fully presented before CoreGraphics blur APIs work.
 * A one-shot guard (@c glassAppliedOnFirstShow) prevents re-triggering
 * on subsequent visibility changes.
 *
 * @par Windows
 * DWM glass is applied synchronously — the caller invokes
 * @c setGlass() after construction.  No AsyncUpdater needed.
 *
 * @see jreng_glass_window.h
 * @see BackgroundBlur
 */

#include "MainComponent.h"
namespace jreng
{
/*____________________________________________________________________________*/

GlassWindow::GlassWindow (juce::Component* mainComponent,
                          juce::String const& name,
                          juce::Colour colour,
                          float opacity,
                          float blur,
                          bool alwaysOnTop,
                          bool showWindowButtons)
    : juce::DocumentWindow (name,
                            juce::Colours::transparentBlack,
#if JUCE_WINDOWS
                            showWindowButtons ? juce::DocumentWindow::allButtons : 0)
#else
                            juce::DocumentWindow::allButtons)
#endif
    , blurRadius (blur)
    , tintColour (colour.withAlpha (opacity))
    , windowColour (colour)
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

void GlassWindow::closeButtonPressed() { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }

// =============================================================================
// Glass API
// =============================================================================

void GlassWindow::setGlass (bool enabled, juce::Colour colour, float opacity, float blur)
{
    windowColour = colour;
    tintColour = colour.withAlpha (opacity);
    blurRadius = blur;

    if (enabled)
    {
        setOpaque (false);
        setBackgroundColour (juce::Colours::transparentBlack);
        BackgroundBlur::enable (this, blurRadius, tintColour);

        if (not shouldShowWindowButtons)
            BackgroundBlur::hideWindowButtons (this);
    }
    else
    {
        BackgroundBlur::disable (this);
        setOpaque (true);
        setBackgroundColour (windowColour);
    }
}

// =============================================================================
// macOS: deferred first-show blur
// =============================================================================

#if JUCE_MAC

void GlassWindow::visibilityChanged()
{
    if (not isBlurApplied)
        triggerAsyncUpdate();
}

void GlassWindow::handleAsyncUpdate()
{
    isBlurApplied = true;
    setGlass (true, windowColour, tintColour.getFloatAlpha(), blurRadius);

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
