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
#include "nexus/Nexus.h"
#include "terminal/data/Identifier.h"

/**
 * @brief Constructs MainComponent.
 *
 * Member init order (declared in header):
 * - `config`, `appState` — cached context references
 * - `action` — global action registry (constructed after Config)
 * - `terminalLookAndFeel` — application-wide LookAndFeel
 * - `fonts` — global font context (from config)
 *
 * Constructor body:
 * 1. `setOpaque(false)` — tells JUCE the component has transparency.
 * 2. `initialiseOverlays()` — creates overlays, shows startup errors.
 * 3. `setSize()` — reads window dimensions from AppState.
 * 4. `setDefaultLookAndFeel()` — applies terminalLookAndFeel to all children.
 *
 * Note: `applyConfig()` is NOT called from the ctor. It is called from
 * Main.cpp after `jam::Window` is constructed, so that
 * `dynamic_cast<jam::Window*>(getTopLevelComponent())` inside
 * `setRenderer` resolves to the fully constructed window.
 *
 * @note MESSAGE THREAD — called from ENDApplication::initialise().
 */
MainComponent::MainComponent (jam::Typeface::Registry& fontRegistry)
    : typeface (fontRegistry,
                config.getString (Config::Key::fontFamily),
                config.dpiCorrectedFontSize(),
                jam::Glyph::AtlasSize::compact,
                true)
{
    glyphAtlas.setAtlasSize (jam::Glyph::AtlasSize::compact);

    setOpaque (appState.getRendererType() == App::RendererType::cpu);

    //==============================================================================
    initialiseOverlays();
    //==============================================================================

    nexusNode = appState.getNexusNode();
    nexusNode.addListener (this);
    appState.getWindow().addListener (this);

    // Session is constructed AFTER MainComponent in both modes, so listeners are live
    // before any tree mutations occur.
    // Local mode: Session ctor creates the SESSIONS node → fires
    //   valueTreeChildAdded(nexusNode, SESSIONS) → walker triggered.
    // Client mode: When Message::sessions arrives, Link rewrites the SESSIONS subtree →
    //   fires valueTreeChildAdded(nexusNode, SESSIONS) → walker triggered.
    // sessionsNode is assigned lazily inside valueTreeChildAdded
    // when SESSIONS is created under nexusNode.  No getOrCreate here —
    // premature creation would prevent the child-added events from firing.

    setLookAndFeel (&terminalLookAndFeel);
    juce::LookAndFeel::setDefaultLookAndFeel (&terminalLookAndFeel);
    setSize (appState.getWindowWidth(), appState.getWindowHeight());

    //==============================================================================
    // Construct Scripting::Engine — sole provider of key bindings and popup/custom actions.
    Scripting::Engine::DisplayCallbacks displayCb;
    displayCb.splitHorizontal = [this] { tabs->splitHorizontal(); };
    displayCb.splitVertical   = [this] { tabs->splitVertical(); };
    displayCb.splitWithRatio  = [this] (const juce::String& dir, bool isVert, double ratio)
        { tabs->splitActiveWithRatio (dir, isVert, ratio); };
    displayCb.newTab    = [this] { tabs->addNewTab(); };
    displayCb.closeTab  = [this] { tabs->closeActiveTab(); };
    displayCb.nextTab   = [this] { tabs->selectNextTab(); };
    displayCb.prevTab   = [this] { tabs->selectPreviousTab(); };
    displayCb.focusPane = [this] (int dx, int dy)
    {
        if (dx < 0) tabs->focusPaneLeft();
        else if (dx > 0) tabs->focusPaneRight();
        else if (dy < 0) tabs->focusPaneUp();
        else if (dy > 0) tabs->focusPaneDown();
    };
    displayCb.closePane = [this] { tabs->closeActiveTab(); };

    Scripting::Engine::PopupCallbacks popupCb;
    popupCb.launchPopup = [this] (const juce::String& /*name*/,
                                  const juce::String& command,
                                  const juce::String& args,
                                  const juce::String& cwd,
                                  int popupCols,
                                  int popupRows)
    {
        if (not popup.isActive())
        {
            const auto shell { config.getString (Config::Key::shellProgram) };
            const auto configShellArgs { config.getString (Config::Key::shellArgs) };
            auto shellArgs { (configShellArgs.isNotEmpty() ? configShellArgs + " " : juce::String())
                             + "-c " + command
                             + (args.isNotEmpty() ? " " + args : "") };

            const int cols { popupCols > 0 ? popupCols : config.getInt (Config::Key::popupCols) };
            const int rows { popupRows > 0 ? popupRows : config.getInt (Config::Key::popupRows) };

            const auto fm { typeface.calcMetrics (Config::getContext()->dpiCorrectedFontSize()) };

            const int titleBarHeight { config.getBool (Config::Key::windowButtons) ? App::titleBarHeight : 0 };
            const int paddingTop { config.getInt (Config::Key::terminalPaddingTop) };
            const int paddingRight { config.getInt (Config::Key::terminalPaddingRight) };
            const int paddingBottom { config.getInt (Config::Key::terminalPaddingBottom) };
            const int paddingLeft { config.getInt (Config::Key::terminalPaddingLeft) };

            const float lineHeightMultiplier { config.getFloat (Config::Key::fontLineHeight) };
            const float cellWidthMultiplier { config.getFloat (Config::Key::fontCellWidth) };

            const int effectiveCellW { static_cast<int> (static_cast<float> (fm.logicalCellW)
                                                         * cellWidthMultiplier) };
            const int effectiveCellH { static_cast<int> (static_cast<float> (fm.logicalCellH)
                                                         * lineHeightMultiplier) };

            const int pixelWidth { cols * effectiveCellW + paddingLeft + paddingRight };
            const int pixelHeight { rows * effectiveCellH + paddingTop + paddingBottom + titleBarHeight };

            const auto effectiveCwd { cwd.isNotEmpty() ? cwd : appState.getPwd() };

            auto termSession { Terminal::Session::create (effectiveCwd, cols, rows, shell, shellArgs) };

            termSession->onExit = [this]
            {
                juce::MessageManager::callAsync ([this] { popup.dismiss(); });
            };

            auto terminal { termSession->getProcessor().createDisplay (typeface, glyphAtlas, graphicsAtlas) };

            auto renderer { (appState.getRendererType() == App::RendererType::gpu)
                ? std::unique_ptr<jam::gl::Renderer> { std::make_unique<jam::GLAtlasRenderer> (typeface, glyphAtlas) }
                : nullptr };

            popup.show (*this, std::move (terminal), pixelWidth, pixelHeight, std::move (renderer));
            popup.setTerminalSession (std::move (termSession));
        }
    };

    scriptingEngine = std::make_unique<Scripting::Engine> (std::move (displayCb), std::move (popupCb));
    scriptingEngine->load();

    scriptingEngine->onActionReload = [this]
    {
        scriptingEngine->load();
        registerActions();

        const auto& error { scriptingEngine->getLoadError() };

        if (error.isEmpty())
        {
            const auto rendererType { AppState::getContext()->getRendererType() };
            const juce::String rendererName { rendererType == App::RendererType::gpu
                                                  ? App::ID::rendererGpu.toUpperCase()
                                                  : App::ID::rendererCpu.toUpperCase() };
            messageOverlay->showMessage ("RELOADED (" + rendererName + ")", 1000);
        }
        else
        {
            messageOverlay->showMessage (error);
        }
    };

    scriptingEngine->onConfigReload = [this]
    {
        const auto reloadError { config.reload() };

        if (reloadError.isEmpty())
        {
            const auto rendererType { AppState::getContext()->getRendererType() };
            const juce::String rendererName { rendererType == App::RendererType::gpu
                                                  ? App::ID::rendererGpu.toUpperCase()
                                                  : App::ID::rendererCpu.toUpperCase() };
            messageOverlay->showMessage ("RELOADED (" + rendererName + ")", 1000);
        }
        else
        {
            messageOverlay->showMessage (reloadError);
        }
    };
}

void MainComponent::applyConfig()
{
    registerActions();

    appState.setFontFamily (config.getString (Config::Key::fontFamily));
    appState.setFontSize   (static_cast<float> (config.dpiCorrectedFontSize()));
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
    const auto atlasSize { isUsingGpu ? jam::Glyph::AtlasSize::standard : jam::Glyph::AtlasSize::compact };
    typeface.setAtlasSize (atlasSize);
    glyphAtlas.setAtlasSize (atlasSize);

    if (auto* window { dynamic_cast<jam::Window*> (getTopLevelComponent()) })
    {
        if (isUsingGpu)
        {
            window->setRenderer (std::make_unique<jam::GLAtlasRenderer> (typeface, glyphAtlas));

            window->setRenderables (
                [this] (std::function<void (jam::gl::Component&)> renderComponent)
                {
                    if (appState.consumeAtlasDirty())
                        glyphAtlas.rebuildAtlas();

                    if (tabs != nullptr)
                    {
                        for (auto& pane : tabs->getPanes())
                        {
                            if (pane->isVisible())
                                renderComponent (*pane);
                        }
                    }
                });

            appState.markAtlasDirty();
        }
        else
        {
            window->setRenderer (nullptr);
        }
    }

    setOpaque (not isUsingGpu);

    if (tabs != nullptr)
        tabs->switchRenderer (rendererType);
}

void MainComponent::paint (juce::Graphics& g)
{
    if (isOpaque())
    {
        g.fillAll (findColour (juce::ResizableWindow::backgroundColourId));
    }
}

/**
 * @brief Lays out child components: tabs, messageOverlay, statusBarOverlay.
 *
 * Called by JUCE on every size change (including initial layout).  Assigns
 * full-bounds to `tabs` and `messageOverlay`; positions `statusBarOverlay`
 * at top or bottom edge per `keys.status_bar.position`.  Triggers the
 * `showMessageOverlay()` resize ruler only while the user is actively
 * dragging the window border (`Terminal::Window::isUserResizing()`).
 *
 * @note MESSAGE THREAD.
 */
void MainComponent::resized()
{
    appState.setWindowSize (getWidth(), getHeight());

    if (tabs != nullptr)
        tabs->setBounds (getLocalBounds());

    if (messageOverlay != nullptr)
        messageOverlay->setBounds (getLocalBounds());

    if (auto* window { dynamic_cast<Terminal::Window*> (getTopLevelComponent()) }; window != nullptr)
    {
        if (window->isUserResizing())
            showMessageOverlay();
    }

    // Position status bar overlay: full-width at configured edge.
    // No space is reserved when hidden — the bar overlays the terminal area.
    {
        const juce::String position { config.getString (Config::Key::statusBarPosition) };
        const int barHeight { statusBarOverlay->getPreferredHeight() };
        const int y { (position == "top") ? 0 : getHeight() - barHeight };
        statusBarOverlay->setBounds (0, y, getWidth(), barHeight);
    }
}

/**
 * @brief Removes ValueTree listeners and tears down LookAndFeel.
 *
 * Unregisters from `sessionsNode` (if valid), `nexusNode`, and the window
 * ValueTree node, then clears the component and default LookAndFeel to avoid
 * dangling references during JUCE shutdown.
 *
 * @note MESSAGE THREAD — called during window teardown.
 */
MainComponent::~MainComponent()
{
    if (sessionsNode.isValid())
        sessionsNode.removeListener (this);

    nexusNode.removeListener (this);
    appState.getWindow().removeListener (this);
    setLookAndFeel (nullptr);
    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
}

/**
 * @brief Fires when a property on a listened ValueTree node changes.
 *
 * All connection and loading signals are now driven by child-add/remove events.
 * This override is retained as required by the ValueTree::Listener interface.
 *
 * @note MESSAGE THREAD.
 */
void MainComponent::valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property)
{
    if (tree.getType() == App::ID::WINDOW)
    {
        if (property == App::ID::fontFamily)
        {
            typeface.setFontFamily (appState.getFontFamily());
            appState.markAtlasDirty();
        }
        else if (property == App::ID::fontSize)
        {
            typeface.setFontSize (appState.getFontSize());
            appState.markAtlasDirty();
        }
        else if (property == App::ID::renderer)
        {
            appState.markAtlasDirty();
        }
    }
}

/**
 * @brief Fires when a direct child is added to nexusNode or sessionsNode.
 *
 * - parent == nexusNode and child type == SESSIONS → SESSIONS node arrived (both modes), initialise tabs.
 *
 * @note MESSAGE THREAD.
 */
void MainComponent::valueTreeChildAdded (juce::ValueTree& parent, juce::ValueTree& child)
{
    if (parent == nexusNode and child.getType() == App::ID::SESSIONS)
    {
        sessionsNode = child;
        sessionsNode.addListener (this);

        if (tabs == nullptr)
        {
            initialiseTabs();
            resized();
        }
    }
}

/**
 * @brief Fires when a direct child is removed from a listened node.
 *
 * @note MESSAGE THREAD.
 */
void MainComponent::valueTreeChildRemoved (juce::ValueTree& parent, juce::ValueTree& child, int /*index*/)
{
    if (parent == sessionsNode and tabs != nullptr)
    {
        const juce::String uuid { child.getProperty (jam::ID::id).toString() };

        if (uuid.isNotEmpty())
        {
            tabs->closeSession (uuid);

            if (tabs->getTabCount() == 0)
                juce::JUCEApplication::getInstance()->systemRequestedQuit();
        }
    }
}

/**
 * @brief Registers all user-performable actions with `Action::Registry`.
 *
 * Clears existing actions, delegates to grouped register* methods, then
 * rebuilds the key map.
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

    // Register popup + custom Lua actions from action.lua.
    scriptingEngine->registerActions (action);

    // Build key maps from action.lua bindings (replaces old Config-based buildKeyMap).
    scriptingEngine->buildKeyMap (action);
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
 *
 * Bounds are set by `resized()`; this method only computes ruler content (cols × rows from font
 * metrics) and triggers `MessageOverlay::showResize()`.
 *
 * @note MESSAGE THREAD — called from resized().
 * @see MessageOverlay
 * @see fonts
 */
void MainComponent::showMessageOverlay()
{
    if (messageOverlay != nullptr)
    {
        const auto fm { typeface.calcMetrics (Config::getContext()->dpiCorrectedFontSize()) };

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
 * @brief Creates Terminal::Tabs, wires repaint callback, restores tabs.
 *
 * Reads the saved tab count from AppState. If tabs were saved, clears the
 * TABS subtree and recreates that many tabs (addNewTab rebuilds the tree).
 * Falls back to one tab if no saved state exists.
 *
 * @note MESSAGE THREAD.
 * @see Terminal::Tabs
 * @see AppState
 */
void MainComponent::initialiseTabs()
{
    tabs = std::make_unique<Terminal::Tabs> (
        typeface,
        glyphAtlas,
        graphicsAtlas,
        Terminal::Tabs::orientationFromString (config.getString (Config::Key::tabPosition)));
    addAndMakeVisible (tabs.get());

    tabs->onRepaintNeeded = [this]
    {
        if (auto* terminal { tabs->getActiveTerminal() }; terminal != nullptr)
        {
            statusBarOverlay->updateHintInfo (terminal->getHintPage(), terminal->getHintTotalPages());
        }

        if (auto* window { dynamic_cast<jam::Window*> (getTopLevelComponent()) })
            window->triggerRepaint();
    };

    // Restore tabs and split layout from `<uuid>.display` when present (daemon client mode).
    // Session::create routes internally to client path; the walker is oblivious to mode.

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
