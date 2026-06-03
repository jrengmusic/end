/**
 * @file MainComponent.cpp
 * @brief Implementation of the root application content component.
 *
 * Constructs the `terminal::Tabs` container, sets the initial window size from
 * persisted state, and registers a close callback so that window dimensions
 * are saved when the native close button is pressed.
 *
 * Owns `action::Registry` and registers all user-performable action callbacks
 * via `registerActions()`.
 *
 * @see MainComponent
 * @see terminal::Tabs
 * @see Config
 * @see action::Registry
 */

/*
  ==============================================================================

    END - Ephemeral Nexus Display
    Main application component

    MainComponent.cpp - Main application content component

  ==============================================================================
*/

#include "MainComponent.h"

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
 * 3. `setSize()` — reads window dimensions from AppModel.
 * 4. `setDefaultLookAndFeel()` — applies terminalLookAndFeel to all children.
 *
 * Note: `applyConfig()` is NOT called from the ctor. It is called from
 * Main.cpp after `jam::Window` is constructed. `setRenderer` is deferred
 * further — it runs at the end of `initialiseTabs()`, after all Displays
 * and their Screens are in the component tree, so that `glContextCreated`
 * fires on all Screens when the GL context is attached.
 *
 * @note MESSAGE THREAD — called from ENDApplication::initialise().
 */
MainComponent::MainComponent()
{
    {
        const juce::String fontFamily { appState.getValue<juce::String> (app::id::DISPLAY_LUA, app::id::fontFamily) };
        const float fontSize { appState.dpiCorrectedFontSize() };

        auto typeface { std::make_unique<jam::Typeface> (fontFamily,
#if JUCE_MAC
                                                         "Apple Color Emoji",
#elif JUCE_WINDOWS
                                                         "Segoe UI Emoji",
#else
                                                         "Noto Color Emoji",
#endif
                                                         fontSize) };

        // Style variant — font metadata declares bold.
        typeface->registerStyleFont (jam::fonts::DisplayMonoBold_ttf, jam::fonts::DisplayMonoBold_ttfSize);

        // Display Mono Book as first fallback — wins PUA codepoint resolution (E000/E001 branding).
        typeface->addFallbackFont (jam::fonts::DisplayMonoBook_ttf, jam::fonts::DisplayMonoBook_ttfSize);

        const auto [nfData, nfSize] { BinaryData::fetcher ("SymbolsNerdFont-Regular.ttf") };
        typeface->addFallbackFont (nfData, nfSize);

        jam::Typeface::registerTypeface (fontFamily, std::move (typeface));
    }

    setOpaque (appState.getRendererType() == app::RendererType::cpu);

    //==============================================================================
    initialiseOverlays();
    //==============================================================================

    nexusNode = appState.getNexusNode();
    nexusNode.addListener (this);
    appState.getWindow().addListener (this);
    configNode = appState.getConfig();
    configNode.addListener (this);

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
    // Wire display and popup callbacks through AppModel delegation.
    lua::Engine::DisplayCallbacks displayCb;
    displayCb.splitHorizontal = [this]
    {
        tabs->splitHorizontal();
    };
    displayCb.splitVertical = [this]
    {
        tabs->splitVertical();
    };
    displayCb.splitWithRatio = [this] (const juce::String& dir, bool isVert, double ratio)
    {
        tabs->splitActiveWithRatio (dir, isVert, ratio);
    };
    displayCb.newTab = [this]
    {
        tabs->addNewTab();
    };
    displayCb.closeTab = [this]
    {
        tabs->closeActiveTab();
    };
    displayCb.nextTab = [this]
    {
        tabs->selectNextTab();
    };
    displayCb.prevTab = [this]
    {
        tabs->selectPreviousTab();
    };
    displayCb.focusPane = [this] (int dx, int dy)
    {
        if (dx < 0)
            tabs->focusPaneLeft();
        else if (dx > 0)
            tabs->focusPaneRight();
        else if (dy < 0)
            tabs->focusPaneUp();
        else if (dy > 0)
            tabs->focusPaneDown();
    };
    displayCb.closePane = [this]
    {
        tabs->closeActiveTab();
    };
    displayCb.renameTab = [this] (const juce::String& name)
    {
        tabs->renameActiveTab (name);
    };

    lua::Engine::PopupCallbacks popupCb;
    popupCb.launchPopup = [this] (const juce::String& /*name*/,
                                  const juce::String& command,
                                  const juce::String& args,
                                  const juce::String& cwd,
                                  cell popupCols,
                                  cell popupRows)
    {
        if (not popup.isActive())
        {
            const auto shell { appState.getValue<juce::String> (app::id::NEXUS_LUA, app::id::shellProgram) };
            const auto configShellArgs { appState.getValue<juce::String> (app::id::NEXUS_LUA, app::id::shellArgs) };
            auto shellArgs { (configShellArgs.isNotEmpty() ? configShellArgs + " " : juce::String()) + "-c " + command
                             + (args.isNotEmpty() ? " " + args : "") };

            const cell cols { popupCols.value > 0 ? popupCols
                                                   : cell (appState.getValue<int> (app::id::POPUPS_LUA, app::id::defaultCols)) };
            const cell rows { popupRows.value > 0 ? popupRows
                                                  : cell (appState.getValue<int> (app::id::POPUPS_LUA, app::id::defaultRows)) };

            const float fontSize { appState.dpiCorrectedFontSize() };
            const juce::String fontFamily { appState.getValue<juce::String> (app::id::DISPLAY_LUA, app::id::fontFamily) };
            const float cellWidth  { appState.getValue<float> (app::id::DISPLAY_LUA, app::id::cellWidth) };
            const float lineHeight { appState.getValue<float> (app::id::DISPLAY_LUA, app::id::lineHeight) };
            const jam::Font font { fontFamily, fontSize, cellWidth, lineHeight };
            jassert (font.cellWidth > 0 and font.cellHeight > 0);

            const bool hasButtons { appState.getValue<int> (app::id::DISPLAY_LUA, app::id::windowButtons) != 0 };
            const int titleBarHeight { hasButtons ? app::titleBarHeight : 0 };
            const int paddingTop    { appState.getValue<int> (app::id::NEXUS_LUA, app::id::paddingTop) };
            const int paddingRight  { appState.getValue<int> (app::id::NEXUS_LUA, app::id::paddingRight) };
            const int paddingBottom { appState.getValue<int> (app::id::NEXUS_LUA, app::id::paddingBottom) };
            const int paddingLeft   { appState.getValue<int> (app::id::NEXUS_LUA, app::id::paddingLeft) };

            const auto cellPx { jam::Cell::Rectangle (cols, rows).toPixel (font.cellWidth, font.cellHeight) };
            const int pixelWidth  { cellPx.getWidth()  + paddingLeft + paddingRight };
            const int pixelHeight { cellPx.getHeight() + paddingTop + paddingBottom + titleBarHeight };

            const auto effectiveCwd { cwd.isNotEmpty() ? cwd : appState.getPwd() };

            auto termSession { terminal::Session::create (
                effectiveCwd, jam::Cell::Rectangle (cols, rows), shell, shellArgs) };
            auto* sessionPtr { termSession.get() };

            auto terminal { std::make_unique<terminal::Display> (*termSession) };
            terminal->setComponentID (termSession->getProcessor().getUuid());

            popup.show (*this, std::move (terminal), pixelWidth, pixelHeight);
            popup.setTerminalSession (std::move (termSession));

            // Open the TTY after Display is in the component hierarchy —
            // resized() fires with real bounds, screen nodes are grafted,
            // all atomics exist before the reader thread starts.
            sessionPtr->start();
        }
    };

    appState.setDisplayCallbacks (std::move (displayCb));
    appState.setPopupCallbacks (std::move (popupCb));
    appState.registerApiTable();
}

void MainComponent::setRenderer (app::RendererType rendererType)
{
    const bool isUsingGpu { rendererType == app::RendererType::gpu };
    const auto atlasSize { isUsingGpu ? jam::glyph::AtlasSize::standard
                                      : jam::glyph::AtlasSize::compact };// AtlasSize stays in jam::glyph
    jam::Typeface::setAtlasSize (atlasSize);

    // Always detach first — setOpenGLVersionRequired/setComponentPaintingEnabled
    // require the context to not be attached
    openGLContext.detach();

    if (isUsingGpu)
    {
        openGLContext.setComponentPaintingEnabled (true);
        openGLContext.setContinuousRepainting (false);
        openGLContext.setRenderer (this);
        openGLContext.attachTo (*this);
    }

    setOpaque (not isUsingGpu);
}

void MainComponent::newOpenGLContextCreated() { jam::BackgroundBlur::enableWindowTransparency(); }
void MainComponent::renderOpenGL() { juce::OpenGLHelpers::clear (juce::Colours::transparentBlack); }
void MainComponent::openGLContextClosing() {}

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
 * dragging the window border (`terminal::Window::isUserResizing()`).
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

    if (auto* window { dynamic_cast<terminal::Window*> (getTopLevelComponent()) }; window != nullptr)
    {
        if (window->isUserResizing())
            showMessageOverlay();
    }

    // Position status bar overlay: full-width at configured edge.
    // No space is reserved when hidden — the bar overlays the terminal area.
    {
        const juce::String position { appState.getValue<juce::String> (app::id::DISPLAY_LUA, app::id::statusBarPosition) };
        const int barHeight { statusBarOverlay->getPreferredHeight() };
        const int y { (position == Map::Position::getContext()->get (Map::Position::top)) ? 0
                                                                                          : getHeight() - barHeight };
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
    openGLContext.detach();

    if (sessionsNode.isValid())
        sessionsNode.removeListener (this);

    nexusNode.removeListener (this);
    appState.getWindow().removeListener (this);
    configNode.removeListener (this);
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
    if (tree.getType() == app::id::WINDOW)
    {
        if (property == app::id::fontFamily)
        {
            const juce::String fontFamily { appState.getValue<juce::String> (app::id::DISPLAY_LUA, app::id::fontFamily) };
            auto* typeface { jam::Typeface::findTypeface (fontFamily) };

            if (typeface != nullptr)
                typeface->setFontFamily (fontFamily);

            appState.markAtlasDirty();
        }
        else if (property == app::id::fontSize)
        {
            const juce::String fontFamily { appState.getValue<juce::String> (app::id::DISPLAY_LUA, app::id::fontFamily) };
            auto* typeface { jam::Typeface::findTypeface (fontFamily) };

            if (typeface != nullptr)
                typeface->setFontSize (appState.dpiCorrectedFontSize());

            appState.markAtlasDirty();
        }
        else if (property == app::id::renderer)
        {
            appState.markAtlasDirty();
        }
    }
    else if (tree == configNode and property == app::id::configGeneration)
    {
        appState.setRendererType (appState.getValue<juce::String> (app::id::NEXUS_LUA, app::id::gpu));

        registerActions();
        showReloadMessage();

        if (auto* window { findParentComponentOfClass<terminal::Window>() }; window != nullptr)
        {
            window->setGlass (
                juce::Colour (static_cast<juce::uint32> (appState.getValue<int> (app::id::DISPLAY_LUA, app::id::windowColour)))
                    .withAlpha (appState.getValue<float> (app::id::DISPLAY_LUA, app::id::windowOpacity)),
                appState.getValue<float> (app::id::DISPLAY_LUA, app::id::windowBlurRadius));
        }

#if JUCE_WINDOWS
        if (isWindows11() and appState.getRendererType() == app::RendererType::cpu)
        {
            jam::BackgroundBlur::applyForceEffectRegistry (
                appState.getValue<int> (app::id::DISPLAY_LUA, app::id::windowForceDwm) != 0);
        }
#endif

        if (tabs != nullptr)
        {
            setRenderer (appState.getRendererType());
            tabs->applyOrientation();
        }

        terminalLookAndFeel.setColours();
        sendLookAndFeelChange();
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
    if (parent == nexusNode and child.getType() == app::id::SESSIONS)
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
 * @brief Registers all user-performable actions with `action::Registry`.
 *
 * Clears existing actions, delegates to grouped register* methods, then
 * rebuilds the key map.
 *
 * @note MESSAGE THREAD.
 * @see action::Registry
 */
void MainComponent::registerActions()
{
    auto& action { *action::Registry::getContext() };
    action.clear();

    registerEditActions (action);
    registerApplicationActions (action);
    registerTabActions (action);
    registerPaneActions (action);
    registerNavigationActions (action);

    // Register popup + custom Lua actions from actions.lua and popups.lua.
    appState.registerActions (action);

    // Build key maps from keys.lua bindings.
    appState.buildKeyMap (action);
}

juce::Rectangle<int> MainComponent::getContentRect (int windowWidth, int windowHeight, int tabCount) const noexcept
{
    // windowWidth/windowHeight are MainComponent bounds (content area) —
    // native title bar is already excluded by JUCE's setUsingNativeTitleBar.
    auto content { juce::Rectangle<int> (0, 0, windowWidth, windowHeight) };

    const int tabBarDepth { (tabCount > 1) ? terminal::LookAndFeel::getTabBarHeight() : 0 };

    if (tabBarDepth > 0)
    {
        const auto orientation { terminal::Tabs::orientationFromString (
            appState.getValue<juce::String> (app::id::DISPLAY_LUA, app::id::tabPosition)) };

        if (orientation == jam::TabbedButtonBar::TabsAtTop)
            content = content.withTrimmedTop (tabBarDepth);
        else if (orientation == jam::TabbedButtonBar::TabsAtBottom)
            content = content.withTrimmedBottom (tabBarDepth);
        else if (orientation == jam::TabbedButtonBar::TabsAtLeft)
            content = content.withTrimmedLeft (tabBarDepth);
        else if (orientation == jam::TabbedButtonBar::TabsAtRight)
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
        const float fontSize { appState.dpiCorrectedFontSize() };
        const float cellWidth  { appState.getValue<float> (app::id::DISPLAY_LUA, app::id::cellWidth) };
        const float lineHeight { appState.getValue<float> (app::id::DISPLAY_LUA, app::id::lineHeight) };
        const jam::Font font { appState.getValue<juce::String> (app::id::DISPLAY_LUA, app::id::fontFamily), fontSize, cellWidth, lineHeight };

        if (font.cellWidth > 0)
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

            const bool hasButtons { appState.getValue<int> (app::id::DISPLAY_LUA, app::id::windowButtons) != 0 };
            const int titleBarHeight { hasButtons ? app::titleBarHeight : 0 };
            const int padTop    { appState.getValue<int> (app::id::NEXUS_LUA, app::id::paddingTop) };
            const int padRight  { appState.getValue<int> (app::id::NEXUS_LUA, app::id::paddingRight) };
            const int padBottom { appState.getValue<int> (app::id::NEXUS_LUA, app::id::paddingBottom) };
            const int padLeft   { appState.getValue<int> (app::id::NEXUS_LUA, app::id::paddingLeft) };

            content.removeFromTop (titleBarHeight);
            content.removeFromTop (padTop);
            content.removeFromRight (padRight);
            content.removeFromBottom (padBottom);
            content.removeFromLeft (padLeft);

            const auto gridRect { jam::Cell::Rectangle::fromPixel (content, font.cellWidth, font.cellHeight) };
            if (gridRect.isValid() and isShowing())
            {
                messageOverlay->showResize (gridRect, padTop, padRight, padBottom, padLeft);
            }
        }
    }
}

/**
 * @brief Creates terminal::Tabs, wires repaint callback, restores tabs.
 *
 * Reads the saved tab count from AppModel. If tabs were saved, clears the
 * TABS subtree and recreates that many tabs (addNewTab rebuilds the tree).
 * Falls back to one tab if no saved state exists.
 *
 * @note MESSAGE THREAD.
 * @see terminal::Tabs
 * @see AppModel
 */
void MainComponent::initialiseTabs()
{
    tabs = std::make_unique<terminal::Tabs> (
        terminal::Tabs::orientationFromString (
            appState.getValue<juce::String> (app::id::DISPLAY_LUA, app::id::tabPosition)));
    addAndMakeVisible (tabs.get());

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
        if (savedTabs.getChild (t).getType() == app::id::TAB)
            ++savedTabCount;
    }

    const auto contentRect { getContentRect (appState.getWindowWidth(), appState.getWindowHeight(), savedTabCount) };
    const auto savedSnapshot { savedTabs.createCopy() };

    // Remove TAB children only — preserve PARAM children.
    auto tabsNode { appState.getTabs() };

    for (int i { tabsNode.getNumChildren() - 1 }; i >= 0; --i)
    {
        if (tabsNode.getChild (i).getType() == app::id::TAB)
            tabsNode.removeChild (i, nullptr);
    }
    AppModel::getContext()->setActivePaneType (Map::PaneType::getContext()->get (Map::PaneType::terminal));

    if (savedTabCount > 0)
    {
        tabs->restore (savedSnapshot, contentRect);
    }
    else
    {
        tabs->addNewTab();
    }

    AppModel::getContext()->setActiveTabIndex (0);

    setRenderer (appState.getRendererType());

    sendLookAndFeelChange();
}

/**
 * @brief Exits selection mode on the active terminal if it is currently modal.
 * @note MESSAGE THREAD.
 * @see terminal::Display::exitSelectionMode
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
 * @see lua::Engine::getLoadError()
 */
void MainComponent::initialiseOverlays()
{
    messageOverlay = std::make_unique<MessageOverlay>();
    statusBarOverlay = std::make_unique<StatusBarOverlay> (appState.getContext()->getTabs());

    addChildComponent (messageOverlay.get());
    addChildComponent (statusBarOverlay.get());

    if (const auto& startupError { appState.getLoadError() }; startupError.isNotEmpty())
    {
        juce::MessageManager::callAsync (
            [this, error = startupError]
            {
                messageOverlay->showMessage (error);
            });
    }
}
