/**
 * @file ModalWindow.h
 * @brief Reusable glass modal window for the terminal emulator.
 *
 * Terminal::ModalWindow wraps jreng::ModalWindow with glass blur,
 * DWM rounded corners, L&F inheritance from the caller, and optional
 * GL rendering via the base `jreng::Window` renderer ownership model.
 *
 * The single constructor accepts a `std::unique_ptr<jreng::GLRenderer>` —
 * pass non-null for GPU mode, nullptr for CPU mode.  The renderer is
 * forwarded to `jreng::Window::setRenderer`; the shared context handle
 * (HGLRC) is forwarded to `jreng::Window::setNativeSharedContext` so the
 * popup renderer inherits the root window's GL context.
 *
 * @see jreng::ModalWindow
 * @see Terminal::Popup
 * @see Action::List
 */

#pragma once
#include <JuceHeader.h>
#include "../config/Config.h"

namespace Terminal
{ /*____________________________________________________________________________*/

/**
 * @class ModalWindow
 * @brief Reusable glass modal for the terminal emulator.
 *
 * Provides glass blur, DWM rounded corners, L&F inheritance, border paint,
 * and GL rendering via `jreng::Window::setRenderer`. Content is passed
 * directly to the base window (no ContentView wrapper).
 *
 * @see jreng::ModalWindow
 * @see Terminal::Popup
 * @see Action::List
 */
class ModalWindow : public jreng::ModalWindow
{
public:
    /**
     * @brief Constructs a modal glass window.
     *
     * Calls `setNativeSharedContext`, `setRenderer`, `setupWindow`,
     * `setVisible`, and `enterModalState` in order. Wires
     * `Terminal::Display::onRepaintNeeded` if @p content is a
     * `Terminal::Display`.
     *
     * @param content              Content component; ownership transferred.
     * @param centreAround         Component to centre the dialog on; L&F inherited.
     * @param renderer             GL renderer; ownership transferred. nullptr = CPU mode.
     * @param nativeSharedContext  Shared HGLRC from root Window; nullptr if no sharing.
     * @param dismissCallback      Called when the dialog is dismissed.
     * @note MESSAGE THREAD.
     */
    ModalWindow (std::unique_ptr<juce::Component> content,
                 juce::Component& centreAround,
                 std::unique_ptr<jreng::GLRenderer> renderer,
                 void* nativeSharedContext,
                 std::function<void()> dismissCallback);

    void paint (juce::Graphics& g) override;
    bool keyPressed (const juce::KeyPress& key) override;

    /** @brief Window corner radius in float. */
#if JUCE_MAC
    static constexpr float cornerSize { 14.0f };
#elif JUCE_WINDOWS
    static constexpr float cornerSize { 8.0f };
#endif

protected:
    Config& config { *Config::getContext() };

private:
    //==========================================================================
    /**
     * @brief Shared setup: sets L&F on content, calls setResizable,
     * centreAroundComponent, and setGlass.
     *
     * @param centreAround  Component to centre around; L&F inherited from this.
     */
    void setupWindow (juce::Component& centreAround);

    //==========================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ModalWindow)
};

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Terminal
