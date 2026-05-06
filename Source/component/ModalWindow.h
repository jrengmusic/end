/**
 * @file ModalWindow.h
 * @brief Terminal-specific modal window wrapper over jam::ModalWindow.
 *
 * Terminal::ModalWindow is a thin wrapper that delegates the generic modal
 * setup sequence (centreAroundComponent, setGlass, setVisible, enterModalState)
 * to jam::ModalWindow's caller-ctor.
 *
 * This wrapper retains Terminal-specific concerns only:
 *  - Terminal::Display::onRepaintNeeded callback
 *  - paint() border drawn from Config colour/size keys
 *  - keyPressed() override
 *  - cornerSize platform constants
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
 * Adds Terminal-specific Display::onRepaintNeeded callback and paint border.
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
     * centreAroundComponent, setGlass, setVisible, and enterModalState.
     * Wires Terminal::Display::onRepaintNeeded if content is a Terminal::Display.
     *
     * @param content          Content component; ownership transferred.
     * @param centreAround     Component to centre the dialog on; L&F inherited.
     * @param dismissCallback  Called when the dialog is dismissed.
     * @note MESSAGE THREAD.
     */
    ModalWindow (std::unique_ptr<juce::Component> content,
                 juce::Component& centreAround,
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
