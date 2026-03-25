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
#include "RendererType.h"
#include "TerminalComponent.h"

namespace Terminal
{ /*____________________________________________________________________________*/

//==============================================================================
// ContentView
//==============================================================================

Popup::ContentView::ContentView (std::unique_ptr<juce::Component> content)
    : ownedContent (std::move (content))
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

Popup::ContentView::~ContentView()
{
    glRenderer.detach();
}

void Popup::ContentView::resized()
{
    if (ownedContent != nullptr)
        ownedContent->setBounds (getLocalBounds());
}

void Popup::ContentView::initialiseGL()
{
    if (glContent != nullptr and Terminal::getRendererType() == Terminal::RendererType::gpu)
    {
        glRenderer.setComponentIterator (
            [this] (std::function<void (jreng::GLComponent&)> renderComponent)
            {
                if (glContent != nullptr and glContent->isVisible())
                    renderComponent (*glContent);
            });
        glRenderer.setComponentPaintingEnabled (true);
        glRenderer.attachTo (*this);
    }
}

//==============================================================================
// Window
//==============================================================================

Popup::Window::Window (std::unique_ptr<juce::Component> content,
                       juce::Component& centreAround,
                       std::function<void()> dismissCallback)
    : juce::DialogWindow ({}, Config::getContext()->getColour (Config::Key::windowColour), false)
    , onDismissed (std::move (dismissCallback))
{
    setOpaque (false);
    setUsingNativeTitleBar (false);
    setTitleBarHeight (0);

    const int desiredWidth  { content->getWidth() };
    const int desiredHeight { content->getHeight() };

    auto contentView { std::make_unique<ContentView> (std::move (content)) };
    auto* viewPtr { contentView.get() };

    setContentOwned (contentView.release(), false);
    setSize (desiredWidth, desiredHeight);
    setAlwaysOnTop (true);
    centreAroundComponent (&centreAround, desiredWidth, desiredHeight);
    setResizable (false, false);
    setVisible (true);

    viewPtr->initialiseGL();

    enterModalState (true);

    if (auto* focusTarget { viewPtr->getChildComponent (0) })
        focusTarget->grabKeyboardFocus();
}

void Popup::Window::paint (juce::Graphics& g)
{
    const auto borderWidth { config.getFloat (Config::Key::popupBorderWidth) };

    if (borderWidth > 0.0f)
    {
        g.setColour (config.getColour (Config::Key::popupBorderColour));
        g.drawRect (getLocalBounds().toFloat(), borderWidth);
    }
}

void Popup::Window::closeButtonPressed()
{
    exitModalState (0);

    if (onDismissed != nullptr)
        onDismissed();
}

void Popup::Window::inputAttemptWhenModal()
{
    toFront (true);
}

void Popup::Window::visibilityChanged()
{
    if (isVisible() and not blurApplied)
        triggerAsyncUpdate();
}

void Popup::Window::handleAsyncUpdate()
{
    const auto tint { config.getColour (Config::Key::windowColour)
                           .withAlpha (config.getFloat (Config::Key::windowOpacity)) };
    const float blur { config.getFloat (Config::Key::windowBlurRadius) };

    blurApplied = jreng::BackgroundBlur::apply (this, blur, tint);
}

//==============================================================================
// Popup
//==============================================================================

Popup::~Popup()
{
    dismiss();
}

void Popup::show (juce::Component& caller, std::unique_ptr<juce::Component> content)
{
    dismiss();

    const int contentWidth  { static_cast<int> (static_cast<float> (caller.getWidth())
                                                * config.getFloat (Config::Key::popupWidth)) };
    const int contentHeight { static_cast<int> (static_cast<float> (caller.getHeight())
                                                * config.getFloat (Config::Key::popupHeight)) };

    content->setSize (contentWidth, contentHeight);

    if (auto* terminal { dynamic_cast<Terminal::Component*> (content.get()) })
        terminal->onProcessExited = [this] { dismiss(); };

    window = std::make_unique<Window> (std::move (content), caller,
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

bool Popup::isActive() const noexcept
{
    return window != nullptr;
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Terminal
