/**
 * @file Popup.cpp
 * @brief Implementation of the Terminal::Popup modal glass dialog.
 *
 * @see Terminal::Popup
 * @see Config
 * @see jreng::BackgroundBlur
 * @see jreng::GLRenderer
 */

#include "Popup.h"
#include "../AppState.h"
#include "../Gpu.h"
#include "TerminalComponent.h"

namespace Terminal
{ /*____________________________________________________________________________*/

//==============================================================================
// ContentView
//==============================================================================

Popup::ContentView::ContentView (std::unique_ptr<juce::Component> content, jreng::GLRenderer& sharedRenderer)
    : ownedContent (std::move (content))
    , sharedSource (sharedRenderer)
{
    setOpaque (false);
    glContent = dynamic_cast<jreng::GLComponent*> (ownedContent.get());

    if (auto* terminal { dynamic_cast<Terminal::Component*> (ownedContent.get()) })
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

Popup::ContentView::~ContentView() { glRenderer.detach(); }

void Popup::ContentView::resized()
{
    if (ownedContent != nullptr)
        ownedContent->setBounds (getLocalBounds());
}

void Popup::ContentView::initialiseGL()
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
// Window
//==============================================================================

Popup::Window::Window (std::unique_ptr<juce::Component> content,
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

    if (auto* viewPtr { dynamic_cast<ContentView*> (getContentComponent()) })
        viewPtr->setLookAndFeel (&centreAround.getLookAndFeel());

    setResizable (false, false);
    centreAroundComponent (&centreAround, getWidth(), getHeight());

    setGlass (config.getColour (Config::Key::windowColour),
              Gpu::resolveOpacity (config.getFloat (Config::Key::windowOpacity)),
              config.getFloat (Config::Key::windowBlurRadius));

    setVisible (true);

    if (auto* viewPtr { dynamic_cast<ContentView*> (getContentComponent()) })
        viewPtr->initialiseGL();

    enterModalState (true);

    juce::MessageManager::callAsync (
        [this]
        {
            if (auto* viewPtr { dynamic_cast<ContentView*> (getContentComponent()) })
            {
                if (auto* focusTarget { viewPtr->getChildComponent (0) })
                    focusTarget->grabKeyboardFocus();
            }
        });
}

void Popup::Window::paint (juce::Graphics& g)
{
    const auto borderWidth { config.getFloat (Config::Key::popupBorderWidth) };

    g.setColour (config.getColour (Config::Key::popupBorderColour));

    g.drawRoundedRectangle (getLocalBounds().reduced (2 * borderWidth).toFloat(), Window::cornerSize, borderWidth);
}

bool Popup::Window::keyPressed (const juce::KeyPress&) { return false; }

//==============================================================================
// Popup
//==============================================================================

Popup::~Popup() { dismiss(); }

void Popup::show (juce::Component& caller,
                  std::unique_ptr<juce::Component> content,
                  int width,
                  int height,
                  jreng::GLRenderer& sharedRenderer)
{
    dismiss();

    content->setSize (width, height);

    if (auto* terminal { dynamic_cast<Terminal::Component*> (content.get()) })
        terminal->onProcessExited = [this]
        {
            dismiss();
        };

    window = std::make_unique<Window> (std::move (content),
                                       caller,
                                       sharedRenderer,
                                       [this]
                                       {
                                           window.reset();

                                           if (onDismiss != nullptr)
                                               onDismiss();
                                       });
}

void Popup::dismiss()
{
    if (window != nullptr)
    {
        window->exitModalState (0);
        window.reset();
    }
}

bool Popup::isActive() const noexcept { return window != nullptr; }

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Terminal

