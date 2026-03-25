/**
 * @file Popup.h
 * @brief Modal glass dialog for the terminal emulator.
 *
 * Terminal::Popup creates a modal `juce::DialogWindow` with native glass blur
 * that hosts any `juce::Component` as content.  The dialog blocks input to all
 * other components while active and dismisses on Escape or close button.
 *
 * ### Ownership
 * - Popup owns the `DialogWindow` via `std::unique_ptr`.
 * - The `DialogWindow` owns the ContentView via `setContentOwned()`.
 * - ContentView owns the actual content component via `std::unique_ptr`.
 * - Content size is computed from Config fractions in `show()`.
 *
 * ### GL rendering
 * If the content is a `jreng::GLComponent` (e.g. `Terminal::Component`), the
 * ContentView creates its own `jreng::GLRenderer`, attaches it to itself, and
 * wires the component iterator and repaint callback.  This mirrors the pattern
 * used by `jreng::GlassWindow` + `MainComponent`.  Non-GL content works
 * without any GL setup.
 *
 * ### Glass blur
 * The inner `Window` class applies `jreng::BackgroundBlur` on first
 * visibility via deferred `juce::AsyncUpdater`, identical to the pattern
 * in `jreng::GlassWindow`.
 *
 * ### Dismiss mechanisms
 * - **Escape** — `escapeKeyTriggersCloseButton` is `true` in the
 *   `DialogWindow` constructor, so JUCE calls `closeButtonPressed()`.
 * - **Close button** — `closeButtonPressed()` calls `exitModalState (0)`.
 * - **Click outside** — `inputAttemptWhenModal()` brings window to front
 *   (does NOT dismiss).
 *
 * ### Config keys read
 * - `Config::Key::popupWidth`       — fraction of caller width  (float, 0.1–1.0)
 * - `Config::Key::popupHeight`      — fraction of caller height (float, 0.1–1.0)
 * - `Config::Key::windowColour`     — glass tint colour (hex string)
 * - `Config::Key::windowOpacity`    — glass opacity (float, 0.0–1.0)
 * - `Config::Key::windowBlurRadius` — blur radius in points (float)
 *
 * @note All methods are called on the **MESSAGE THREAD**.
 *
 * @see Config
 * @see jreng::BackgroundBlur
 * @see jreng::GLRenderer
 * @see juce::DialogWindow
 */

#pragma once
#include <JuceHeader.h>
#include "../config/Config.h"

namespace Terminal
{ /*____________________________________________________________________________*/
/**
 * @class Popup
 * @brief Modal glass dialog that hosts an arbitrary content component.
 *
 * @par Usage
 * @code
 * auto content { std::make_unique<Terminal::Component>() };
 * popup.show (*getTopLevelComponent(), std::move (content));
 * @endcode
 *
 * @par Thread context
 * **MESSAGE THREAD** — all public methods.
 *
 * @see Config::Key::popupWidth
 * @see Config::Key::popupHeight
 * @see juce::DialogWindow
 */
class Popup
{
public:
    Popup() = default;
    ~Popup();

    /**
     * @brief Shows a modal glass dialog centred on @p caller with @p content inside.
     *
     * Reads `popupWidth` and `popupHeight` from Config, computes pixel size
     * as fractions of @p caller's bounds, sets that size on the content, then
     * creates the dialog window, centres it on @p caller, and enters modal
     * state.
     *
     * If the content is a `jreng::GLComponent`, the ContentView sets up its
     * own GL rendering pipeline (renderer, iterator, repaint callback).
     *
     * Ownership of @p content transfers to the ContentView inside the dialog.
     *
     * @param caller   The component to centre the dialog around.
     * @param content  The component to host; ownership is transferred.
     * @note MESSAGE THREAD.
     * @see dismiss
     */
    void show (juce::Component& caller, std::unique_ptr<juce::Component> content);

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
         * `Terminal::Component::onRepaintNeeded` if applicable.
         *
         * @param content  The component to host; ownership is transferred.
         */
        explicit ContentView (std::unique_ptr<juce::Component> content);

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

        /** @brief Non-owning pointer to the GL content; nullptr for non-GL content. */
        jreng::GLComponent* glContent { nullptr };

        //======================================================================
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ContentView)
    };

    //==========================================================================
    /**
     * @struct Window
     * @brief Glass DialogWindow with native blur and Escape dismiss.
     *
     * Subclasses `juce::DialogWindow` with `escapeKeyTriggersCloseButton = true`
     * so JUCE handles Escape -> `closeButtonPressed()` automatically.
     * Inherits `juce::AsyncUpdater` to defer blur application until the native
     * peer is ready.
     *
     * GL rendering is delegated entirely to the `ContentView` content component.
     *
     * @see jreng::BackgroundBlur
     * @see ContentView
     */
    struct Window : public juce::DialogWindow
                  , private juce::AsyncUpdater
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
                std::function<void()> dismissCallback);

        void paint (juce::Graphics& g) override;
        void closeButtonPressed() override;
        void inputAttemptWhenModal() override;
        void visibilityChanged() override;
        void handleAsyncUpdate() override;

    private:
        Config& config { *Config::getContext() };
        std::function<void()> onDismissed;
        bool blurApplied { false };

        //======================================================================
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Window)
    };

    //==========================================================================
    Config& config { *Config::getContext() };
    std::unique_ptr<Window> window;

    //==========================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Popup)
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Terminal
