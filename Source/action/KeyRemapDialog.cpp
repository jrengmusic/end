/**
 * @file KeyRemapDialog.cpp
 * @brief Inline key remap overlay for the Action::List command palette.
 */

#include "KeyRemapDialog.h"
#include "Action.h"
#include "../config/Config.h"

namespace Action
{ /*____________________________________________________________________________*/

KeyRemapDialog::KeyRemapDialog()
{
    auto* cfg { Config::getContext() };

    addAndMakeVisible (titleLabel);
    titleLabel.setJustificationType (juce::Justification::centredLeft);
    titleLabel.setColour (juce::Label::textColourId,
                          cfg->getColour (Config::Key::coloursForeground));

    addAndMakeVisible (shortcutField);
    shortcutField.setMultiLine (false);
    shortcutField.setReturnKeyStartsNewLine (false);
    shortcutField.setScrollbarsShown (false);
    shortcutField.setPopupMenuEnabled (false);

    shortcutField.setColour (juce::TextEditor::backgroundColourId,
        cfg->getColour (Config::Key::windowColour)
            .withAlpha (cfg->getFloat (Config::Key::windowOpacity)));
    shortcutField.setColour (juce::TextEditor::textColourId,
        cfg->getColour (Config::Key::coloursForeground));
    shortcutField.setColour (juce::TextEditor::outlineColourId,
        cfg->getColour (Config::Key::coloursCursor));
    shortcutField.setColour (juce::TextEditor::focusedOutlineColourId,
        cfg->getColour (Config::Key::coloursCursor));

    shortcutField.setFont (juce::Font (juce::FontOptions()
        .withName (cfg->getString (Config::Key::fontFamily))
        .withPointHeight (cfg->getFloat (Config::Key::fontSize))));

    addAndMakeVisible (learnButton);
    learnButton.onClick = [this]
    {
        learning = true;
        learnButton.setButtonText ("...");
    };

    addAndMakeVisible (modalToggle);
    modalToggle.setColour (juce::ToggleButton::textColourId,
                           cfg->getColour (Config::Key::coloursForeground));
    modalToggle.setColour (juce::ToggleButton::tickColourId,
                           cfg->getColour (Config::Key::coloursCursor));

    addAndMakeVisible (okButton);
    okButton.onClick = [this]
    {
        if (onCommit != nullptr)
            onCommit (shortcutField.getText(), modalToggle.getToggleState());
    };

    addAndMakeVisible (cancelButton);
    cancelButton.onClick = [this]
    {
        if (onCancel != nullptr)
            onCancel();
    };

    setVisible (false);
}

void KeyRemapDialog::show (const juce::String& currentShortcut,
                           bool isModal,
                           const juce::String& actionName)
{
    titleLabel.setText ("Remap: " + actionName, juce::dontSendNotification);
    shortcutField.setText (currentShortcut, juce::dontSendNotification);
    modalToggle.setToggleState (isModal, juce::dontSendNotification);
    learning = false;
    learnButton.setButtonText ("L");
    setVisible (true);
    shortcutField.grabKeyboardFocus();
}

void KeyRemapDialog::resized()
{
    auto bounds { getLocalBounds().reduced (10) };

    titleLabel.setBounds (bounds.removeFromTop (24));
    bounds.removeFromTop (6);

    auto shortcutRow { bounds.removeFromTop (28) };
    learnButton.setBounds (shortcutRow.removeFromRight (30));
    shortcutRow.removeFromRight (4);
    shortcutField.setBounds (shortcutRow);

    bounds.removeFromTop (6);
    modalToggle.setBounds (bounds.removeFromTop (24));

    bounds.removeFromTop (6);
    auto buttonRow { bounds.removeFromTop (28) };
    cancelButton.setBounds (buttonRow.removeFromRight (80));
    buttonRow.removeFromRight (6);
    okButton.setBounds (buttonRow.removeFromRight (80));
}

bool KeyRemapDialog::handleLearnKey (const juce::KeyPress& key)
{
    bool handled { false };

    if (learning and key.isValid())
    {
        shortcutField.setText (Registry::shortcutToString (key),
                               juce::dontSendNotification);
        learning = false;
        learnButton.setButtonText ("L");
        shortcutField.grabKeyboardFocus();
        handled = true;
    }

    return handled;
}

bool KeyRemapDialog::isLearning() const noexcept
{
    return learning;
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Action
