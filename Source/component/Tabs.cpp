/**
 * @file Tabs.cpp
 * @brief Terminal::Tabs implementation — tab lifecycle and visibility management.
 *
 * @see Tabs.h
 * @see Terminal::Component
 * @see Terminal::LookAndFeel
 */

#include "Tabs.h"

namespace Terminal
{

/**
 * @brief Constructs the tab container with the given tab bar orientation.
 *
 * Sets the custom LookAndFeel for tab drawing. The tab bar starts hidden
 * (depth 0) since no tabs exist yet. The first tab is added by the caller
 * (MainComponent) after construction.
 *
 * @param orientation  Tab bar position: top, bottom, left, or right.
 *
 * @note MESSAGE THREAD.
 */
Tabs::Tabs (juce::TabbedButtonBar::Orientation orientation)
    : juce::TabbedComponent (orientation)
{
    setLookAndFeel (&tabLookAndFeel);
    setTabBarDepth (0);
    setOutline (0);
}

/**
 * @brief Clears the LookAndFeel reference before destruction.
 *
 * @note MESSAGE THREAD.
 */
Tabs::~Tabs()
{
    setLookAndFeel (nullptr);
}

/**
 * @brief Creates a new terminal session in a new tab and switches to it.
 *
 * Construction order:
 * 1. Create Terminal::Component via Owner::add().
 * 2. Add as hidden child via addChildComponent().
 * 3. Add a tab entry to TabbedComponent with nullptr content.
 * 4. Switch to the new tab (which triggers currentTabChanged).
 * 5. Update tab bar visibility.
 *
 * @note MESSAGE THREAD.
 */
void Tabs::addNewTab()
{
    auto& terminal { terminals.add (std::make_unique<Terminal::Component>()) };

    if (onRepaintNeeded != nullptr)
        static_cast<Terminal::Component*> (terminal.get())->onRepaintNeeded = onRepaintNeeded;

    addChildComponent (terminal.get());

    const int tabIndex { getNumTabs() };
    addTab ("Terminal", juce::Colours::transparentBlack, nullptr, false, tabIndex);
    setCurrentTabIndex (tabIndex);

    updateTabBarVisibility();
}

/**
 * @brief Closes the active tab and destroys its terminal session.
 *
 * If this is the last tab, does nothing — the caller (MainComponent)
 * handles application quit when no tabs remain after this call.
 *
 * Removal order:
 * 1. Record active index.
 * 2. Remove the child component.
 * 3. Remove from Owner (destroys Terminal::Component + PTY).
 * 4. Remove the tab entry from TabbedComponent.
 * 5. Switch to adjacent tab (prefer left, fall back to right).
 * 6. Update tab bar visibility.
 *
 * @note MESSAGE THREAD.
 */
void Tabs::closeActiveTab()
{
    const int index { getCurrentTabIndex() };

    if (index >= 0 and static_cast<size_t> (index) < terminals.size())
    {
        if (terminals.size() > 1)
        {
            removeChildComponent (terminals.at (index).get());
            terminals.remove (index);
            removeTab (index);

            const int newIndex { (index > 0) ? index - 1 : 0 };
            setCurrentTabIndex (newIndex);

            updateTabBarVisibility();
        }
    }
}

/**
 * @brief Switches to the previous tab, wrapping around to the last.
 *
 * @note MESSAGE THREAD.
 */
void Tabs::selectPreviousTab()
{
    const int current { getCurrentTabIndex() };
    const int count { getNumTabs() };

    if (count > 1)
    {
        const int newIndex { (current > 0) ? current - 1 : count - 1 };
        setCurrentTabIndex (newIndex);
    }
}

/**
 * @brief Switches to the next tab, wrapping around to the first.
 *
 * @note MESSAGE THREAD.
 */
void Tabs::selectNextTab()
{
    const int current { getCurrentTabIndex() };
    const int count { getNumTabs() };

    if (count > 1)
    {
        const int newIndex { (current < count - 1) ? current + 1 : 0 };
        setCurrentTabIndex (newIndex);
    }
}

/**
 * @brief Returns the number of open tabs.
 *
 * @return The number of terminal tabs.
 * @note MESSAGE THREAD.
 */
int Tabs::getTabCount() const noexcept
{
    return static_cast<int> (terminals.size());
}

Terminal::Component* Tabs::getActiveTerminal() const noexcept
{
    const int index { getCurrentTabIndex() };

    if (index >= 0 and static_cast<size_t> (index) < terminals.size())
        return static_cast<Terminal::Component*> (terminals.at (index).get());

    return nullptr;
}

void Tabs::copySelection()
{
    if (auto* t { getActiveTerminal() })
        t->copySelection();
}

void Tabs::pasteClipboard()
{
    if (auto* t { getActiveTerminal() })
        t->pasteClipboard();
}

void Tabs::reloadConfig()
{
    if (auto* t { getActiveTerminal() })
        t->reloadConfig();
}

void Tabs::increaseZoom()
{
    if (auto* t { getActiveTerminal() })
        t->increaseZoom();
}

void Tabs::decreaseZoom()
{
    if (auto* t { getActiveTerminal() })
        t->decreaseZoom();
}

void Tabs::resetZoom()
{
    if (auto* t { getActiveTerminal() })
        t->resetZoom();
}

/**
 * @brief Lays out all terminal components to fill the content area.
 *
 * Calls the base class resized() first (which positions the tab bar),
 * then calculates the content area and applies it to every terminal.
 * Only the active terminal is visible, but all get correct bounds so
 * that switching tabs doesn't trigger an extra resize.
 *
 * @note MESSAGE THREAD.
 */
void Tabs::resized()
{
    juce::TabbedComponent::resized();

    const auto content { getLocalBounds().withTrimmedTop (getTabBarDepth()) };

    for (auto& terminal : terminals)
    {
        if (terminal != nullptr)
        {
            terminal->setBounds (content);
        }
    }
}

/**
 * @brief Toggles terminal visibility when the active tab changes.
 *
 * Hides all terminals, then shows only the one at the new index.
 * This is called by JUCE's internal changeCallback() after the tab
 * bar state has been updated. Since we pass nullptr as content to
 * addTab(), the base class does no child management — we handle it.
 *
 * @param newIndex  The index of the newly selected tab.
 * @param name      The name of the newly selected tab (unused).
 *
 * @note MESSAGE THREAD.
 */
void Tabs::currentTabChanged (int newIndex, const juce::String& name)
{
    for (auto& terminal : terminals)
    {
        if (terminal != nullptr)
        {
            terminal->setVisible (false);
        }
    }

    if (newIndex >= 0 and static_cast<size_t> (newIndex) < terminals.size())
    {
        terminals.at (newIndex)->setVisible (true);

        if (terminals.at (newIndex)->isShowing())
            terminals.at (newIndex)->grabKeyboardFocus();
    }
}

/**
 * @brief Shows or hides the tab bar based on the number of tabs.
 *
 * Tab bar is hidden (depth 0) when only one tab exists, and shown
 * at defaultTabBarHeight when multiple tabs are present.
 *
 * @note MESSAGE THREAD.
 */
void Tabs::updateTabBarVisibility()
{
    const int depth { (getNumTabs() > 1) ? defaultTabBarHeight : 0 };
    setTabBarDepth (depth);
}

}
