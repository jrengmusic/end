/**
 * @file Popup.cpp
 * @brief Implementation of the Terminal::Popup modal glass dialog.
 *
 * @see Terminal::Popup
 * @see Terminal::ModalWindow
 * @see Config
 */

#include "Popup.h"
#include "TerminalDisplay.h"

namespace Terminal
{ /*____________________________________________________________________________*/

//==============================================================================
// Popup
//==============================================================================

Popup::~Popup() { dismiss(); }

void Popup::show (juce::Component& caller,
                  std::unique_ptr<juce::Component> content,
                  int width,
                  int height,
                  std::unique_ptr<jam::gl::Renderer> renderer)
{
    dismiss();

    content->setSize (width, height);

    if (auto* terminal { dynamic_cast<Terminal::Display*> (content.get()) })
    {
        popupSessionUuid = terminal->getComponentID();
        terminal->onProcessExited = [this]
        {
            dismiss();
        };
    }

    void* sharedContext { nullptr };

    if (auto* rootWindow { dynamic_cast<jam::Window*> (caller.getTopLevelComponent()) })
        sharedContext = rootWindow->getNativeSharedContext();

    window = std::make_unique<Terminal::ModalWindow> (std::move (content),
                                                      caller,
                                                      std::move (renderer),
                                                      sharedContext,
                                                      [this]
                                                      {
                                                          window.reset();
                                                          removePopupSession();

                                                          if (onDismiss != nullptr)
                                                              onDismiss();
                                                      });
}

void Popup::setTerminalSession (std::unique_ptr<Terminal::Session> session)
{
    terminalSession = std::move (session);
}

void Popup::removePopupSession()
{
    terminalSession.reset();
    popupSessionUuid = {};
}

void Popup::dismiss()
{
    if (window != nullptr)
    {
        window->exitModalState (0);
        window.reset();
        removePopupSession();
    }
}

bool Popup::isActive() const noexcept { return window != nullptr; }

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Terminal
