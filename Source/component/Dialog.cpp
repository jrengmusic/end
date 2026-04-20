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
    noButton.getProperties().set  (jam::ID::font, jam::ID::name.toString());

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
    const juce::Font font { [this]
    {
        const auto* cfg { Config::getContext() };
        return juce::Font { juce::FontOptions()
                                .withName (cfg->getString (Config::Key::actionListNameFamily))
                                .withStyle (cfg->getString (Config::Key::actionListNameStyle))
                                .withPointHeight (cfg->getFloat (Config::Key::actionListNameSize)) };
    }() };

    const int lineH    { static_cast<int> (std::ceil (font.getHeight())) };
    const int buttonH  { lineH + verticalPadding * 2 };
    const int yesW     { static_cast<int> (std::ceil (juce::TextLayout::getStringWidth (font, "Yes")))
                         + buttonTextPadding * 2 };
    const int noW      { static_cast<int> (std::ceil (juce::TextLayout::getStringWidth (font, "No")))
                         + buttonTextPadding * 2 };

    auto bounds { getLocalBounds() };

    const int labelH { lineH + verticalPadding * 2 };
    messageLabel.setBounds (bounds.removeFromTop (labelH));

    const int totalButtonW { yesW + buttonGap + noW };
    const int startX       { (bounds.getWidth() - totalButtonW) / 2 };
    const int startY       { bounds.getY() + (bounds.getHeight() - buttonH) / 2 };

    yesButton.setBounds (startX,                    startY, yesW, buttonH);
    noButton.setBounds  (startX + yesW + buttonGap, startY, noW,  buttonH);
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
    const juce::Font font { juce::FontOptions()
                                .withName (config.getString (Config::Key::actionListNameFamily))
                                .withStyle (config.getString (Config::Key::actionListNameStyle))
                                .withPointHeight (config.getFloat (Config::Key::actionListNameSize)) };

    const int messageW    { static_cast<int> (std::ceil (juce::TextLayout::getStringWidth (font, messageLabel.getText()))) };
    const int yesW        { static_cast<int> (std::ceil (juce::TextLayout::getStringWidth (font, "Yes")))
                            + buttonTextPadding * 2 };
    const int noW         { static_cast<int> (std::ceil (juce::TextLayout::getStringWidth (font, "No")))
                            + buttonTextPadding * 2 };
    const int buttonRowW  { yesW + buttonGap + noW };

    return juce::jmax (messageW, buttonRowW) + horizontalPadding * 2;
}

int Dialog::getPreferredHeight() const noexcept
{
    const juce::Font font { juce::FontOptions()
                                .withName (config.getString (Config::Key::actionListNameFamily))
                                .withStyle (config.getString (Config::Key::actionListNameStyle))
                                .withPointHeight (config.getFloat (Config::Key::actionListNameSize)) };

    const int lineH   { static_cast<int> (std::ceil (font.getHeight())) };
    const int buttonH { lineH + verticalPadding * 2 };
    const int labelH  { lineH + verticalPadding * 2 };

    return labelH + buttonH + verticalPadding;
}

//==============================================================================
void Dialog::visibilityChanged()
{
    if (isVisible())
    {
        juce::Component::SafePointer<Dialog> safeThis { this };
        juce::MessageManager::callAsync ([safeThis]
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
        juce::MessageManager::callAsync ([safeThis]
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
} // namespace Terminal
