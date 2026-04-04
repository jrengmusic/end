/**
 * @file ActionRow.h
 * @brief Single row in the Action::List — name label and shortcut label.
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

/**
 * @class Row
 * @brief A single row in the command palette action list.
 *
 * Flat component with NameLabel and ShortcutLabel as direct children.
 * Required for jreng::ValueTree::attach single-level walk.
 */
class Row : public juce::Component
{
public:
    Row (const Registry::Entry& entry, const juce::String& uuid);

    void resized() override;
    void mouseDoubleClick (const juce::MouseEvent& event) override;

    NameLabel     nameLabel;
    ShortcutLabel shortcutLabel;
    juce::String  actionConfigKey;
    std::function<bool()>    run;
    std::function<void(Row*)> onExecuted;
    std::function<void(Row*)> onShortcutClicked;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Row)
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Action
