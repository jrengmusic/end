/**
 * @file ModalWindow.cpp
 * @brief Terminal::ModalWindow — thin wrapper over jam::ModalWindow that adds
 *        border paint, keyPressed override, cornerSize constants, and
 *        Terminal::Display::onRepaintNeeded wiring.
 *
 * @see jam::ModalWindow
 */

#include "ModalWindow.h"
#include "TerminalDisplay.h"

namespace Terminal
{ /*____________________________________________________________________________*/

//==============================================================================
// ModalWindow
//==============================================================================

ModalWindow::ModalWindow (std::unique_ptr<juce::Component> content,
                          juce::Component& centreAround,
                          std::unique_ptr<jam::gl::Renderer> renderer,
                          void* nativeSharedContext,
                          std::function<void()> dismissCallback)
    : jam::ModalWindow (std::move (content),
                        centreAround,
                        std::move (renderer),
                        nativeSharedContext,
                        std::move (dismissCallback),
                        lua::Engine::getContext()->display.window.opacity,
                        lua::Engine::getContext()->display.window.blurRadius)
{
    if (auto* terminal { dynamic_cast<Terminal::Display*> (getContentComponent()) })
    {
        terminal->onRepaintNeeded = [this, terminal]
        {
            terminal->repaint();
            triggerRepaint();
        };
    }
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
} // namespace Terminal
