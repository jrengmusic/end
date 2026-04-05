/**
 * @file ActionRow.h
 * @brief Single row in the Action::List — search box (index 0) or action entry (index > 0).
 */

#pragma once

#include <JuceHeader.h>
#include "Action.h"

namespace Action
{ /*____________________________________________________________________________*/

/** @brief Type tag for name labels; used by LookAndFeel font dispatch. */
class NameLabel : public juce::Label {};

/** @brief Type tag for shortcut labels; used by LookAndFeel font dispatch. */
class ShortcutLabel : public juce::Label {};

/** @brief Distinguishes row variants in the command palette. */
enum class RowKind
{
    search,
    action,
    separator
};

/**
 * @class Row
 * @brief A single row in the command palette.
 *
 * Index 0 = search box. Index > 0 = action entry with name and shortcut labels.
 * Inherits ObjectID so its `selected` Value is wired into the ValueTree.
 * Self-managing: paints highlight and grabs focus based on its own Value.
 */
class Row : public juce::Component,
            public jreng::Value::ObjectID<Row>,
            private juce::Value::Listener
{
public:
    /** @brief Construct search row (index 0). */
    Row (int index, const juce::String& uuid);

    /** @brief Construct action row (index > 0). */
    Row (int index, const juce::String& uuid, const Registry::Entry& entry);

    /** @brief Construct separator row. */
    Row (int index, const juce::String& uuid, RowKind kind);

    ~Row() override;

    void resized() override;
    void paint (juce::Graphics& g) override;

    juce::Value& getValueObject() noexcept override;

    int     getIndex() const noexcept;
    bool    isSelected() const noexcept;
    bool    isSelectable() const noexcept;
    RowKind getKind() const noexcept;

    /** @brief Returns the search TextEditor for index 0, nullptr for action rows. */
    juce::TextEditor* getSearchBox() noexcept;

    juce::Colour  highlightColour;
    juce::String  actionConfigKey;
    std::function<bool()> run;

    /** @brief Access name label (index > 0 only). */
    NameLabel* getNameLabel() noexcept;

    /** @brief Access shortcut label (index > 0 only). */
    ShortcutLabel* getShortcutLabel() noexcept;

private:
    static constexpr int   shortcutWidthDivisor { 3 };
    static constexpr int   separatorHeight      { 1 };
    static constexpr float separatorAlpha       { 0.3f };

    int         rowIndex;
    RowKind     kind { RowKind::action };
    juce::Value selected;

    // Index 0 content.
    std::unique_ptr<juce::TextEditor> searchBox;

    // Index > 0 content.
    std::unique_ptr<NameLabel>     nameLabel;
    std::unique_ptr<ShortcutLabel> shortcutLabel;

    void valueChanged (juce::Value& value) override;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Row)
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Action
