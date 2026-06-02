/**
 * @file Tabs.cpp
 * @brief terminal::Tabs implementation — tab lifecycle and visibility management.
 *
 * @see Tabs.h
 * @see terminal::Display
 * @see terminal::LookAndFeel
 */

#include "Tabs.h"

namespace terminal
{
/*____________________________________________________________________________*/
Tabs::Tabs (jam::TabbedButtonBar::Orientation orientation)
    : jam::TabbedComponent (orientation)
{
    setOpaque (false);
    setTabBarDepth (0);
    setOutline (0);
    juce::Desktop::getInstance().addFocusChangeListener (this);
    tabName.addListener (this);
    AppModel::getContext()->getWindow().addListener (this);

    getTabbedButtonBar().onTabMoved = [this] (int fromIndex, int toIndex)
    {
        auto movedPane { std::move (panes.at (fromIndex)) };
        panes.erase (panes.begin() + fromIndex);
        panes.insert (panes.begin() + toIndex, std::move (movedPane));

        auto tabsTree { AppModel::getContext()->getTabs() };
        tabsTree.moveChild (fromIndex, toIndex, nullptr);

        if (AppModel::getContext()->isDaemonMode())
            AppModel::getContext()->save();
    };
}

/**
 * @brief Clears the LookAndFeel reference before destruction.
 *
 * @note MESSAGE THREAD.
 */
Tabs::~Tabs()
{
    AppModel::getContext()->getWindow().removeListener (this);
    tabName.removeListener (this);
    juce::Desktop::getInstance().removeFocusChangeListener (this);
}

/**
 * @brief Creates a new Panes instance in a new tab and switches to it.
 *
 * Computes terminal dimensions from the current Tabs component bounds minus the
 * (soon-to-be-updated) tab bar depth. Tabs is always laid out by the time the
 * user triggers cmd+T (window is visible and sized).
 *
 * @note MESSAGE THREAD.
 */
void Tabs::addNewTab()
{
    // Compute the content rect for the new pane's spawn dims.
    // newDepth is the tab-bar depth that will apply AFTER this tab is added.
    const int newTabCount { getNumTabs() + 1 };
    const int newDepth { (newTabCount > 1) ? LookAndFeel::getTabBarHeight() : 0 };
    const auto contentRect { computeContentRect (newDepth) };
    const auto [newCols, newRows] { Panes::cellsFromRect (contentRect) };

    addNewTab (AppModel::getContext()->getPwd(), {}, jam::Cell::Rectangle (newCols, newRows));
}

void Tabs::addNewTab (const juce::String& workingDirectory, const juce::String& uuid, jam::Cell::Rectangle dims)
{
    jassert (dims.getWidth().value > 0 and dims.getHeight().value > 0);

    auto& newPanesPtr { panes.add (std::make_unique<Panes>()) };
    auto& newPanes { *newPanesPtr };
    // Register as VT listener on this tab's PANES tree — valueTreeChildRemoved
    // fires when the last pane exits and triggers tab close / quit.
    newPanes.getState().addListener (this);
    addChildComponent (&newPanes);

    const auto sessionUuid { newPanes.createTerminal (workingDirectory, uuid, dims) };

    auto tab { AppModel::getContext()->addTab() };
    tab.removeChild (tab.getChildWithName (app::id::PANES), nullptr);
    tab.appendChild (newPanes.getState(), nullptr);

    AppModel::getContext()->setActivePaneID (sessionUuid);
    auto paneNode { jam::PaneManager::findLeaf (newPanes.getState(), sessionUuid) };
    auto sessionTree { paneNode.getChild (0) };
    AppModel::getContext()->setPwd (sessionTree);
    tabName.referTo (sessionTree.getPropertyAsValue (app::id::displayName, nullptr));

    const auto initialName { juce::File (AppModel::getContext()->getPwd()).getFileName() };
    const int tabIndex { getNumTabs() };
    addTab (initialName, juce::Colours::transparentBlack, tabIndex);
    setCurrentTabIndex (tabIndex);

    updateTabBarVisibility();

    if (AppModel::getContext()->isDaemonMode())
        AppModel::getContext()->save();
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
    Panes* result { nullptr };

    if (index >= 0 and index < static_cast<int> (panes.size()))
        result = panes.at (index).get();

    return result;
}

/**
 * @brief Returns the active Panes' panes for GL iteration.
 *
 * @return Reference to the active pane owner, or a static empty owner.
 * @note MESSAGE THREAD.
 */
jam::Owner<PaneView>& Tabs::getPanes() noexcept
{
    static jam::Owner<PaneView> empty;
    jam::Owner<PaneView>* result { &empty };

    if (auto* active { getActivePanes() }; active != nullptr)
        result = &active->getPanes();

    return *result;
}

void Tabs::globalFocusChanged (juce::Component* focusedComponent)
{
    if (auto* term { dynamic_cast<terminal::Display*> (focusedComponent) }; term != nullptr)
    {
        const auto uuid { term->getComponentID() };
        auto sessionTree { term->getProcessor().getState().getRootTree() };
        tabName.referTo (sessionTree.getPropertyAsValue (app::id::displayName, nullptr));
        AppModel::getContext()->setActivePaneID (uuid);
        AppModel::getContext()->setPwd (sessionTree);
    }
}

void Tabs::valueChanged (juce::Value& value)
{
    if (value.refersToSameSourceAs (tabName))
    {
        const auto index { getCurrentTabIndex() };

        if (index >= 0)
        {
            const auto tabNode { AppModel::getContext()->getTab (index) };
            const auto userOverride { tabNode.getProperty (app::id::userTabName).toString() };
            const auto name { userOverride.isNotEmpty() ? userOverride : tabName.toString() };

            if (name.isNotEmpty())
                getTabbedButtonBar().setTabName (index, name);
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
terminal::Display* Tabs::getActiveTerminal() const noexcept
{
    const auto activeID { AppModel::getContext()->getActivePaneID() };
    terminal::Display* result { nullptr };

    if (auto* active { getActivePanes() }; active != nullptr)
    {
        for (auto& pane : active->getPanes())
        {
            if (pane->getComponentID() == activeID)
                result = dynamic_cast<terminal::Display*> (pane.get());
        }
    }

    return result;
}

PaneView* Tabs::getActivePane() const noexcept
{
    const auto activeID { AppModel::getContext()->getActivePaneID() };
    const auto activeType { AppModel::getContext()->getActivePaneType() };
    PaneView* result { nullptr };

    if (auto* active { getActivePanes() }; active != nullptr)
    {
        for (auto& pane : active->getPanes())
        {
            if (pane->getComponentID() == activeID and pane->getPaneType() == activeType)
                result = pane.get();
        }
    }

    return result;
}

bool Tabs::hasSelection() const noexcept
{
    bool result { false };

    if (const auto* pane { getActivePane() }; pane != nullptr)
        result = pane->hasSelection();

    return result;
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

/**
 * @brief Lays out the active Panes to fill the content area.
 *
 * @note MESSAGE THREAD.
 */
void Tabs::resized()
{
    jam::TabbedComponent::resized();

    if (auto* active { getActivePanes() }; active != nullptr)
    {
        active->setBounds (computeContentRect (getTabBarDepth()));
    }
}

void Tabs::currentTabChanged (int newIndex, const juce::String&)
{
    for (auto& p : panes)
    {
        p->setVisible (false);
    }

    AppModel::getContext()->setModalType (0);
    AppModel::getContext()->setSelectionType (0);

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
            AppModel::getContext()->setActivePaneID (firstPane->getComponentID());

            if (firstPane->isShowing())
            {
                firstPane->grabKeyboardFocus();
            }
        }
    }

    AppModel::getContext()->setActiveTabIndex (newIndex);
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
    const auto orientation { orientationFromString (lua::Engine::getContext()->display.tab.position) };
    setOrientation (orientation);
    updateTabBarVisibility();
}

jam::TabbedButtonBar::Orientation Tabs::orientationFromString (const juce::String& position)
{
    return static_cast<jam::TabbedButtonBar::Orientation> (Map::TabPosition::getContext()->get (position));
}

juce::Rectangle<int> Tabs::computeContentRect (int tabBarDepth) const noexcept
{
    const auto* ctx { AppModel::getContext() };
    const juce::Rectangle<int> base { 0, 0, ctx->getWindowWidth(), ctx->getWindowHeight() };

    juce::Rectangle<int> result { base };

    if (tabBarDepth > 0)
    {
        const auto orientation { getOrientation() };

        if (orientation == jam::TabbedButtonBar::TabsAtTop)
            result = base.withTrimmedTop (tabBarDepth);
        else if (orientation == jam::TabbedButtonBar::TabsAtBottom)
            result = base.withTrimmedBottom (tabBarDepth);
        else if (orientation == jam::TabbedButtonBar::TabsAtLeft)
            result = base.withTrimmedLeft (tabBarDepth);
        else if (orientation == jam::TabbedButtonBar::TabsAtRight)
            result = base.withTrimmedRight (tabBarDepth);
    }

    return result;
}

void Tabs::openMarkdown (const juce::File& file)
{
    if (auto* active { getActivePanes() }; active != nullptr)
        active->createWhelmed (file);
}

void Tabs::popupMenuClickOnTab (int tabIndex, const juce::String&)
{
    showRenameEditor (tabIndex);
}

void Tabs::renameActiveTab (const juce::String& name)
{
    const auto index { getCurrentTabIndex() };

    if (index >= 0)
    {
        auto tabNode { AppModel::getContext()->getTab (index) };

        if (name.isEmpty())
            tabNode.removeProperty (app::id::userTabName, nullptr);
        else
            tabNode.setProperty (app::id::userTabName, name, nullptr);

        const auto displayName { name.isNotEmpty() ? name : tabName.toString() };

        if (displayName.isNotEmpty())
            getTabbedButtonBar().setTabName (index, displayName);

        if (AppModel::getContext()->isDaemonMode())
            AppModel::getContext()->save();
    }
}

void Tabs::showRenameEditor (int tabIndex)
{
    if (auto* button = dynamic_cast<jam::TabBarButton*> (getTabbedButtonBar().getTabButton (tabIndex));
        button != nullptr)
    {
        const auto originalText { button->getButtonText() };

        button->onRenameCommit = [this, tabIndex, originalText] (const juce::String& newText)
        {
            if (newText != originalText)
                renameActiveTab (newText);
        };

        button->showRenameEditor();
    }
}

void Tabs::focusLastTerminal (Panes* active)
{
    auto& activePanes { active->getPanes() };

    if (not activePanes.isEmpty())
    {
        auto* lastPane { activePanes.back().get() };
        AppModel::getContext()->setActivePaneID (lastPane->getComponentID());

        if (lastPane->isShowing())
            lastPane->grabKeyboardFocus();
    }
}

// =============================================================================

/**
 * @brief Handles pendingMarkdownFile and pendingImageFile written by LinkManager::dispatch().
 *
 * LinkManager writes to the WINDOW node when the user activates a .md or image link.
 * Tabs reads the path, clears the property, and creates the appropriate pane.
 *
 * @note MESSAGE THREAD — VT listener fires on the message thread.
 */
void Tabs::valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property)
{
    if (tree.getType() == app::id::WINDOW)
    {
        if (property == app::id::pendingMarkdownFile)
        {
            const auto path { tree.getProperty (app::id::pendingMarkdownFile).toString() };
            tree.removeProperty (app::id::pendingMarkdownFile, nullptr);

            if (path.isNotEmpty())
            {
                if (auto* active { getActivePanes() }; active != nullptr)
                    active->createWhelmed (juce::File (path));
            }
        }
        else if (property == app::id::pendingImageFile)
        {
            // Image open path — consume and clear.
            tree.removeProperty (app::id::pendingImageFile, nullptr);
        }
    }
}

/**
 * @brief Detects when a PANE child is removed from the PANES VT of a tab.
 *
 * Searches the owned Panes instances to find the one whose state VT matches
 * the notifying parent.  When that Panes instance is empty (all terminals
 * closed), closes the active tab and quits if no tabs remain.
 *
 * This replaces the onLastPaneClosed callback on Panes.
 *
 * @note MESSAGE THREAD — VT listener fires on the message thread.
 */
void Tabs::valueTreeChildRemoved (juce::ValueTree& parent, juce::ValueTree&, int)
{
    for (auto& panesPtr : panes)
    {
        if (panesPtr->getState() == parent and panesPtr->isEmpty())
        {
            closeActiveTab();

            if (getTabCount() == 0)
            {
                juce::JUCEApplication::getInstance()->systemRequestedQuit();
            }
            else if (AppModel::getContext()->isDaemonMode())
            {
                AppModel::getContext()->save();
            }

            break;
        }
    }
}

/**______________________________END OF NAMESPACE______________________________*/
} // namespace terminal
