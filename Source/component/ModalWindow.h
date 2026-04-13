/**
 * @file ModalWindow.h
 * @brief Reusable glass modal window for the terminal emulator.
 *
 * Terminal::ModalWindow wraps jreng::ModalWindow with glass blur,
 * DWM rounded corners, L&F inheritance from the caller, and optional
 * GL rendering support via an internal ContentView.
 *
 * Two constructors:
 * - GL constructor: wraps content in a GL-capable ContentView (used by Popup).
 * - Non-GL constructor: takes content directly (used by Action::List).
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
 * Extracted from Terminal::Popup::Window. Provides glass blur, DWM rounded
 * corners, L&F inheritance, border paint, and optional GL content wrapping.
 *
 * @see jreng::ModalWindow
 * @see Terminal::Popup
 * @see Action::List
 */
class ModalWindow : public jreng::ModalWindow
{
public:
    /**
     * @brief GL constructor — wraps content in a GL-capable ContentView.
     *
     * Used by Terminal::Popup for GL-rendered content (Terminal::Display).
     * Calls setVisible and enterModalState internally (GL init requires a peer).
     *
     * @param content          Content component; ownership transferred to ContentView.
     * @param centreAround     Component to centre the dialog on; L&F inherited from this.
     * @param sharedRenderer   Main window's GL renderer; popup shares its GL context.
     * @param dismissCallback  Called when the dialog is dismissed.
     */
    ModalWindow (std::unique_ptr<juce::Component> content,
                 juce::Component& centreAround,
                 jreng::GLRenderer& sharedRenderer,
                 std::function<void()> dismissCallback);

    /**
     * @brief Non-GL constructor — takes content directly without wrapping.
     *
     * Used by Action::List. Does NOT call setVisible or enterModalState —
     * the caller does that after additional setup (e.g. building rows).
     *
     * @param content       Content component; ownership transferred (JUCE DocumentWindow convention).
     * @param centreAround  Component to centre the dialog on; L&F inherited from this.
     */
    ModalWindow (std::unique_ptr<juce::Component> content,
                 juce::Component& centreAround,
                 std::function<void()> dismissCallback = nullptr);

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
     * @struct ContentView
     * @brief GL content wrapper, used only by the GL constructor.
     *
     * Mirrors the MainComponent pattern: Window holds ContentView as its
     * content component, and ContentView attaches the jreng::GLRenderer to
     * itself. Non-GL content works without any GL setup.
     *
     * @see jreng::GLRenderer
     * @see jreng::GLComponent
     */
    struct ContentView : public juce::Component
    {
        /**
         * @brief Constructs the content view and takes ownership of @p content.
         *
         * Detects whether @p content is a jreng::GLComponent and wires
         * Terminal::Display::onRepaintNeeded if applicable.
         *
         * @param content        Content component; ownership is transferred.
         * @param sharedRenderer Main window's GL renderer; context is shared.
         */
        ContentView (std::unique_ptr<juce::Component> content, jreng::GLRenderer& sharedRenderer);

        ~ContentView() override;

        void resized() override;

        /**
         * @brief Attaches the GL renderer to this component.
         *
         * Must be called after the parent window is visible so that a native
         * peer exists. Does nothing if the owned content is not a
         * jreng::GLComponent.
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
     * @brief Shared setup called by both constructors.
     *
     * Sets L&F on content, calls setResizable, centreAroundComponent, and setGlass.
     *
     * @param centreAround  Component to centre around; L&F inherited from this.
     */
    void setupWindow (juce::Component& centreAround);

    //==========================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ModalWindow)
};

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Terminal
