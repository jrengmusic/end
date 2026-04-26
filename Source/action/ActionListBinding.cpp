/**
 * @file ActionListBinding.cpp
 * @brief Action::List — binding mode and ValueTree synchronization.
 */

#include "ActionList.h"

namespace Action
{ /*____________________________________________________________________________*/

//==============================================================================
void List::valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&)
{
    auto* registry { Action::Registry::getContext() };

    for (int i { 1 }; i < static_cast<int> (rows.size()); ++i)
    {
        if (rows.at (static_cast<std::size_t> (i))->actionConfigKey.isNotEmpty())
        {
            if (auto* label { rows.at (static_cast<std::size_t> (i))->getShortcutLabel() }; label != nullptr)
            {
                const auto currentShortcut { label->getText() };
                const auto engineShortcut { luaEngine.getShortcutString (rows.at (static_cast<std::size_t> (i))->actionConfigKey) };

                if (currentShortcut != engineShortcut)
                {
                    luaEngine.patchKey (rows.at (static_cast<std::size_t> (i))->actionConfigKey, currentShortcut);
                    luaEngine.load();

                    if (registry != nullptr)
                        luaEngine.buildKeyMap (*registry);

                    state.get().setProperty (bindingsDirtyId, true, nullptr);
                }
            }
        }
    }
}

//==============================================================================
int List::getBindingRowIndex() const { return static_cast<int> (state.get().getProperty (bindingRowIndexId)); }

void List::setBindingRowIndex (int index) { state.get().setProperty (bindingRowIndexId, index, nullptr); }

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
                    luaEngine.patchKey (rows.at (static_cast<std::size_t> (targetIndex))->actionConfigKey, shortcutString);
                    luaEngine.load();
                    luaEngine.buildKeyMap (*registry);
                    state.get().setProperty (bindingsDirtyId, true, nullptr);

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
} // namespace Action
