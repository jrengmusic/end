/**
 * @file KeyHandler.cpp
 * @brief Keyboard dispatch for Action::List — unified navigation.
 */

#include "KeyHandler.h"

namespace Action
{ /*____________________________________________________________________________*/

KeyHandler::KeyHandler (Callbacks cbs)
    : callbacks (std::move (cbs))
{
    buildSearchTable();
    buildNavigationTable();
}

//==============================================================================
void KeyHandler::buildSearchTable()
{
    searchTable[juce::KeyPress::escapeKey] = [this]
    {
        if (callbacks.visibleRowCount() > 0)
            callbacks.selectRow (1);
        else
            callbacks.dismiss();
    };

    searchTable[juce::KeyPress::returnKey] = [this]
    {
        callbacks.executeSelected();
    };
}

//==============================================================================
void KeyHandler::buildNavigationTable()
{
    navigationTable['J'] = [this]
    {
        callbacks.selectRow (callbacks.selectedIndex() + 1);
    };

    navigationTable['K'] = [this]
    {
        callbacks.selectRow (callbacks.selectedIndex() - 1);
    };

    navigationTable['I'] = [this]
    {
        callbacks.selectRow (0);
    };

    navigationTable[juce::KeyPress::downKey] = [this]
    {
        callbacks.selectRow (callbacks.selectedIndex() + 1);
    };

    navigationTable[juce::KeyPress::upKey] = [this]
    {
        callbacks.selectRow (callbacks.selectedIndex() - 1);
    };

    navigationTable[juce::KeyPress::escapeKey] = [this]
    {
        callbacks.dismiss();
    };

    navigationTable[juce::KeyPress::returnKey] = [this]
    {
        callbacks.executeSelected();
    };
}

//==============================================================================
bool KeyHandler::handleKey (const juce::KeyPress& key)
{
    bool handled { false };

    // Shift+Return = binding mode (only from navigation, not search).
    if (callbacks.selectedIndex() > 0
        and key.getKeyCode() == juce::KeyPress::returnKey
        and key.getModifiers().isShiftDown())
    {
        callbacks.enterBindingMode();
        handled = true;
    }
    else
    {
        const auto& table { callbacks.selectedIndex() == 0 ? searchTable : navigationTable };
        const auto it { table.find (key.getKeyCode()) };

        handled = it != table.end();

        if (handled)
            it->second();
    }

    return handled;
}

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Action
