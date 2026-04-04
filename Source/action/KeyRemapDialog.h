/**
 * @file KeyRemapDialog.h
 * @brief Inline key remap overlay for the Action::List command palette.
 */

#pragma once

#include <JuceHeader.h>

namespace Action
{ /*____________________________________________________________________________*/

/**
 * @class KeyRemapDialog
 * @brief Inline overlay for capturing and editing a shortcut binding.
 *
 * Added as a child component of Action::List's content area.  Not a separate
 * window — no nested modal state.  Pre-fills with the current shortcut and
 * modal flag.  Learn mode captures the next keypress into the text field.
 */
class KeyRemapDialog : public juce::Component
{
public:
    KeyRemapDialog();

    void show (const juce::String& currentShortcut,
               bool isModal,
               const juce::String& actionName);

    void resized() override;

    /** @brief Fires on OK with (shortcutString, isModal). */
    std::function<void (const juce::String&, bool)> onCommit;

    /** @brief Fires on Cancel. */
    std::function<void()> onCancel;

    /** @brief Called by the parent's keyPressed when learn mode is active. */
    bool handleLearnKey (const juce::KeyPress& key);

    /** @brief True when waiting for a keypress in learn mode. */
    bool isLearning() const noexcept;

private:
    juce::Label        titleLabel;
    juce::TextEditor   shortcutField;
    juce::TextButton   learnButton { "L" };
    juce::ToggleButton modalToggle { "Modal (prefix + key)" };
    juce::TextButton   okButton { "OK" };
    juce::TextButton   cancelButton { "Cancel" };

    bool learning { false };

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KeyRemapDialog)
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Action
