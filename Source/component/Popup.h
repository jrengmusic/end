/**
 * @file Popup.h
 * @brief Modal glass dialog for the terminal emulator.
 *
 * Terminal::Popup creates a modal `jreng::ModalWindow` with native glass blur
 * that hosts any `juce::Component` as content.  The dialog blocks input to all
 * other components while active and dismisses on Escape or close button.
 *
 * ### Ownership
 * - Popup owns the `ModalWindow` via `std::unique_ptr`.
 * - The `ModalWindow` owns the ContentView via `setContentOwned()`.
 * - ContentView owns the actual content component via `std::unique_ptr`.
 * - Content size is computed from Config fractions in `show()`.
 *
 * ### GL rendering
 * If the content is a `jreng::GLComponent` (e.g. `Terminal::Display`), the
 * ContentView creates its own `jreng::GLRenderer`, attaches it to itself, and
 * wires the component iterator and repaint callback.  This mirrors the pattern
 * used by `jreng::Window` + `MainComponent`.  Non-GL content works
 * without any GL setup.
 *
 * ### Glass blur
 * The inner `Window` class inherits `jreng::ModalWindow` which inherits
 * `jreng::Window`.  Blur is applied on first visibility via the
 * Window deferred mechanism.
 *
 * ### Dismiss mechanisms
 * - **Escape** — `ModalWindow::keyPressed()` handles Escape and calls `closeButtonPressed()`.
 * - **Close button** — `closeButtonPressed()` calls `exitModalState (0)`.
 * - **Click outside** — `inputAttemptWhenModal()` brings window to front
 *   (does NOT dismiss).
 *
 * ### Config keys read
 * - `Config::Key::windowColour`     — glass tint colour (hex string)
 * - `Config::Key::windowOpacity`    — glass opacity (float, 0.0–1.0)
 * - `Config::Key::windowBlurRadius` — blur radius in points (float)
 *
 * @note All methods are called on the **MESSAGE THREAD**.
 *
 * @see Config
 * @see jreng::BackgroundBlur
 * @see jreng::GLRenderer
 * @see jreng::ModalWindow
 */

#pragma once
#include <JuceHeader.h>
#include "../config/Config.h"
#include "../terminal/logic/Session.h"

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
 * **MESSAGE THREAD** — all public methods.
 *
 * @see jreng::ModalWindow
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
     * If the content is a `jreng::GLComponent`, the ContentView sets up its
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
     * @brief Dismisses the dialog if active.
     *
     * Calls `exitModalState (0)` on the dialog window and releases it.
     *
     * @note MESSAGE THREAD.
     * @see show
     */
    void dismiss();

    /**
     * @brief Returns whether the dialog is currently active.
     *
     * @return `true` if the dialog window exists and is visible.
     * @note MESSAGE THREAD.
     */
    bool isActive() const noexcept;

    /** @brief Owned PTY session; destroyed on dismiss after Display is gone. */
    std::unique_ptr<Terminal::Session> terminalSession;

    /** @brief Callback invoked when the dialog is dismissed (Escape or close button). */
    std::function<void()> onDismiss;


private:
    //==========================================================================
    /**
     * @struct ContentView
     * @brief Intermediate content component that owns the GL renderer.
     *
     * Mirrors the `MainComponent` pattern: the native window (`Window`) holds
     * a `ContentView` as its content component, and `ContentView` attaches the
     * `jreng::GLRenderer` to itself rather than to the window.
     *
     * If the owned content is a `jreng::GLComponent`, `initialiseGL()` must be
     * called after the component has a native peer (i.e. after `setVisible
     * (true)` on the parent window).
     *
     * @see jreng::GLRenderer
     * @see jreng::GLComponent
     */
    struct ContentView : public juce::Component
    {
        /**
         * @brief Constructs the content view and takes ownership of @p content.
         *
         * Detects whether @p content is a `jreng::GLComponent` and, if so,
         * stores a non-owning pointer for the renderer iterator.  Also wires
         * `Terminal::Display::onRepaintNeeded` if applicable.
         *
         * @param content  The component to host; ownership is transferred.
         */
        ContentView (std::unique_ptr<juce::Component> content, jreng::GLRenderer& sharedRenderer);

        ~ContentView() override;

        void resized() override;

        /**
         * @brief Attaches the GL renderer to this component.
         *
         * Must be called after the parent window is visible so that a native
         * peer exists.  Does nothing if the owned content is not a
         * `jreng::GLComponent`.
         */
        void initialiseGL();

    private:
        /** @brief Owned content component displayed inside this view. */
        std::unique_ptr<juce::Component> ownedContent;

        /** @brief GL renderer attached to this component; active only for GL content. */
        jreng::GLRenderer glRenderer;

        /** @brief Main window's GL renderer; used to share the GL context. */
        jreng::GLRenderer& sharedSource;

        /** @brief Non-owning pointer to the GL content; nullptr for non-GL content. */
        jreng::GLComponent* glContent { nullptr };

        //======================================================================
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ContentView)
    };

    //==========================================================================
    /**
     * @struct Window
     * @brief Glass ModalWindow with border paint and GL content support.
     *
     * Subclasses `jreng::ModalWindow` which provides Escape dismiss,
     * close button handling, input blocking, and deferred glass blur
     * via `jreng::Window`.
     *
     * GL rendering is delegated entirely to the `ContentView` content component.
     *
     * @see jreng::ModalWindow
     * @see ContentView
     */
    struct Window : public jreng::ModalWindow
    {
        /**
         * @brief Constructs the glass dialog window.
         *
         * Wraps @p content in a `ContentView`, sets it as the owned content,
         * then calls `ContentView::initialiseGL()` after the window is visible.
         *
         * @param content          Content component; ownership transferred to ContentView.
         * @param centreAround     Component to centre the dialog on.
         * @param dismissCallback  Called when the dialog is dismissed.
         */
        Window (std::unique_ptr<juce::Component> content,
                juce::Component& centreAround,
                jreng::GLRenderer& sharedRenderer,
                std::function<void()> dismissCallback);

        void paint (juce::Graphics& g) override;
        bool keyPressed (const juce::KeyPress& key) override;

        /** @brief Window corner radius in float */
#if JUCE_MAC
        static constexpr float cornerSize { 14.0f };
#elif JUCE_WINDOWS
        static constexpr float cornerSize { 8.0f };
#endif

    private:
        Config& config { *Config::getContext() };

        //======================================================================
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Window)
    };

    //==========================================================================
    void removePopupSession();

    //==========================================================================
    Config& config { *Config::getContext() };
    std::unique_ptr<Window> window;
    juce::String popupSessionUuid;

    //==========================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Popup)
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Terminal
