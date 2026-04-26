/**
 * @file TabsClose.cpp
 * @brief Terminal::Tabs implementation — tab and session close operations.
 *
 * @see Tabs.h
 */

#include "Tabs.h"
#include "../AppState.h"
#include "../terminal/data/Identifier.h"
#include "../nexus/Nexus.h"

namespace Terminal
{ /*____________________________________________________________________________*/

/**
 * @brief Closes the active tab and destroys its Panes instance.
 *
 * If this is the last tab, does nothing — the caller (MainComponent)
 * handles application quit when no tabs remain after this call.
 *
 * @note MESSAGE THREAD.
 */
void Tabs::closeActiveTab()
{
    const int index { getCurrentTabIndex() };

    if (index >= 0 and index < static_cast<int> (panes.size()))
    {
        auto* activePanes { panes.at (index).get() };
        const juce::String paneType { AppState::getContext()->getActivePaneType() };

        if (paneType == App::ID::paneTypeDocument)
        {
            activePanes->closeWhelmed();

            if (AppState::getContext()->isDaemonMode())
                AppState::getContext()->save();
        }
        else if (activePanes->getPanes().size() > 1)
        {
            const juce::String activeID { AppState::getContext()->getActivePaneID() };

            int closedIndex { 0 };

            for (size_t i { 0 }; i < activePanes->getPanes().size(); ++i)
            {
                if (activePanes->getPanes().at (i)->getComponentID() == activeID)
                {
                    closedIndex = static_cast<int> (i);
                    break;
                }
            }

            activePanes->closePane (activeID);

            if (not activePanes->getPanes().isEmpty())
            {
                const int nextIndex { juce::jmin (closedIndex, static_cast<int> (activePanes->getPanes().size()) - 1) };
                auto* nearest { activePanes->getPanes().at (static_cast<size_t> (nextIndex)).get() };
                AppState::getContext()->setActivePaneID (nearest->getComponentID());

                if (nearest->isShowing())
                    nearest->grabKeyboardFocus();
            }

            if (AppState::getContext()->isDaemonMode())
                AppState::getContext()->save();
        }
        else
        {
            // C1 fix: collect terminal UUIDs BEFORE destroying Panes, then
            // remove sessions from Host AFTER Panes (and its Displays) are destroyed.
            // Destruction order: Display (~Display unwires callbacks) → Host::remove().
            // This matches the Panes::closePane() contract.
            juce::StringArray terminalUuids;

            for (const auto& pane : activePanes->getPanes())
            {
                if (pane->getPaneType() == App::ID::paneTypeTerminal)
                    terminalUuids.add (pane->getComponentID());
            }

            removeChildComponent (activePanes);
            panes.erase (panes.begin() + index);

            // Displays are now destroyed (Panes erased). Session::remove handles mode routing internally.
            for (const auto& uuid : terminalUuids)
                Nexus::getContext()->remove (uuid);

            removeTab (index);
            AppState::getContext()->removeTab (index);

            if (not panes.isEmpty())
            {
                const int newIndex { (index > 0) ? index - 1 : 0 };
                setCurrentTabIndex (newIndex);
            }

            updateTabBarVisibility();
        }
    }
}

/**
 * @brief Closes the pane identified by @p uuid, removing the tab if it becomes empty.
 *
 * Iterates all Panes instances. When the owning Panes is found (at least one pane
 * componentID matches the UUID), delegates to Panes::closePane. If the Panes
 * instance is then empty (last pane in the tab), the tab itself is removed via
 * the same single-pane branch as closeActiveTab.
 *
 * @note MESSAGE THREAD.
 */
void Tabs::closeSession (const juce::String& uuid)
{
    int ownerIndex { -1 };

    for (int i { 0 }; i < static_cast<int> (panes.size()) and ownerIndex < 0; ++i)
    {
        const auto& panePanes { panes.at (i)->getPanes() };

        for (size_t j { 0 }; j < panePanes.size() and ownerIndex < 0; ++j)
        {
            if (panePanes.at (j)->getComponentID() == uuid)
                ownerIndex = i;
        }
    }

    if (ownerIndex >= 0)
    {
        auto* ownerPanes { panes.at (ownerIndex).get() };

        if (ownerPanes->getPanes().size() > 1)
        {
            ownerPanes->closePane (uuid);

            if (AppState::getContext()->isDaemonMode())
                AppState::getContext()->save();
        }
        else
        {
            juce::StringArray terminalUuids;

            for (const auto& pane : ownerPanes->getPanes())
            {
                if (pane->getPaneType() == App::ID::paneTypeTerminal)
                    terminalUuids.add (pane->getComponentID());
            }

            removeChildComponent (ownerPanes);
            panes.erase (panes.begin() + ownerIndex);

            for (const auto& termUuid : terminalUuids)
                Nexus::getContext()->remove (termUuid);

            removeTab (ownerIndex);
            AppState::getContext()->removeTab (ownerIndex);

            if (not panes.isEmpty())
            {
                const int newIndex { (ownerIndex > 0) ? ownerIndex - 1 : 0 };
                setCurrentTabIndex (newIndex);
            }

            updateTabBarVisibility();

            if (not panes.isEmpty() and AppState::getContext()->isDaemonMode())
                AppState::getContext()->save();
        }
    }
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Terminal
