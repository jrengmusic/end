/**
 * @file ModalWindow.cpp
 * @brief Implementation of Terminal::ModalWindow — reusable glass modal.
 *
 * @see Terminal::ModalWindow
 * @see Terminal::Popup
 * @see Action::List
 */

#include "ModalWindow.h"
#include "../AppState.h"
#include "../Gpu.h"
#include "TerminalDisplay.h"

namespace Terminal
{ /*____________________________________________________________________________*/

//==============================================================================
// ContentView
//==============================================================================

ModalWindow::ContentView::ContentView (std::unique_ptr<juce::Component> content, jreng::GLRenderer& sharedRenderer)
    : ownedContent (std::move (content))
    , sharedSource (sharedRenderer)
{
    setOpaque (false);
    glContent = dynamic_cast<jreng::GLComponent*> (ownedContent.get());

    if (auto* terminal { dynamic_cast<Terminal::Display*> (ownedContent.get()) })
    {
        terminal->onRepaintNeeded = [this, terminal]
        {
            terminal->repaint();
            glRenderer.triggerRepaint();
        };
    }

    setSize (ownedContent->getWidth(), ownedContent->getHeight());
    addAndMakeVisible (ownedContent.get());
}

ModalWindow::ContentView::~ContentView() { glRenderer.detach(); }

void ModalWindow::ContentView::resized()
{
    jassert (ownedContent != nullptr);
    ownedContent->setBounds (getLocalBounds());
}

void ModalWindow::ContentView::initialiseGL()
{
    if (glContent != nullptr and AppState::getContext()->getRendererType() == App::RendererType::gpu)
    {
        glRenderer.setComponentIterator (
            [this] (std::function<void (jreng::GLComponent&)> renderComponent)
            {
                if (glContent != nullptr and glContent->isVisible())
                    renderComponent (*glContent);
            });
        glRenderer.setComponentPaintingEnabled (true);
        glRenderer.setSharedRenderer (sharedSource);
        glRenderer.attachTo (*this);
    }
}

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
                          jreng::GLRenderer& sharedRenderer,
                          std::function<void()> dismissCallback)
    : jreng::ModalWindow (
          [&content, &sharedRenderer]
          {
              const int w { content->getWidth() };
              const int h { content->getHeight() };
              auto view { new ContentView (std::move (content), sharedRenderer) };
              view->setSize (w, h);
              return view;
          }(),
          {},
          true,
          false)
{
    onModalDismissed = std::move (dismissCallback);

    setupWindow (centreAround);

    setVisible (true);

    if (auto* viewPtr { dynamic_cast<ContentView*> (getContentComponent()) })
        viewPtr->initialiseGL();

    enterModalState (true);

    juce::MessageManager::callAsync (
        [this]
        {
            if (auto* content { getContentComponent() })
            {
                if (auto* focusTarget { content->getChildComponent (0) })
                    focusTarget->grabKeyboardFocus();
            }
        });
}

ModalWindow::ModalWindow (std::unique_ptr<juce::Component> content,
                          juce::Component& centreAround,
                          std::function<void()> dismissCallback)
    : jreng::ModalWindow (
          [&content]
          {
              const int w { content->getWidth() };
              const int h { content->getHeight() };
              auto* view { content.release() };
              view->setSize (w, h);
              return view;
          }(),
          {},
          true,
          false)
{
    onModalDismissed = std::move (dismissCallback);
    setGpuRenderer (false);
    setupWindow (centreAround);
    setVisible (true);
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

