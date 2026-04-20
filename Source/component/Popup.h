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
 * - The ModalWindow owns the content component via setContentOwned().
 * - Content size is computed from Config fractions in show().
 *
 * ### GL rendering
 * Pass a non-null `std::unique_ptr<jam::GLRenderer>` to `show()` for GPU
 * mode. The renderer ownership is transferred to `jam::Window` (via
 * `Terminal::ModalWindow`). Pass `nullptr` for CPU mode. In either case the
 * caller extracts the shared context handle from the root `jam::Window` and
 * forwards it so the popup renderer inherits the same GL context.
 *
 * ### Glass blur
 * Terminal::ModalWindow inherits jam::ModalWindow which inherits
 * jam::Window.  Blur is applied on first visibility via the
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
 * @see jam::BackgroundBlur
 * @see jam::GLRenderer
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
 * auto renderer { std::make_unique<jam::GLAtlasRenderer>(...) }; // or nullptr
 * popup.show (*this, std::move (content), width, height, std::move (renderer));
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
     * The shared context handle is extracted from @p caller's top-level
     * `jam::Window` and forwarded to the `Terminal::ModalWindow`. Pass a
     * non-null @p renderer for GPU mode; pass `nullptr` for CPU mode.
     *
     * Ownership of @p content and @p renderer transfers to the dialog.
     *
     * @param caller    The component to centre the dialog around.
     * @param content   The component to host; ownership is transferred.
     * @param width     Popup width in logical pixels.
     * @param height    Popup height in logical pixels.
     * @param renderer  GL renderer; ownership transferred. nullptr = CPU mode.
     * @note MESSAGE THREAD.
     * @see dismiss
     */
    void show (juce::Component& caller,
               std::unique_ptr<juce::Component> content,
               int width,
               int height,
               std::unique_ptr<jam::GLRenderer> renderer);

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
