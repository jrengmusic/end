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
 * - `lua::Engine::display.window.colour`     — glass tint colour
 * - `lua::Engine::display.window.opacity`    — glass opacity (float, 0.0–1.0)
 * - `lua::Engine::display.window.blurRadius` — blur radius in points (float)
 *
 * @note All methods are called on the MESSAGE THREAD.
 *
 * @see lua::Engine
 * @see jam::BackgroundBlur
 * @see Terminal::ModalWindow
 */

#pragma once
#include <JuceHeader.h>
#include "../lua/Engine.h"
#include "../terminal/logic/Session.h"
#include "../terminal/data/Identifier.h"
#include "ModalWindow.h"

namespace Terminal
{ /*____________________________________________________________________________*/
/**
 * @class Popup
 * @brief Modal glass dialog that hosts an arbitrary content component.
 *
 * @par Usage
 * @code
 * popup.show (*this, std::move (content), width, height);
 * @endcode
 *
 * @par Thread context
 * MESSAGE THREAD — all public methods.
 *
 * @see Terminal::ModalWindow
 */
class Popup : public juce::ValueTree::Listener
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
     * Ownership of @p content transfers to the dialog.
     *
     * @param caller   The component to centre the dialog around.
     * @param content  The component to host; ownership is transferred.
     * @param width    Popup width in logical pixels.
     * @param height   Popup height in logical pixels.
     * @note MESSAGE THREAD.
     * @see dismiss
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

    // juce::ValueTree::Listener
    void valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property) override;

private:
    //==========================================================================
    void removePopupSession();

    //==========================================================================
    std::unique_ptr<Terminal::ModalWindow> window;
    std::unique_ptr<Terminal::Session> terminalSession;
    juce::String popupSessionUuid;
    juce::ValueTree watchedStateRoot;

    //==========================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Popup)
};

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Terminal
