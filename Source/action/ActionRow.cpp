/**
 * @file ActionRow.cpp
 * @brief Single row in the Action::List — search box (index 0) or action entry (index > 0).
 */

#include "ActionRow.h"

namespace Action
{ /*____________________________________________________________________________*/

Row::Row (int index, const juce::String& uuid)
    : ObjectID<Row> (uuid)
    , rowIndex (index)
    , kind (RowKind::search)
{
    searchBox = std::make_unique<juce::TextEditor>();
    addAndMakeVisible (searchBox.get());
    selected.addListener (this);
}

Row::Row (int index, const juce::String& uuid, const Registry::Entry& entry)
    : ObjectID<Row> (uuid)
    , rowIndex (index)
    , kind (RowKind::action)
{
    nameLabel = std::make_unique<juce::Label>();
    nameLabel->getProperties().set (jam::ID::font, jam::ID::name.toString());
    nameLabel->setText (entry.name, juce::dontSendNotification);
    nameLabel->setInterceptsMouseClicks (false, false);
    addAndMakeVisible (nameLabel.get());

    shortcutLabel = std::make_unique<juce::Label>();
    shortcutLabel->getProperties().set (jam::ID::font, jam::ID::keyPress.toString());
    shortcutLabel->setText (Registry::shortcutToString (entry.shortcut),
                            juce::dontSendNotification);
    shortcutLabel->setJustificationType (juce::Justification::centredRight);
    shortcutLabel->setInterceptsMouseClicks (false, false);
    addAndMakeVisible (shortcutLabel.get());

    run = entry.execute;

    selected.addListener (this);
}

Row::Row (int index, const juce::String& uuid, RowKind rowKind)
    : ObjectID<Row> (uuid)
    , rowIndex (index)
    , kind (rowKind)
{
    selected.addListener (this);
}

Row::~Row()
{
    selected.removeListener (this);
}

//==============================================================================
void Row::valueChanged (juce::Value& value)
{
    if (value.refersToSameSourceAs (selected))
    {
        repaint();

        if (isSelected() and searchBox != nullptr)
            searchBox->grabKeyboardFocus();
    }
}

//==============================================================================
void Row::paint (juce::Graphics& g)
{
    if (kind == RowKind::separator)
    {
        g.setColour (highlightColour.withAlpha (separatorAlpha));
        g.fillRect (getLocalBounds().withHeight (separatorHeight)
                        .withY (getHeight() / 2));
    }
    else if (isSelected() and searchBox == nullptr)
    {
        g.setColour (highlightColour);
        g.fillRect (getLocalBounds());
    }
}

//==============================================================================
void Row::resized()
{
    if (searchBox != nullptr)
    {
        searchBox->setBounds (getLocalBounds());
    }
    else if (nameLabel != nullptr and shortcutLabel != nullptr)
    {
        auto bounds { getLocalBounds() };
        const int shortcutWidth { bounds.getWidth() / shortcutWidthDivisor };

        nameLabel->setBounds (bounds.removeFromLeft (bounds.getWidth() - shortcutWidth));
        shortcutLabel->setBounds (bounds);
    }
}

//==============================================================================
juce::Value& Row::getValueObject() noexcept
{
    return selected;
}

int Row::getIndex() const noexcept
{
    return rowIndex;
}

bool Row::isSelected() const noexcept
{
    return static_cast<bool> (selected.getValue());
}

bool Row::isSelectable() const noexcept
{
    return kind != RowKind::separator;
}

RowKind Row::getKind() const noexcept
{
    return kind;
}

juce::TextEditor* Row::getSearchBox() noexcept
{
    return searchBox.get();
}

juce::Label* Row::getNameLabel() noexcept
{
    return nameLabel.get();
}

juce::Label* Row::getShortcutLabel() noexcept
{
    return shortcutLabel.get();
}

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Action
