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
#include <jam_tui/jam_tui.h>

namespace Terminal
{ /*____________________________________________________________________________*/

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

    if (auto* terminal { dynamic_cast<Terminal::Display*> (content.get()) })
    {
        popupSessionUuid = terminal->getComponentID();
        watchedStateRoot = terminal->getValueTree();
        watchedStateRoot.addListener (this);
    }

    window = std::make_unique<Terminal::ModalWindow> (std::move (content),
                                                      caller,
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
    if (property == Terminal::ID::value
        and tree.getType() == jam::ValueTree::PARAM
        and tree.getProperty (Terminal::ID::id).toString() == Terminal::ID::shellExited.toString()
        and static_cast<int> (tree.getProperty (Terminal::ID::value)) == 1)
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
} // namespace Terminal
