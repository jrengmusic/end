/**
 * @file ModalWindow.cpp
 * @brief Implementation of Terminal::ModalWindow — reusable glass modal.
 *
 * @see Terminal::ModalWindow
 * @see Terminal::Popup
 * @see Action::List
 */

#include "ModalWindow.h"
#include "TerminalDisplay.h"

namespace Terminal
{ /*____________________________________________________________________________*/

//==============================================================================
// ModalWindow
//==============================================================================

void ModalWindow::setupWindow (juce::Component& centreAround)
{
    if (auto* content { getContentComponent() })
        content->setLookAndFeel (&centreAround.getLookAndFeel());

    setResizable (false, false);
    centreAroundComponent (&centreAround, getWidth(), getHeight());

    setGlass (config.getColour (Config::Key::windowColour)
                  .withAlpha (config.getFloat (Config::Key::windowOpacity)),
              config.getFloat (Config::Key::windowBlurRadius));
}

ModalWindow::ModalWindow (std::unique_ptr<juce::Component> content,
                          juce::Component& centreAround,
                          std::unique_ptr<jreng::GLRenderer> renderer,
                          void* nativeSharedContext,
                          std::function<void()> dismissCallback)
    : jreng::ModalWindow (
          [&content]
          {
              const int w { content->getWidth() };
              const int h { content->getHeight() };
              auto* raw { content.release() };
              raw->setSize (w, h);
              return raw;
          }(),
          {},
          true,
          false)
{
    onModalDismissed = std::move (dismissCallback);

    if (renderer != nullptr)
    {
        if (auto* glContent { dynamic_cast<jreng::GLComponent*> (getContentComponent()) })
        {
            renderer->setComponentIterator (
                [glContent] (std::function<void (jreng::GLComponent&)> renderComponent)
                {
                    if (glContent->isVisible())
                        renderComponent (*glContent);
                });
        }
    }

    if (nativeSharedContext != nullptr)
        setNativeSharedContext (nativeSharedContext);

    setRenderer (std::move (renderer));

    setupWindow (centreAround);

    setVisible (true);

    if (auto* terminal { dynamic_cast<Terminal::Display*> (getContentComponent()) })
    {
        terminal->onRepaintNeeded = [this, terminal]
        {
            terminal->repaint();
            triggerRepaint();
        };
    }

    enterModalState (true);
}

void ModalWindow::paint (juce::Graphics& g)
{
    const auto borderWidth { config.getFloat (Config::Key::popupBorderWidth) };

    g.setColour (config.getColour (Config::Key::popupBorderColour));

    g.drawRoundedRectangle (getLocalBounds().reduced (2 * borderWidth).toFloat(), cornerSize, borderWidth);
}

bool ModalWindow::keyPressed (const juce::KeyPress&) { return false; }

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Terminal
