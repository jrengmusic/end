/**
 * @file ActionRow.cpp
 * @brief Single row in the Action::List — name label and shortcut label.
 */

#include "ActionRow.h"

namespace Action
{ /*____________________________________________________________________________*/

Row::Row (const Registry::Entry& entry, const juce::String& uuid)
{
    setComponentID (uuid);

    nameLabel.setText (entry.name, juce::dontSendNotification);
    nameLabel.setColour (juce::Label::textColourId,
                         Config::getContext()->getColour (Config::Key::actionListNameColour));
    nameLabel.setInterceptsMouseClicks (false, false);

    shortcutLabel.setText (Registry::shortcutToString (entry.shortcut),
                           juce::dontSendNotification);
    shortcutLabel.setColour (juce::Label::textColourId,
                             Config::getContext()->getColour (Config::Key::actionListShortcutColour));
    shortcutLabel.setComponentID ("shortcut");
    shortcutLabel.setJustificationType (juce::Justification::centredRight);
    shortcutLabel.setInterceptsMouseClicks (false, false);

    actionConfigKey = Registry::configKeyForAction (entry.id);
    run = entry.execute;

    addAndMakeVisible (nameLabel);
    addAndMakeVisible (shortcutLabel);
}

void Row::resized()
{
    auto bounds { getLocalBounds() };
    const int shortcutWidth { bounds.getWidth() / 3 };

    nameLabel.setBounds (bounds.removeFromLeft (bounds.getWidth() - shortcutWidth));
    shortcutLabel.setBounds (bounds);
}

void Row::mouseDoubleClick (const juce::MouseEvent& event)
{
    const auto localPoint { event.getPosition() };

    if (shortcutLabel.getBounds().contains (localPoint))
    {
        if (onShortcutClicked != nullptr)
            onShortcutClicked (this);
    }
    else
    {
        if (run != nullptr)
            run();

        if (onExecuted != nullptr)
            onExecuted (this);
    }
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Action
