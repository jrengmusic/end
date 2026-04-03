/**
 * @file Tabs.cpp
 * @brief Terminal::Tabs implementation — tab lifecycle and visibility management.
 *
 * @see Tabs.h
 * @see Terminal::Component
 * @see Terminal::LookAndFeel
 */

#include "Tabs.h"
#include "../AppState.h"
#include "../terminal/data/Identifier.h"
#include "../whelmed/Component.h"

namespace Terminal
{ /*____________________________________________________________________________*/
/**
 * @brief Constructs the tab container with the given tab bar orientation.
 *
 * The tab bar starts hidden
 * (depth 0) since no tabs exist yet. The first tab is added by the caller
 * (MainComponent) after construction.
 *
 * @param orientation  Tab bar position: top, bottom, left, or right.
 *
 * @note MESSAGE THREAD.
 */
Tabs::Tabs (jreng::Typeface& font_,
            jreng::Typeface& whelmedBodyFont_,
            jreng::Typeface& whelmedCodeFont_,
            juce::TabbedButtonBar::Orientation orientation)
    : juce::TabbedComponent (orientation)
    , font (font_)
    , whelmedBodyFont (whelmedBodyFont_)
    , whelmedCodeFont (whelmedCodeFont_)
{
    setOpaque (false);
    setTabBarDepth (0);
    setOutline (0);
    juce::Desktop::getInstance().addFocusChangeListener (this);
    tabName.addListener (this);
}

/**
 * @brief Clears the LookAndFeel reference before destruction.
 *
 * @note MESSAGE THREAD.
 */
Tabs::~Tabs()
{
    tabName.removeListener (this);
    juce::Desktop::getInstance().removeFocusChangeListener (this);
}

/**
 * @brief Creates a new Panes instance in a new tab and switches to it.
 *
 * @note MESSAGE THREAD.
 */
void Tabs::addNewTab()
{
    auto& newPanesPtr { panes.add (std::make_unique<Panes> (font, whelmedBodyFont, whelmedCodeFont)) };
    auto& newPanes { *newPanesPtr };
    newPanes.onRepaintNeeded = onRepaintNeeded;
    newPanes.onOpenMarkdown = [this] (const juce::File& file)
    {
        if (auto* active { getActivePanes() }; active != nullptr)
            active->createWhelmed (file);
    };
    newPanes.onLastPaneClosed = [this]
    {
        closeActiveTab();

        if (getTabCount() == 0)
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
    };
    addChildComponent (&newPanes);

    const auto uuid { newPanes.createTerminal (AppState::getContext()->getPwd()) };

    auto tab { AppState::getContext()->addTab() };
    tab.removeChild (tab.getChildWithName (App::ID::PANES), nullptr);
    tab.appendChild (newPanes.getState(), nullptr);

    AppState::getContext()->setActivePaneID (uuid);
    auto paneNode { jreng::PaneManager::findLeaf (newPanes.getState(), uuid) };
    auto sessionTree { paneNode.getChild (0) };
    AppState::getContext()->setPwd (sessionTree);
    tabName.referTo (sessionTree.getPropertyAsValue (Terminal::ID::displayName, nullptr));

    const auto initialName { juce::File (AppState::getContext()->getPwd()).getFileName() };
    const int tabIndex { getNumTabs() };
    addTab (initialName, juce::Colours::transparentBlack, nullptr, false, tabIndex);
    setCurrentTabIndex (tabIndex);

    updateTabBarVisibility();
}

/**
 * @brief Returns the Panes instance for the active tab.
 *
 * @return Pointer to the active Panes, or nullptr if none.
 * @note MESSAGE THREAD.
 */
Panes* Tabs::getActivePanes() const noexcept
{
    const int index { getCurrentTabIndex() };

    if (index >= 0 and index < static_cast<int> (panes.size()))
    {
        return panes.at (index).get();
    }

    return nullptr;
}

/**
 * @brief Returns the active Panes' panes for GL iteration.
 *
 * @return Reference to the active pane owner, or a static empty owner.
 * @note MESSAGE THREAD.
 */
jreng::Owner<PaneComponent>& Tabs::getPanes() noexcept
{
    static jreng::Owner<PaneComponent> empty;

    if (auto* active { getActivePanes() }; active != nullptr)
        return active->getPanes();

    return empty;
}

void Tabs::globalFocusChanged (juce::Component* focusedComponent)
{
    if (auto* term { dynamic_cast<Terminal::Component*> (focusedComponent) }; term != nullptr)
    {
        const auto uuid { term->getValueTree().getProperty (jreng::ID::id).toString() };
        AppState::getContext()->setActivePaneID (uuid);
        AppState::getContext()->setPwd (term->getValueTree());
        tabName.referTo (term->getValueTree().getPropertyAsValue (Terminal::ID::displayName, nullptr));
    }
}

void Tabs::valueChanged (juce::Value& value)
{
    if (value.refersToSameSourceAs (tabName))
    {
        const auto name { tabName.toString() };

        if (name.isNotEmpty())
        {
            getTabbedButtonBar().setTabName (getCurrentTabIndex(), name);
        }
    }
}

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
        }
        else
        {
            removeChildComponent (activePanes);
            panes.erase (panes.begin() + index);

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
int Tabs::getTabCount() const noexcept { return getNumTabs(); }

/**
 * @brief Returns the currently active terminal component.
 *
 * @return Pointer to the active terminal, or nullptr if none.
 * @note MESSAGE THREAD.
 */
Terminal::Component* Tabs::getActiveTerminal() const noexcept
{
    const auto activeID { AppState::getContext()->getActivePaneID() };

    if (auto* active { getActivePanes() }; active != nullptr)
    {
        for (auto& pane : active->getPanes())
        {
            if (pane->getComponentID() == activeID)
            {
                return dynamic_cast<Terminal::Component*> (pane.get());
            }
        }
    }

    return nullptr;
}


PaneComponent* Tabs::getActivePane() const noexcept
{
    const auto activeID { AppState::getContext()->getActivePaneID() };
    const auto activeType { AppState::getContext()->getActivePaneType() };

    if (auto* active { getActivePanes() }; active != nullptr)
    {
        for (auto& pane : active->getPanes())
        {
            if (pane->getComponentID() == activeID and pane->getPaneType() == activeType)
            {
                return pane.get();
            }
        }
    }

    return nullptr;
}

bool Tabs::hasSelection() const noexcept
{
    if (const auto* pane { getActivePane() }; pane != nullptr)
        return pane->hasSelection();

    return false;
}

void Tabs::copySelection()
{
    if (auto* pane { getActivePane() }; pane != nullptr)
        pane->copySelection();
}

void Tabs::pasteClipboard()
{
    if (auto* t { getActiveTerminal() }; t != nullptr)
        t->pasteClipboard();
}

void Tabs::writeToActivePty (const char* data, int len)
{
    if (auto* t { getActiveTerminal() }; t != nullptr)
        t->writeToPty (data, len);
}

void Tabs::applyConfig()
{
    for (auto& p : panes)
    {
        for (auto& pane : p->getPanes())
        {
            pane->applyConfig();
        }
    }
}

void Tabs::switchRenderer (App::RendererType type)
{
    for (auto& p : panes)
    {
        for (auto& pane : p->getPanes())
        {
            pane->switchRenderer (type);
        }
    }
}

void Tabs::increaseZoom()
{
    if (auto* t { getActiveTerminal() }; t != nullptr)
        t->increaseZoom();
}

void Tabs::decreaseZoom()
{
    if (auto* t { getActiveTerminal() }; t != nullptr)
        t->decreaseZoom();
}

void Tabs::resetZoom()
{
    if (auto* t { getActiveTerminal() }; t != nullptr)
        t->resetZoom();
}

void Tabs::focusLastTerminal (Panes* active)
{
    auto& activePanes { active->getPanes() };

    if (not activePanes.isEmpty())
    {
        auto* lastPane { activePanes.back().get() };
        AppState::getContext()->setActivePaneID (lastPane->getComponentID());

        if (lastPane->isShowing())
            lastPane->grabKeyboardFocus();
    }
}

void Tabs::splitHorizontal()
{
    if (auto* active { getActivePanes() }; active != nullptr)
    {
        active->splitHorizontal();
        focusLastTerminal (active);
    }
}

void Tabs::splitVertical()
{
    if (auto* active { getActivePanes() }; active != nullptr)
    {
        active->splitVertical();
        focusLastTerminal (active);
    }
}

/**
 * @brief Lays out the active Panes to fill the content area.
 *
 * @note MESSAGE THREAD.
 */
void Tabs::resized()
{
    juce::TabbedComponent::resized();

    const auto depth { getTabBarDepth() };
    const auto orientation { getOrientation() };
    auto content { getLocalBounds() };

    if (orientation == juce::TabbedButtonBar::TabsAtTop)
    {
        content = content.withTrimmedTop (depth);
    }
    else if (orientation == juce::TabbedButtonBar::TabsAtBottom)
    {
        content = content.withTrimmedBottom (depth);
    }
    else if (orientation == juce::TabbedButtonBar::TabsAtLeft)
    {
        content = content.withTrimmedLeft (depth);
    }
    else if (orientation == juce::TabbedButtonBar::TabsAtRight)
    {
        content = content.withTrimmedRight (depth);
    }

    if (auto* active { getActivePanes() }; active != nullptr)
    {
        active->setBounds (content);
    }
}

/**
 * @brief Toggles Panes visibility when the active tab changes.
 *
 * Hides all Panes, shows the newly selected one, then updates focus.
 *
 * @param newIndex  The index of the newly selected tab.
 *
 * @note MESSAGE THREAD.
 */
void Tabs::currentTabChanged (int newIndex, const juce::String&)
{
    for (auto& p : panes)
    {
        p->setVisible (false);
    }

    AppState::getContext()->setModalType (0);
    AppState::getContext()->setSelectionType (0);

    resized();

    if (newIndex >= 0 and newIndex < static_cast<int> (panes.size()))
    {
        panes.at (newIndex)->setVisible (true);
    }

    if (auto* active { getActivePanes() }; active != nullptr)
    {
        auto& activePanes { active->getPanes() };

        if (not activePanes.isEmpty())
        {
            auto* firstPane { activePanes.at (0).get() };
            AppState::getContext()->setActivePaneID (firstPane->getComponentID());

            if (firstPane->isShowing())
            {
                firstPane->grabKeyboardFocus();
            }
        }
    }

    AppState::getContext()->setActiveTabIndex (newIndex);
}

/**
 * @brief Shows or hides the tab bar based on the number of tabs.
 *
 * Tab bar is hidden (depth 0) when only one tab exists, and shown
 * at the height derived from the configured tab font size when
 * multiple tabs are present.
 *
 * @note MESSAGE THREAD.
 */
void Tabs::updateTabBarVisibility()
{
    const int depth { (getNumTabs() > 1) ? LookAndFeel::getTabBarHeight() : 0 };
    setTabBarDepth (depth);
}

void Tabs::applyOrientation()
{
    const auto* cfg { Config::getContext() };
    const auto orientation { orientationFromString (cfg->getString (Config::Key::tabPosition)) };
    setOrientation (orientation);
    updateTabBarVisibility();
}

juce::TabbedButtonBar::Orientation Tabs::orientationFromString (const juce::String& position)
{
    if (position == "top")
        return juce::TabbedButtonBar::TabsAtTop;

    if (position == "bottom")
        return juce::TabbedButtonBar::TabsAtBottom;

    if (position == "right")
        return juce::TabbedButtonBar::TabsAtRight;

    return juce::TabbedButtonBar::TabsAtLeft;
}

void Tabs::openMarkdown (const juce::File& file)
{
    if (auto* active { getActivePanes() }; active != nullptr)
        active->createWhelmed (file);
}

void Tabs::focusPaneLeft()
{
    if (auto* active { getActivePanes() }; active != nullptr)
        active->focusPane (-1, 0);
}

void Tabs::focusPaneDown()
{
    if (auto* active { getActivePanes() }; active != nullptr)
        active->focusPane (0, 1);
}

void Tabs::focusPaneUp()
{
    if (auto* active { getActivePanes() }; active != nullptr)
        active->focusPane (0, -1);
}

void Tabs::focusPaneRight()
{
    if (auto* active { getActivePanes() }; active != nullptr)
        active->focusPane (1, 0);
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Terminal
