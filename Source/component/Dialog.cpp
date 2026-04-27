/**
 * @file Dialog.cpp
 * @brief Implementation of the Terminal::Dialog confirmation modal body.
 *
 * @see Terminal::Dialog
 * @see Terminal::Popup
 */

#include "Dialog.h"

namespace Terminal
{ /*____________________________________________________________________________*/

//==============================================================================
// Dialog
//==============================================================================

Dialog::Dialog (const juce::String& message)
{
    messageLabel.setText (message, juce::dontSendNotification);
    messageLabel.setJustificationType (juce::Justification::centred);
    messageLabel.getProperties().set (jam::ID::font, jam::ID::name.toString());
    addAndMakeVisible (messageLabel);

    yesButton.getProperties().set (jam::ID::font, jam::ID::name.toString());
    noButton.getProperties().set (jam::ID::font, jam::ID::name.toString());

    yesButton.onClick = [this]
    {
        if (onYes != nullptr)
            onYes();
    };

    noButton.onClick = [this]
    {
        if (onNo != nullptr)
            onNo();
    };

    addAndMakeVisible (yesButton);
    addAndMakeVisible (noButton);

    setWantsKeyboardFocus (true);
}

Dialog::~Dialog() = default;

//==============================================================================
void Dialog::paint (juce::Graphics& /*g*/)
{
    // Transparent — Popup / ModalWindow supplies the glass blur background.
}

//==============================================================================
void Dialog::resized()
{
    auto area { getLocalBounds().reduced (2 * padding) };
    messageLabel.setBounds (area.removeFromTop (textHeight));
    area.removeFromTop (2 * padding);

    area = area.reduced (padding, 0);
    int buttonWidth { (area.getWidth() / 2) - padding };
    yesButton.setBounds (area.removeFromLeft (buttonWidth));
    noButton.setBounds (area.removeFromRight (buttonWidth));
}

//==============================================================================
bool Dialog::keyPressed (const juce::KeyPress& key)
{
    bool consumed { false };

    if (key.getKeyCode() == 'Y' or key.getKeyCode() == 'y')
    {
        if (onYes != nullptr)
            onYes();

        consumed = true;
    }
    else if (key.getKeyCode() == 'N' or key.getKeyCode() == 'n')
    {
        if (onNo != nullptr)
            onNo();

        consumed = true;
    }

    return consumed;
}

//==============================================================================
int Dialog::getPreferredWidth() const noexcept
{
    int width { 4 * padding };
    int messageWidth { jam::toInt (juce::TextLayout::getStringWidth (font, messageLabel.getText())) };

    width += messageWidth;

    return width;
}

int Dialog::getPreferredHeight() const noexcept
{
    int height { 6 * padding };
    height += textHeight;
    height += buttonHeight;

    return height;
}

//==============================================================================
void Dialog::visibilityChanged()
{
    if (isVisible())
    {
        juce::Component::SafePointer<Dialog> safeThis { this };
        juce::MessageManager::callAsync (
            [safeThis]
            {
                if (safeThis != nullptr)
                    safeThis->grabKeyboardFocus();
            });
    }
}

//==============================================================================
void Dialog::parentHierarchyChanged()
{
    if (isShowing())
    {
        juce::Component::SafePointer<Dialog> safeThis { this };
        juce::MessageManager::callAsync (
            [safeThis]
            {
                if (safeThis != nullptr)
                    safeThis->grabKeyboardFocus();
            });
    }
}

//==============================================================================
void Dialog::lookAndFeelChanged()
{
    auto& laf { getLookAndFeel() };

    messageLabel.setLookAndFeel (&laf);
    yesButton.setLookAndFeel (&laf);
    noButton.setLookAndFeel (&laf);
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Terminal
