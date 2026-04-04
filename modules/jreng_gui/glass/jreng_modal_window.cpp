/**
 * @file jreng_modal_window.cpp
 * @brief Implementation of ModalWindow — modal GlassWindow.
 *
 * @see jreng_modal_window.h
 * @see GlassWindow
 */

namespace jreng
{
/*____________________________________________________________________________*/

ModalWindow::ModalWindow (juce::Component* mainComponent,
                          const juce::String& name,
                          bool alwaysOnTop,
                          bool showWindowButtons)
    : GlassWindow (mainComponent, name, alwaysOnTop, showWindowButtons)
{
}

void ModalWindow::closeButtonPressed ()
{
    exitModalState (0);

    if (onModalDismissed != nullptr)
        onModalDismissed();
}

bool ModalWindow::keyPressed (const juce::KeyPress& key)
{
    bool handled { false };

    if (key == juce::KeyPress::escapeKey)
    {
        closeButtonPressed();
        handled = true;
    }

    return handled;
}

void ModalWindow::inputAttemptWhenModal ()
{
    toFront (true);
}

/**_____________________________END_OF_NAMESPACE______________________________*/
} /** namespace jreng */
