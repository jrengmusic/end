/**
 * @file Tabs.cpp
 * @brief Terminal::Tabs implementation — tab lifecycle and visibility management.
 *
 * @see Tabs.h
 * @see Terminal::Display
 * @see Terminal::LookAndFeel
 */

#include "Tabs.h"
#include "../AppState.h"
#include "../terminal/data/Identifier.h"
#include "../whelmed/Component.h"
#include "../nexus/Nexus.h"

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
Tabs::Tabs (jam::Typeface& font_,
            jam::GlyphAtlas& glAtlas_,
            jam::GraphicsAtlas& graphicsAtlas_,
            juce::TabbedButtonBar::Orientation orientation)
    : juce::TabbedComponent (orientation)
    , font (font_)
    , glAtlasRef (glAtlas_)
    , graphicsAtlasRef (graphicsAtlas_)
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
    const auto [cols, rows] { Panes::cellsFromRect (contentRect, font) };

    addNewTab (AppState::getContext()->getPwd(), {}, cols, rows);
}

/**
 * @brief Creates a new Panes instance in a new tab, using the given cwd, UUID hint, and spawn dims.
 *
 * Used by the state restoration walker. Passes all parameters through to
 * Panes::createTerminal() so the first terminal spawns with deterministic PTY dims
 * derived from the saved split tree, rather than zero bounds.
 *
 * @param workingDirectory  Initial cwd for the first terminal.
 * @param uuid              UUID hint.
 * @param cols              Terminal column count. Must be > 0.
 * @param rows              Terminal row count. Must be > 0.
 * @note MESSAGE THREAD.
 */
void Tabs::addNewTab (const juce::String& workingDirectory, const juce::String& uuid, int cols, int rows)
{
    jassert (cols > 0 and rows > 0);

    auto& newPanesPtr { panes.add (std::make_unique<Panes> (font, glAtlasRef, graphicsAtlasRef)) };
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
        {
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
        }
        else if (AppState::getContext()->isDaemonMode())
        {
            AppState::getContext()->save();
        }
    };
    addChildComponent (&newPanes);

    const auto sessionUuid { newPanes.createTerminal (workingDirectory, uuid, cols, rows) };

    auto tab { AppState::getContext()->addTab() };
    tab.removeChild (tab.getChildWithName (App::ID::PANES), nullptr);
    tab.appendChild (newPanes.getState(), nullptr);

    AppState::getContext()->setActivePaneID (sessionUuid);
    auto paneNode { jam::PaneManager::findLeaf (newPanes.getState(), sessionUuid) };
    auto sessionTree { paneNode.getChild (0) };
    AppState::getContext()->setPwd (sessionTree);
    tabName.referTo (sessionTree.getPropertyAsValue (App::ID::displayName, nullptr));

    const auto initialName { juce::File (AppState::getContext()->getPwd()).getFileName() };
    const int tabIndex { getNumTabs() };
    addTab (initialName, juce::Colours::transparentBlack, nullptr, false, tabIndex);
    setCurrentTabIndex (tabIndex);

    updateTabBarVisibility();

    if (AppState::getContext()->isDaemonMode())
        AppState::getContext()->save();
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
jam::Owner<PaneComponent>& Tabs::getPanes() noexcept
{
    static jam::Owner<PaneComponent> empty;
    jam::Owner<PaneComponent>* result { &empty };

    if (auto* active { getActivePanes() }; active != nullptr)
        result = &active->getPanes();

    return *result;
}

void Tabs::globalFocusChanged (juce::Component* focusedComponent)
{
    if (auto* pane { dynamic_cast<PaneComponent*> (focusedComponent) }; pane != nullptr)
    {
        tabName.referTo (pane->getValueTree().getPropertyAsValue (App::ID::displayName, nullptr));

        if (auto* term { dynamic_cast<Terminal::Display*> (focusedComponent) }; term != nullptr)
        {
            const auto uuid { term->getValueTree().getProperty (jam::ID::id).toString() };
            AppState::getContext()->setActivePaneID (uuid);
            AppState::getContext()->setPwd (term->getValueTree());
        }
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
Terminal::Display* Tabs::getActiveTerminal() const noexcept
{
    const auto activeID { AppState::getContext()->getActivePaneID() };
    Terminal::Display* result { nullptr };

    if (auto* active { getActivePanes() }; active != nullptr)
    {
        for (auto& pane : active->getPanes())
        {
            if (pane->getComponentID() == activeID)
                result = dynamic_cast<Terminal::Display*> (pane.get());
        }
    }

    return result;
}

PaneComponent* Tabs::getActivePane() const noexcept
{
    const auto activeID { AppState::getContext()->getActivePaneID() };
    const auto activeType { AppState::getContext()->getActivePaneType() };
    PaneComponent* result { nullptr };

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
    const float current { AppState::getContext()->getWindowZoom() };
    const float newZoom { juce::jlimit (Config::zoomMin, Config::zoomMax, current + Config::zoomStep) };
    AppState::getContext()->setWindowZoom (newZoom);

    for (auto& p : panes)
    {
        for (auto& pane : p->getPanes())
        {
            pane->applyZoom (newZoom);
        }
    }
}

void Tabs::decreaseZoom()
{
    const float current { AppState::getContext()->getWindowZoom() };
    const float newZoom { juce::jlimit (Config::zoomMin, Config::zoomMax, current - Config::zoomStep) };
    AppState::getContext()->setWindowZoom (newZoom);

    for (auto& p : panes)
    {
        for (auto& pane : p->getPanes())
        {
            pane->applyZoom (newZoom);
        }
    }
}

void Tabs::resetZoom()
{
    const float defaultZoom { Config::zoomMin };
    AppState::getContext()->setWindowZoom (defaultZoom);

    for (auto& p : panes)
    {
        for (auto& pane : p->getPanes())
        {
            pane->applyZoom (defaultZoom);
        }
    }
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

        if (AppState::getContext()->isDaemonMode())
            AppState::getContext()->save();
    }
}

void Tabs::splitVertical()
{
    if (auto* active { getActivePanes() }; active != nullptr)
    {
        active->splitVertical();
        focusLastTerminal (active);

        if (AppState::getContext()->isDaemonMode())
            AppState::getContext()->save();
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

    if (auto* active { getActivePanes() }; active != nullptr)
    {
        active->setBounds (computeContentRect (getTabBarDepth()));
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
    static const std::unordered_map<juce::String, juce::TabbedButtonBar::Orientation> table {
        { "top",    juce::TabbedButtonBar::TabsAtTop    },
        { "bottom", juce::TabbedButtonBar::TabsAtBottom },
        { "right",  juce::TabbedButtonBar::TabsAtRight  },
        { "left",   juce::TabbedButtonBar::TabsAtLeft   },
    };

    juce::TabbedButtonBar::Orientation result { juce::TabbedButtonBar::TabsAtLeft };
    const auto it { table.find (position) };

    if (it != table.end())
        result = it->second;

    return result;
}

/**
 * @brief Walks saved TAB nodes, creates tabs, and replays splits.
 *
 * Caller must pass a deep copy of the saved TABS tree detached from the live
 * AppState tree — this method walks it directly without aliasing live state.
 *
 * For each TAB child: locates the first PANE leaf (DFS, left-first) to obtain
 * uuid and cwd for addNewTab, then recursively descends the PANES subtree
 * pre-order, deriving child rects via Panes::splitRect and emitting one
 * splitAt call per internal PANES node with 2 children. Overwrites the default
 * ratio on each new split node with the saved value.
 *
 * @param savedTabs   Deep copy of the TABS node — not aliased with live AppState.
 * @param contentRect Chrome-subtracted pixel rect for dim computation.
 * @note MESSAGE THREAD.
 */
void Tabs::restore (juce::ValueTree savedTabs, juce::Rectangle<int> contentRect)
{
    // Recursive leaf finder — returns {uuid, cwd} of the first PANE with a SESSION.
    std::function<std::pair<juce::String, juce::String> (const juce::ValueTree&)> findFirstLeaf;
    findFirstLeaf = [&] (const juce::ValueTree& node) -> std::pair<juce::String, juce::String>
    {
        std::pair<juce::String, juce::String> result;

        if (node.getType() == App::ID::PANE)
        {
            const auto sessionNode { node.getChildWithName (Terminal::ID::SESSION) };

            if (sessionNode.isValid())
            {
                result.first = sessionNode.getProperty (jam::ID::id).toString();
                result.second = sessionNode.getProperty (Terminal::ID::cwd).toString();
            }
        }
        else
        {
            for (int i { 0 }; i < node.getNumChildren() and result.first.isEmpty(); ++i)
                result = findFirstLeaf (node.getChild (i));
        }

        return result;
    };

    // Recursive PANES descent — emits splitAt calls pre-order and descends rects.
    // parentRect is the pixel rect assigned to this node (chrome already subtracted).
    std::function<void (const juce::ValueTree&, juce::Rectangle<int>, Panes*)> walkPanes;
    walkPanes = [&] (const juce::ValueTree& node, juce::Rectangle<int> parentRect, Panes* activePanes)
    {
        if (node.getType() == App::ID::PANES and node.getNumChildren() == 2)
        {
            const juce::String direction { node.getProperty (jam::PaneManager::idDirection).toString() };
            const double savedRatio { static_cast<double> (node.getProperty (jam::PaneManager::idRatio, 0.5)) };

            const auto [targetRect, newRect] { Panes::splitRect (parentRect, direction, savedRatio) };

            const auto [targetUuid, targetCwd] { findFirstLeaf (node.getChild (0)) };
            const auto [newUuid, newCwd] { findFirstLeaf (node.getChild (1)) };

            if (targetUuid.isNotEmpty() and newUuid.isNotEmpty())
            {
                const auto [newCols, newRows] { Panes::cellsFromRect (newRect, font) };
                const bool isVertical { direction == "vertical" };
                activePanes->splitAt (targetUuid, newUuid, newCwd, direction, isVertical, newCols, newRows);

                auto newLeafNode { jam::PaneManager::findLeaf (activePanes->getState(), newUuid) };
                jassert (newLeafNode.isValid());

                auto splitNode { newLeafNode.getParent() };
                jassert (splitNode.isValid());

                splitNode.setProperty (jam::PaneManager::idRatio, savedRatio, nullptr);
            }

            walkPanes (node.getChild (0), targetRect, activePanes);
            walkPanes (node.getChild (1), newRect, activePanes);
        }
        else
        {
            // Single-child PANES root (before first split) or PANE leaf — pass rect through.
            for (int i { 0 }; i < node.getNumChildren(); ++i)
                walkPanes (node.getChild (i), parentRect, activePanes);
        }
    };

    for (int t { 0 }; t < savedTabs.getNumChildren(); ++t)
    {
        const auto tabNode { savedTabs.getChild (t) };

        if (tabNode.getType() == App::ID::TAB)
        {
            const auto panesNode { tabNode.getChildWithName (App::ID::PANES) };

            if (panesNode.isValid())
            {
                const auto [firstUuid, firstCwd] { findFirstLeaf (panesNode) };

                if (firstUuid.isNotEmpty())
                {
                    // Compute the first leaf's actual sub-rect by descending the
                    // left branch of the saved split tree.  Each PANES node with
                    // 2 children narrows the rect via splitRect.
                    auto firstLeafRect { contentRect };
                    auto walkNode { panesNode };

                    while (walkNode.getType() == App::ID::PANES and walkNode.getNumChildren() == 2)
                    {
                        const juce::String dir { walkNode.getProperty (jam::PaneManager::idDirection).toString() };
                        const double ratio { static_cast<double> (
                            walkNode.getProperty (jam::PaneManager::idRatio, 0.5)) };
                        const auto [targetRect, newRect] { Panes::splitRect (firstLeafRect, dir, ratio) };
                        firstLeafRect = targetRect;
                        walkNode = walkNode.getChild (0);
                    }

                    const auto [firstCols, firstRows] { Panes::cellsFromRect (firstLeafRect, font) };
                    addNewTab (firstCwd, firstUuid, firstCols, firstRows);

                    auto* activePanes { getActivePanes() };
                    jassert (activePanes != nullptr);

                    walkPanes (panesNode, contentRect, activePanes);
                }
            }
        }
    }
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

/**
 * @brief Returns the content rect available for Panes given a tab-bar depthh
 *
 * Reads window dimensions from AppState (SSOT) and applies the
 * orientation-appropriate trim for the given @p depth.
 *
 * @param tabBarDepth  Tab-bar pixel depth to subtract from the base bounds.
 * @return Pixel rect available for the active Panes component.
 * @note MESSAGE THREAD.
 */
juce::Rectangle<int> Tabs::computeContentRect (int tabBarDepth) const noexcept
{
    const auto* ctx { AppState::getContext() };
    const juce::Rectangle<int> base { 0, 0, ctx->getWindowWidth(), ctx->getWindowHeight() };

    juce::Rectangle<int> result { base };

    if (tabBarDepth > 0)
    {
        const auto orientation { getOrientation() };

        if (orientation == juce::TabbedButtonBar::TabsAtTop)
            result = base.withTrimmedTop (tabBarDepth);
        else if (orientation == juce::TabbedButtonBar::TabsAtBottom)
            result = base.withTrimmedBottom (tabBarDepth);
        else if (orientation == juce::TabbedButtonBar::TabsAtLeft)
            result = base.withTrimmedLeft (tabBarDepth);
        else if (orientation == juce::TabbedButtonBar::TabsAtRight)
            result = base.withTrimmedRight (tabBarDepth);
    }

    return result;
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Terminal
