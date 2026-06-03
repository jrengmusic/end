/**
 * @file ActionListBinding.cpp
 * @brief action::List — binding mode and ValueTree synchronization.
 */

#include "ActionList.h"

namespace action
{
/*____________________________________________________________________________*/

//==============================================================================
void List::valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&)
{
    auto* registry { action::Registry::getContext() };

    for (int i { 1 }; i < static_cast<int> (rows.size()); ++i)
    {
        if (rows.at (static_cast<std::size_t> (i))->actionConfigKey.isNotEmpty())
        {
            if (auto* label { rows.at (static_cast<std::size_t> (i))->getShortcutLabel() }; label != nullptr)
            {
                const auto currentShortcut { label->getText() };
                const auto engineShortcut { AppModel::getContext()->getShortcutString (rows.at (static_cast<std::size_t> (i))->actionConfigKey) };

                if (currentShortcut != engineShortcut)
                {
                    AppModel::getContext()->overrideShortcut (rows.at (static_cast<std::size_t> (i))->actionConfigKey, currentShortcut);
                    AppModel::getContext()->reload();

                    if (registry != nullptr)
                        AppModel::getContext()->buildKeyMap (*registry);

                    state.setTreeProperty (bindingsDirtyId, true, nullptr);
                }
            }
        }
    }
}

//==============================================================================
int List::getBindingRowIndex() const { return static_cast<int> (state.getTreeProperty (bindingRowIndexId)); }

void List::setBindingRowIndex (int index) { state.setTreeProperty (bindingRowIndexId, index, nullptr); }

//==============================================================================
void List::enterBindingMode()
{
    const int selected { getSelectedIndex() };

    if (selected > 0)
    {
        setBindingRowIndex (selected);
        messageOverlay.showMessage ("Type key to remap", bindingModeTimeoutMs);
    }
}

void List::exitBindingMode()
{
    setBindingRowIndex (-1);
    jam::Animator::toggleFade (&messageOverlay, false);
    grabKeyboardFocus();
}

bool List::handleBindingKey (const juce::KeyPress& key)
{
    bool handled { false };

    if (getBindingRowIndex() >= 1)
    {
        if (key.getKeyCode() == 'C' and key.getModifiers().isCtrlDown())
        {
            exitBindingMode();
            handled = true;
        }
        else
        {
            const int targetIndex { getBindingRowIndex() };

            if (targetIndex < static_cast<int> (rows.size()))
            {
                auto* registry { Registry::getContext() };
                const auto shortcutString { Registry::shortcutToString (key) };

                if (rows.at (static_cast<std::size_t> (targetIndex))->actionConfigKey.isNotEmpty())
                {
                    AppModel::getContext()->overrideShortcut (rows.at (static_cast<std::size_t> (targetIndex))->actionConfigKey, shortcutString);
                    AppModel::getContext()->reload();
                    AppModel::getContext()->buildKeyMap (*registry);
                    state.setTreeProperty (bindingsDirtyId, true, nullptr);

                    if (auto* label { rows.at (static_cast<std::size_t> (targetIndex))->getShortcutLabel() };
                        label != nullptr)
                        label->setText (shortcutString, juce::dontSendNotification);
                }
            }

            exitBindingMode();
            handled = true;
        }
    }

    return handled;
}

/**______________________________END OF NAMESPACE______________________________*/
} // namespace action
