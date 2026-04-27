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
Tabs::Tabs (jam::Font& font_,
            jam::Glyph::Packer& packer_,
            jam::gl::GlyphAtlas& glAtlas_,
            jam::GraphicsAtlas& graphicsAtlas_,
            Terminal::ImageAtlas& imageAtlas_,
            jam::TabbedButtonBar::Orientation orientation)
    : jam::TabbedComponent (orientation)
    , font (font_)
    , packerRef (packer_)
    , glAtlasRef (glAtlas_)
    , graphicsAtlasRef (graphicsAtlas_)
    , imageAtlasRef (imageAtlas_)
{
    setOpaque (false);
    setTabBarDepth (0);
    setOutline (0);
    juce::Desktop::getInstance().addFocusChangeListener (this);
    tabName.addListener (this);

    getTabbedButtonBar().onTabMoved = [this] (int fromIndex, int toIndex)
    {
        auto movedPane { std::move (panes.at (fromIndex)) };
        panes.erase (panes.begin() + fromIndex);
        panes.insert (panes.begin() + toIndex, std::move (movedPane));

        auto tabsTree { AppState::getContext()->getTabs() };
        tabsTree.moveChild (fromIndex, toIndex, nullptr);

        if (AppState::getContext()->isDaemonMode())
            AppState::getContext()->save();
    };
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

    auto& newPanesPtr { panes.add (std::make_unique<Panes> (font, packerRef, glAtlasRef, graphicsAtlasRef, imageAtlasRef)) };
    auto& newPanes { *newPanesPtr };
    newPanes.onRepaintNeeded = onRepaintNeeded;
    newPanes.onOpenMarkdown = [this] (const juce::File& file)
    {
        if (auto* active { getActivePanes() }; active != nullptr)
            active->createWhelmed (file);
    };
    newPanes.onShowImagePreview = [this] (const juce::Image& img)
    {
        if (onShowImagePreview != nullptr)
            onShowImagePreview (img);
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
    addTab (initialName, juce::Colours::transparentBlack, tabIndex);
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
        const auto index { getCurrentTabIndex() };

        if (index >= 0)
        {
            const auto tabNode { AppState::getContext()->getTab (index) };
            const auto userOverride { tabNode.getProperty (App::ID::userTabName).toString() };
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
    const auto orientation { orientationFromString (lua::Engine::getContext()->display.tab.position) };
    setOrientation (orientation);
    updateTabBarVisibility();
}

jam::TabbedButtonBar::Orientation Tabs::orientationFromString (const juce::String& position)
{
    static const std::unordered_map<juce::String, jam::TabbedButtonBar::Orientation> table {
        { "top",    jam::TabbedButtonBar::TabsAtTop    },
        { "bottom", jam::TabbedButtonBar::TabsAtBottom },
        { "right",  jam::TabbedButtonBar::TabsAtRight  },
        { "left",   jam::TabbedButtonBar::TabsAtLeft   },
    };

    jam::TabbedButtonBar::Orientation result { jam::TabbedButtonBar::TabsAtLeft };
    const auto it { table.find (position) };

    if (it != table.end())
        result = it->second;

    return result;
}

/**
 * @brief Returns the content rect available for Panes given a tab-bar depth.
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
        auto tabNode { AppState::getContext()->getTab (index) };

        if (name.isEmpty())
            tabNode.removeProperty (App::ID::userTabName, nullptr);
        else
            tabNode.setProperty (App::ID::userTabName, name, nullptr);

        const auto displayName { name.isNotEmpty() ? name : tabName.toString() };

        if (displayName.isNotEmpty())
            getTabbedButtonBar().setTabName (index, displayName);

        if (AppState::getContext()->isDaemonMode())
            AppState::getContext()->save();
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
        AppState::getContext()->setActivePaneID (lastPane->getComponentID());

        if (lastPane->isShowing())
            lastPane->grabKeyboardFocus();
    }
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Terminal
