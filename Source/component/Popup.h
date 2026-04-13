/**
 * @file Popup.h
 * @brief Modal glass dialog for the terminal emulator.
 *
 * Terminal::Popup creates a modal Terminal::ModalWindow with native glass blur
 * that hosts any juce::Component as content.  The dialog blocks input to all
 * other components while active and dismisses on Escape or close button.
 *
 * ### Ownership
 * - Popup owns the ModalWindow via std::unique_ptr.
 * - The ModalWindow owns the ContentView via setContentOwned().
 * - ContentView owns the actual content component via std::unique_ptr.
 * - Content size is computed from Config fractions in show().
 *
 * ### GL rendering
 * If the content is a jreng::GLComponent (e.g. Terminal::Display), the
 * ContentView creates its own jreng::GLRenderer, attaches it to itself, and
 * wires the component iterator and repaint callback.  This mirrors the pattern
 * used by jreng::Window + MainComponent.  Non-GL content works
 * without any GL setup.
 *
 * ### Glass blur
 * Terminal::ModalWindow inherits jreng::ModalWindow which inherits
 * jreng::Window.  Blur is applied on first visibility via the
 * Window deferred mechanism.
 *
 * ### Dismiss mechanisms
 * - **Escape** — ModalWindow::keyPressed() handles Escape and calls closeButtonPressed().
 * - **Close button** — closeButtonPressed() calls exitModalState (0).
 * - **Click outside** — inputAttemptWhenModal() brings window to front
 *   (does NOT dismiss).
 *
 * ### Config keys read
 * - Config::Key::windowColour     — glass tint colour (hex string)
 * - Config::Key::windowOpacity    — glass opacity (float, 0.0–1.0)
 * - Config::Key::windowBlurRadius — blur radius in points (float)
 *
 * @note All methods are called on the MESSAGE THREAD.
 *
 * @see Config
 * @see jreng::BackgroundBlur
 * @see jreng::GLRenderer
 * @see Terminal::ModalWindow
 */

#pragma once
#include <JuceHeader.h>
#include "../config/Config.h"
#include "../terminal/logic/Session.h"
#include "ModalWindow.h"

namespace Terminal
{ /*____________________________________________________________________________*/
/**
 * @class Popup
 * @brief Modal glass dialog that hosts an arbitrary content component.
 *
 * @par Usage
 * @code
 * auto content { std::make_unique<Terminal::Display>() };
 * popup.show (*getTopLevelComponent(), std::move (content));
 * @endcode
 *
 * @par Thread context
 * MESSAGE THREAD — all public methods.
 *
 * @see Terminal::ModalWindow
 */
class Popup
{
public:
    Popup() = default;
    ~Popup();

    /**
     * @brief Shows a modal glass dialog centred on @p caller with @p content inside.
     *
     * Sets the given pixel @p width and @p height on the content, then
     * creates the dialog window, centres it on @p caller, and enters modal
     * state.
     *
     * If the content is a jreng::GLComponent, the ContentView sets up its
     * own GL rendering pipeline (renderer, iterator, repaint callback).
     *
     * Ownership of @p content transfers to the ContentView inside the dialog.
     *
     * @param caller          The component to centre the dialog around.
     * @param content         The component to host; ownership is transferred.
     * @param width           Popup width in logical pixels.
     * @param height          Popup height in logical pixels.
     * @param sharedRenderer  Main window's GL renderer; popup shares its GL context.
     * @note MESSAGE THREAD.
     * @see dismiss
     */
    void show (juce::Component& caller,
               std::unique_ptr<juce::Component> content,
               int width,
               int height,
               jreng::GLRenderer& sharedRenderer);

    /**
     * @brief Shows a modal glass dialog without GL rendering.
     *
     * @param caller   The component to centre the dialog around.
     * @param content  The component to host; ownership is transferred.
     * @param width    Popup width in logical pixels.
     * @param height   Popup height in logical pixels.
     * @note MESSAGE THREAD.
     */
    void show (juce::Component& caller,
               std::unique_ptr<juce::Component> content,
               int width,
               int height);

    /**
     * @brief Dismisses the dialog if active.
     *
     * Calls exitModalState (0) on the dialog window and releases it.
     *
     * @note MESSAGE THREAD.
     * @see show
     */
    void dismiss();

    /**
     * @brief Returns whether the dialog is currently active.
     *
     * @return true if the dialog window exists and is visible.
     * @note MESSAGE THREAD.
     */
    bool isActive() const noexcept;

    /** @brief Callback invoked when the dialog is dismissed (Escape or close button). */
    std::function<void()> onDismiss;

    /**
     * @brief Sets the PTY session owned by this popup.
     *
     * Transfers ownership of @p session into the popup. The session is
     * destroyed when the popup is dismissed, after the Display has been torn down.
     *
     * @param session  PTY session to transfer.
     * @note MESSAGE THREAD.
     */
    void setTerminalSession (std::unique_ptr<Terminal::Session> session);

private:
    //==========================================================================
    void removePopupSession();

    //==========================================================================
    Config& config { *Config::getContext() };
    std::unique_ptr<Terminal::ModalWindow> window;
    std::unique_ptr<Terminal::Session> terminalSession;
    juce::String popupSessionUuid;

    //==========================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Popup)
};

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Terminal
