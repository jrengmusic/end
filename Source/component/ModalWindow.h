/**
 * @file ModalWindow.h
 * @brief Terminal-specific modal window wrapper over jam::ModalWindow.
 *
 * Terminal::ModalWindow is a thin wrapper that delegates the generic modal
 * setup sequence (setNativeSharedContext, setRenderer, centreAroundComponent,
 * setGlass, setVisible, enterModalState) to jam::ModalWindow's caller-ctor.
 * Renderer flows through the base; derived does not manually wire renderable.
 *
 * This wrapper retains Terminal-specific concerns only:
 *  - Terminal::Display::onRepaintNeeded callback
 *  - paint() border drawn from Config colour/size keys
 *  - keyPressed() override
 *  - cornerSize platform constants
 *  - Config injection (config member)
 *
 * @see jam::ModalWindow
 * @see Terminal::Popup
 * @see Action::List
 */

#pragma once
#include <JuceHeader.h>
#include "../lua/Engine.h"

namespace Terminal
{ /*____________________________________________________________________________*/

/**
 * @class ModalWindow
 * @brief Terminal-specific modal wrapper over jam::ModalWindow.
 *
 * Delegates generic modal setup (L&F inheritance, centring, glass,
 * setVisible, enterModalState) to jam::ModalWindow's caller-ctor.
 * Renderer flows through the base. Adds Terminal-specific
 * Display::onRepaintNeeded callback, paint border, and Config injection.
 *
 * @see jam::ModalWindow
 * @see Terminal::Popup
 * @see Action::List
 */
class ModalWindow : public jam::ModalWindow
{
public:
    /**
     * @brief Constructs a Terminal modal glass window.
     *
     * Forwards all args to jam::ModalWindow's caller-ctor, which handles
     * setNativeSharedContext, setRenderer (including attachRendererToContent
     * tree-walk), centreAroundComponent, setGlass, setVisible, and
     * enterModalState. Wires Terminal::Display::onRepaintNeeded if content
     * is a Terminal::Display.
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
                 std::unique_ptr<jam::gl::Renderer> renderer,
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

    // No config member — access via lua::Engine::getContext() at call site.

private:
    //==========================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ModalWindow)
};

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Terminal
