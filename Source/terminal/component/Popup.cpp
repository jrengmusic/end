/**
 * @file Popup.cpp
 * @brief Implementation of the terminal::Popup modal glass dialog.
 *
 * @see terminal::Popup
 * @see terminal::ModalWindow
 * @see Config
 */

#include "Popup.h"

namespace terminal
{
/*____________________________________________________________________________*/

//==============================================================================
// Popup
//==============================================================================

Popup::~Popup() { dismiss(); }

void Popup::show (juce::Component& caller,
                  std::unique_ptr<juce::Component> content,
                  int width,
                  int height)
{
    dismiss();

    content->setSize (width, height);

    if (auto* terminal { dynamic_cast<terminal::Display*> (content.get()) })
    {
        popupSessionUuid = terminal->getComponentID();
        watchedStateRoot = terminal->getProcessor().getState().getRootTree();
        watchedStateRoot.addListener (this);
    }

    window = std::make_unique<terminal::ModalWindow> (std::move (content),
                                                      caller,
                                                      [this]
                                                      {
                                                          window.reset();
                                                          removePopupSession();

                                                          if (onDismiss != nullptr)
                                                              onDismiss();
                                                      });
}

void Popup::setTerminalSession (std::unique_ptr<terminal::Session> session)
{
    terminalSession = std::move (session);
}

void Popup::removePopupSession()
{
    if (watchedStateRoot.isValid())
    {
        watchedStateRoot.removeListener (this);
        watchedStateRoot = {};
    }

    terminalSession.reset();
    popupSessionUuid = {};
}

void Popup::valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property)
{
    if (property == terminal::id::value
        and tree.getType() == jam::Model::PARAM
        and tree.getProperty (terminal::id::id).toString() == terminal::id::shellExited.toString()
        and static_cast<int> (tree.getProperty (terminal::id::value)) == 1)
    {
        juce::MessageManager::callAsync ([this]
        {
            dismiss();
        });
    }
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
} // namespace terminal
