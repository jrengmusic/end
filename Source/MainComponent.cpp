/**
 * @file MainComponent.cpp
 * @brief Implementation of the root application content component.
 *
 * Constructs the `Terminal::Tabs` container, sets the initial window size from
 * persisted state, and registers a close callback so that window dimensions
 * are saved when the native close button is pressed.
 *
 * Owns `Action::Registry` and registers all user-performable action callbacks
 * via `registerActions()`.
 *
 * @see MainComponent
 * @see Terminal::Tabs
 * @see Config
 * @see Action::Registry
 */

/*
  ==============================================================================

    END - Ephemeral Nexus Display
    Main application component

    MainComponent.cpp - Main application content component

  ==============================================================================
*/

#include "MainComponent.h"
#include "nexus/Session.h"
#include "terminal/data/Identifier.h"

/**
 * @brief Constructs MainComponent.
 *
 * Member init order (declared in header):
 * - `config`, `appState` — cached context references
 * - `action` — global action registry (constructed after Config)
 * - `terminalLookAndFeel` — application-wide LookAndFeel
 * - `glRenderer` — OpenGL renderer
 * - `fonts` — global font context (from config)
 *
 * Constructor body:
 * 1. `setOpaque(false)` — tells JUCE the component has transparency.
 * 2. `initialiseTabs()` — creates Tabs, attaches GL renderer, adds first tab.
 * 3. `initialiseMessageOverlay()` — creates overlay, shows startup errors.
 * 4. `registerActions()` — wires all action callbacks into `action`, sets
 *    BackgroundBlur close callback.
 * 5. `setSize()` — reads window dimensions from AppState.
 * 6. `setDefaultLookAndFeel()` — applies terminalLookAndFeel to all children.
 *
 * @note MESSAGE THREAD — called from ENDApplication::initialise().
 */
MainComponent::MainComponent (jreng::Typeface::Registry& fontRegistry)
    : typeface (fontRegistry,
                config.getString (Config::Key::fontFamily),
                config.getFloat (Config::Key::fontSize),
                jreng::Glyph::AtlasSize::compact,
                true)
    , whelmedBodyFont (fontRegistry,
                       Whelmed::Config::getContext()->getString (Whelmed::Config::Key::fontFamily),
                       Whelmed::Config::getContext()->getFloat (Whelmed::Config::Key::fontSize),
                       jreng::Glyph::AtlasSize::standard,
                       false)
    , whelmedCodeFont (fontRegistry,
                       Whelmed::Config::getContext()->getString (Whelmed::Config::Key::codeFamily),
                       Whelmed::Config::getContext()->getFloat (Whelmed::Config::Key::codeSize),
                       jreng::Glyph::AtlasSize::standard,
                       true)
{
    setOpaque (appState.getRendererType() == App::RendererType::cpu);

    //==============================================================================
    initialiseOverlays();
    //==============================================================================

    nexusNode = appState.getNexusNode();
    nexusNode.addListener (this);

    // Session is constructed AFTER MainComponent in both modes, so listeners are live
    // before any tree mutations occur.
    // Local mode: Session ctor creates the PROCESSORS node → fires
    //   valueTreeChildAdded(nexusNode, PROCESSORS) → walker triggered.
    // Client mode: When Message::processorList arrives, Client rewrites the PROCESSORS subtree →
    //   fires valueTreeChildAdded(nexusNode, PROCESSORS) → walker triggered.
    // processorsNode is assigned lazily inside valueTreeChildAdded
    // when PROCESSORS is created under nexusNode.  No getOrCreate here —
    // premature creation would prevent the child-added events from firing.

    setLookAndFeel (&terminalLookAndFeel);
    juce::LookAndFeel::setDefaultLookAndFeel (&terminalLookAndFeel);
    setSize (appState.getWindowWidth(), appState.getWindowHeight());
    //==============================================================================
    applyConfig();
}

void MainComponent::onNexusConnected()
{
    if (tabs == nullptr)
    {
        initialiseTabs();
        resized();
    }
}

void MainComponent::applyConfig()
{
    registerActions();

    appState.setRendererType (config.getString (Config::Key::gpuAcceleration));
    setRenderer (appState.getRendererType());

    if (tabs != nullptr)
    {
        tabs->applyConfig();
        tabs->applyOrientation();
    }

    terminalLookAndFeel.setColours();
    sendLookAndFeelChange();
}

void MainComponent::setRenderer (App::RendererType rendererType)
{
    const bool isUsingGpu { rendererType == App::RendererType::gpu };
    const auto atlasSize { isUsingGpu ? jreng::Glyph::AtlasSize::standard : jreng::Glyph::AtlasSize::compact };
    typeface.setAtlasSize (atlasSize);

    glRenderer.detach();
    glRenderer.setComponentPaintingEnabled (true);

    if (isUsingGpu)
    {
        glRenderer.attachTo (*this);
    }

    setOpaque (not isUsingGpu);

    if (tabs != nullptr)
    {
        tabs->switchRenderer (rendererType);
    }
}

void MainComponent::paint (juce::Graphics& g)
{
    if (isOpaque())
    {
        g.fillAll (findColour (juce::ResizableWindow::backgroundColourId));
    }
}

void MainComponent::resized()
{
    if (tabs != nullptr)
        tabs->setBounds (getLocalBounds());

    if (auto* window { dynamic_cast<jreng::Window*> (getTopLevelComponent()) }; window != nullptr)
    {
        if (window->isUserResizing())
            showMessageOverlay();
    }

    // Position status bar overlay: full-width at configured edge.
    // No space is reserved when hidden — the bar overlays the terminal area.
    {
        const juce::String position { config.getString (Config::Key::keysStatusBarPosition) };
        const int barHeight { statusBarOverlay->getPreferredHeight() };
        const int y { (position == "top") ? 0 : getHeight() - barHeight };
        statusBarOverlay->setBounds (0, y, getWidth(), barHeight);
    }
}

/**
 * @brief Clears the BackgroundBlur close callback.
 *
 * Passing `nullptr` prevents the lambda (which captures `this`) from being
 * invoked after this component has been destroyed.
 *
 * @note MESSAGE THREAD — called during window teardown.
 */
MainComponent::~MainComponent()
{
    if (processorsNode.isValid())
        processorsNode.removeListener (this);

    nexusNode.removeListener (this);
    setLookAndFeel (nullptr);
    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    jreng::BackgroundBlur::setCloseCallback (nullptr);
    glRenderer.detach();
}

/**
 * @brief Fires when a property on a listened ValueTree node changes.
 *
 * All connection and loading signals are now driven by child-add/remove events.
 * This override is retained as required by the ValueTree::Listener interface.
 *
 * @note MESSAGE THREAD.
 */
void MainComponent::valueTreePropertyChanged (juce::ValueTree& /*tree*/, const juce::Identifier& /*property*/) {}

/**
 * @brief Fires when a direct child is added to nexusNode or processorsNode.
 *
 * - parent == nexusNode and child type == PROCESSORS → PROCESSORS node arrived (both modes), call onNexusConnected.
 *
 * @note MESSAGE THREAD.
 */
void MainComponent::valueTreeChildAdded (juce::ValueTree& parent, juce::ValueTree& child)
{
    if (parent == nexusNode and child.getType() == App::ID::PROCESSORS)
    {
        processorsNode = child;
        processorsNode.addListener (this);
        onNexusConnected();
    }
}

/**
 * @brief Fires when a direct child is removed from a listened node.
 *
 * @note MESSAGE THREAD.
 */
void MainComponent::valueTreeChildRemoved (juce::ValueTree& /*parent*/, juce::ValueTree& /*child*/, int /*index*/) {}

/**
 * @brief Registers all user-performable actions with `Action::Registry`.
 *
 * Clears existing actions, delegates to grouped register* methods, then
 * rebuilds the key map and sets the BackgroundBlur close callback.
 *
 * @note MESSAGE THREAD.
 * @see Action::Registry
 */
void MainComponent::registerActions()
{
    auto& action { *Action::Registry::getContext() };
    action.clear();

    registerEditActions (action);
    registerApplicationActions (action);
    registerTabActions (action);
    registerPaneActions (action);
    registerNavigationActions (action);
    registerPopupActions (action);

    //==============================================================================
    action.buildKeyMap();

    //==============================================================================
    jreng::BackgroundBlur::setCloseCallback (
        [this]()
        {
            appState.setWindowSize (getWidth(), getHeight());
        });
}

/**
 * @brief Returns the pixel rect available for terminal panes after subtracting chrome.
 *
 * Subtracts: title bar (when windowButtons is enabled) and tab bar (when tabCount > 1).
 * Tab bar depth and orientation are derived from LookAndFeel and the configured tab position.
 * Terminal padding is NOT subtracted here — that is done inside Panes::cellsFromRect.
 *
 * @param windowWidth   Window pixel width.
 * @param windowHeight  Window pixel height.
 * @param tabCount      Number of tabs (determines tab bar visibility).
 * @return Content rect for the panes area.
 * @note MESSAGE THREAD.
 */
juce::Rectangle<int> MainComponent::getContentRect (int windowWidth, int windowHeight, int tabCount) const noexcept
{
    // windowWidth/windowHeight are MainComponent bounds (content area) —
    // native title bar is already excluded by JUCE's setUsingNativeTitleBar.
    auto content { juce::Rectangle<int> (0, 0, windowWidth, windowHeight) };

    const int tabBarDepth { (tabCount > 1) ? Terminal::LookAndFeel::getTabBarHeight() : 0 };

    if (tabBarDepth > 0)
    {
        const auto orientation { Terminal::Tabs::orientationFromString (config.getString (Config::Key::tabPosition)) };

        if (orientation == juce::TabbedButtonBar::TabsAtTop)
            content = content.withTrimmedTop (tabBarDepth);
        else if (orientation == juce::TabbedButtonBar::TabsAtBottom)
            content = content.withTrimmedBottom (tabBarDepth);
        else if (orientation == juce::TabbedButtonBar::TabsAtLeft)
            content = content.withTrimmedLeft (tabBarDepth);
        else if (orientation == juce::TabbedButtonBar::TabsAtRight)
            content = content.withTrimmedRight (tabBarDepth);
    }

    return content;
}

/**
 * @brief Computes grid dimensions from font metrics and window bounds, displays "cols * rows" overlay.
 * @note MESSAGE THREAD — called from resized().
 * @see MessageOverlay
 * @see fonts
 */
void MainComponent::showMessageOverlay()
{
    if (messageOverlay != nullptr)
    {
        messageOverlay->setBounds (getLocalBounds());

        const auto fm { typeface.calcMetrics (Terminal::Display::dpiCorrectedFontSize()) };

        if (fm.isValid())
        {
            auto content { getLocalBounds() };
            const int depth { tabs != nullptr ? tabs->getTabBarDepth() : 0 };
            const auto orientation { tabs != nullptr ? tabs->getOrientation() : juce::TabbedButtonBar::TabsAtLeft };

            if (orientation == juce::TabbedButtonBar::TabsAtTop)
                content = content.withTrimmedTop (depth);
            else if (orientation == juce::TabbedButtonBar::TabsAtBottom)
                content = content.withTrimmedBottom (depth);
            else if (orientation == juce::TabbedButtonBar::TabsAtLeft)
                content = content.withTrimmedLeft (depth);
            else if (orientation == juce::TabbedButtonBar::TabsAtRight)
                content = content.withTrimmedRight (depth);

            const int titleBarHeight { config.getBool (Config::Key::windowButtons) ? App::titleBarHeight : 0 };
            const int padTop { config.getInt (Config::Key::terminalPaddingTop) };
            const int padRight { config.getInt (Config::Key::terminalPaddingRight) };
            const int padBottom { config.getInt (Config::Key::terminalPaddingBottom) };
            const int padLeft { config.getInt (Config::Key::terminalPaddingLeft) };

            content.removeFromTop (titleBarHeight);
            content.removeFromTop (padTop);
            content.removeFromRight (padRight);
            content.removeFromBottom (padBottom);
            content.removeFromLeft (padLeft);

            const int cols { content.getWidth() / fm.logicalCellW };
            const int rows { content.getHeight() / fm.logicalCellH };

            if (cols > 0 and rows > 0 and isShowing())
            {
                messageOverlay->showResize (cols, rows, padTop, padRight, padBottom, padLeft);
            }
        }
    }
}

/**
 * @brief Creates Terminal::Tabs, attaches GL renderer, wires repaint callback, restores tabs.
 *
 * Reads the saved tab count from AppState. If tabs were saved, clears the
 * TABS subtree and recreates that many tabs (addNewTab rebuilds the tree).
 * Falls back to one tab if no saved state exists.
 *
 * @note MESSAGE THREAD.
 * @see Terminal::Tabs
 * @see glRenderer
 * @see AppState
 */
void MainComponent::initialiseTabs()
{
    tabs = std::make_unique<Terminal::Tabs> (
        typeface,
        whelmedBodyFont,
        whelmedCodeFont,
        Terminal::Tabs::orientationFromString (config.getString (Config::Key::tabPosition)));
    addAndMakeVisible (tabs.get());
    tabs->setBounds (getLocalBounds());

    glRenderer.setComponentIterator (
        [this] (std::function<void (jreng::GLComponent&)> renderComponent)
        {
            for (auto& pane : tabs->getPanes())
            {
                if (pane->isVisible())
                {
                    renderComponent (*pane);
                }
            }
        });

    tabs->onRepaintNeeded = [this]
    {
        if (auto* terminal { tabs->getActiveTerminal() }; terminal != nullptr)
        {
            statusBarOverlay->updateHintInfo (terminal->getHintPage(), terminal->getHintTotalPages());
            terminal->repaint();
        }

        glRenderer.triggerRepaint();
    };

    // Restore tabs and split layout from end-<id>.state.
    // Works identically in standalone and nexus modes — Session::create routes
    // internally to local or client path; the walker is oblivious to mode.

    // Snapshot the saved TABS tree before clearing it.
    // addNewTab() mutates the live TABS node, so we must capture a deep copy
    // before removeAllChildren. savedSnapshot is detached from the live tree;
    // Tabs::restore walks it directly without aliasing live state.

    const auto savedTabs { appState.getTabs() };
    int savedTabCount { 0 };

    for (int t { 0 }; t < savedTabs.getNumChildren(); ++t)
    {
        if (savedTabs.getChild (t).getType() == App::ID::TAB)
            ++savedTabCount;
    }

    const auto contentRect { getContentRect (appState.getWindowWidth(), appState.getWindowHeight(), savedTabCount) };
    const auto savedSnapshot { savedTabs.createCopy() };

    appState.getTabs().removeAllChildren (nullptr);
    AppState::getContext()->setActivePaneType (App::ID::paneTypeTerminal);

    if (savedTabCount > 0)
    {
        tabs->restore (savedSnapshot, contentRect);
    }
    else
    {
        tabs->addNewTab();
    }

    AppState::getContext()->setActiveTabIndex (0);

    sendLookAndFeelChange();
}

/**
 * @brief Exits selection mode on the active terminal if it is currently modal.
 * @note MESSAGE THREAD.
 * @see Terminal::Display::exitSelectionMode
 */
void MainComponent::exitActiveTerminalSelectionMode() noexcept
{
    if (auto* terminal { tabs->getActiveTerminal() }; terminal != nullptr)
    {
        if (terminal->isInSelectionMode())
            terminal->exitSelectionMode();
    }
}

/**
 * @brief Creates MessageOverlay, shows startup errors if any.
 * @note MESSAGE THREAD.
 * @see MessageOverlay
 * @see Config::getLoadError()
 */
void MainComponent::initialiseOverlays()
{
    messageOverlay = std::make_unique<MessageOverlay>();
    statusBarOverlay = std::make_unique<StatusBarOverlay> (appState.getContext()->getTabs());

    addChildComponent (messageOverlay.get());
    addChildComponent (statusBarOverlay.get());

    if (const auto& startupError { config.getLoadError() }; startupError.isNotEmpty())
    {
        juce::MessageManager::callAsync (
            [this, error = startupError]
            {
                messageOverlay->showMessage (error);
            });
    }
}
