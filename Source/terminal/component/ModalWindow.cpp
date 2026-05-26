/**
 * @file ModalWindow.cpp
 * @brief terminal::ModalWindow — thin wrapper over jam::ModalWindow that adds
 *        border paint, keyPressed override, cornerSize constants, and
 *        terminal::Display::onRepaintNeeded wiring.  No renderer or shared context.
 *
 * @see jam::ModalWindow
 */

#include "ModalWindow.h"

namespace terminal
{
/*____________________________________________________________________________*/

//==============================================================================
// ModalWindow
//==============================================================================

ModalWindow::ModalWindow (std::unique_ptr<juce::Component> content,
                          juce::Component& centreAround,
                          std::function<void()> dismissCallback)
    : jam::ModalWindow (std::move (content),
                        centreAround,
                        std::move (dismissCallback),
                        lua::Engine::getContext()->display.window.opacity,
                        lua::Engine::getContext()->display.window.blurRadius)
{
    // No onRepaintNeeded wiring needed — terminal repaints via JUCE's normal
    // component repaint mechanism.
}

void ModalWindow::paint (juce::Graphics& g)
{
    const auto* cfg { lua::Engine::getContext() };
    const auto borderWidth { cfg->display.popup.borderWidth };

    g.setColour (cfg->display.popup.borderColour);
    g.drawRoundedRectangle (getLocalBounds().reduced (2 * borderWidth).toFloat(), cornerSize, borderWidth);
}

bool ModalWindow::keyPressed (const juce::KeyPress&) { return false; }

/**______________________________END OF NAMESPACE______________________________*/
} // namespace terminal
