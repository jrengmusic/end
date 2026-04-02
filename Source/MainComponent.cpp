/**
 * @file MainComponent.cpp
 * @brief Implementation of the root application content component.
 *
 * Constructs the `Terminal::Tabs` container, sets the initial window size from
 * persisted state, and registers a close callback so that window dimensions
 * are saved when the native close button is pressed.
 *
 * Owns `Terminal::Action` and registers all user-performable action callbacks
 * via `registerActions()`.
 *
 * @see MainComponent
 * @see Terminal::Tabs
 * @see Config
 * @see Terminal::Action
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
                config.getString (Config::Key::gpuAcceleration) != "false" ? jreng::Glyph::AtlasSize::standard
                                                                           : jreng::Glyph::AtlasSize::compact,
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
    setOpaque (false);

    // Store resolved renderer in AppState. The ValueTree listener handles
    // GL lifecycle, atlas resize, and terminal switching.
    const auto gpuSetting { config.getString (Config::Key::gpuAcceleration) };
    appState.setRendererType (gpuSetting == "false" ? "cpu" : "gpu");

    // Retain the WINDOW subtree reference and listen for renderer changes.
    windowState = appState.getWindow();
    windowState.addListener (this);

    //==============================================================================
    initialiseTabs();
    initialiseMessageOverlay();
    addChildComponent (statusBarOverlay);
    //==============================================================================

    setSize (appState.getWindowWidth(), appState.getWindowHeight());
    setLookAndFeel (&terminalLookAndFeel);
    juce::LookAndFeel::setDefaultLookAndFeel (&terminalLookAndFeel);
    //==============================================================================
    applyConfig();
    // Apply initial renderer state. The listener won't fire when appState
    // already holds the same value, so call directly to set up GL and atlas.
    valueTreePropertyChanged (windowState, App::ID::renderer);
}

void MainComponent::applyConfig()
{
    registerActions();

    // Config → AppState. The ValueTree listener handles GL lifecycle + terminals.
    const auto gpuSetting { config.getString (Config::Key::gpuAcceleration) };
    appState.setRendererType (gpuSetting == "false" ? "cpu" : "gpu");

    if (tabs != nullptr)
    {
        tabs->applyConfig();
        tabs->applyOrientation();
    }

    terminalLookAndFeel.setColours();
    sendLookAndFeelChange();
}

void MainComponent::valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property)
{
    juce::ignoreUnused (tree);

    if (property == App::ID::renderer)
    {
        const auto rendererType { getRendererType() };

        // Shared atlas: resize once for all terminals.
        const auto atlasSize { rendererType == PaneComponent::RendererType::gpu ? jreng::Glyph::AtlasSize::standard
                                                                                : jreng::Glyph::AtlasSize::compact };
        typeface.setAtlasSize (atlasSize);

        // GL lifecycle: detach first (required for setComponentPaintingEnabled).
        glRenderer.detach();
        glRenderer.setComponentPaintingEnabled (true);

        if (rendererType == PaneComponent::RendererType::gpu)
        {
            glRenderer.attachTo (*this);
            setOpaque (false);
        }
        else
        {
            setOpaque (true);
        }

        // Switch all terminal instances.
        if (tabs != nullptr)
        {
            tabs->switchRenderer (rendererType);
        }
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

    showMessageOverlay();

    // Position status bar overlay: full-width at configured edge.
    // No space is reserved when hidden — the bar overlays the terminal area.
    {
        const juce::String position { config.getString (Config::Key::keysStatusBarPosition) };
        const int barHeight { statusBarOverlay.getPreferredHeight() };
        const int y { (position == "top") ? 0 : getHeight() - barHeight };
        statusBarOverlay.setBounds (0, y, getWidth(), barHeight);
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
    windowState.removeListener (this);
    setLookAndFeel (nullptr);
    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    jreng::BackgroundBlur::setCloseCallback (nullptr);
    glRenderer.detach();
}

/**
 * @brief Registers all user-performable actions with `Terminal::Action`.
 *
 * Wires every action callback into `action` and sets the BackgroundBlur
 * close callback.  Called once from the constructor after `tabs` and
 * `messageOverlay` are fully initialised.
 *
 * ### Copy action
 * Returns `false` when no selection is active so the key falls through to
 * the PTY as `\x03` (SIGINT).  Returns `true` only when a selection was
 * actually copied.
 *
 * ### Reload action
 * Calls `action.reload()` to rebuild the key map from the updated config,
 * then applies the new config to all terminals and the look-and-feel.
 *
 * @note MESSAGE THREAD.
 * @see Terminal::Action
 */
void MainComponent::registerActions()
{
    auto& action { *Terminal::Action::getContext() };
    action.clear();

    action.registerAction ("copy",
                           "Copy",
                           "Copy selection to clipboard",
                           "Edit",
                           false,
                           [this]() -> bool
                           {
                               bool consumed { false };

                               if (tabs->hasSelection())
                               {
                                   tabs->copySelection();
                                   consumed = true;
                               }

                               return consumed;
                           });

    action.registerAction ("paste",
                           "Paste",
                           "Paste from clipboard",
                           "Edit",
                           false,
                           [this]() -> bool
                           {
                               tabs->pasteClipboard();
                               return true;
                           });

    action.registerAction ("quit",
                           "Quit",
                           "Quit application",
                           "Application",
                           false,
                           [this]() -> bool
                           {
                               juce::JUCEApplication::getInstance()->systemRequestedQuit();
                               return true;
                           });

    action.registerAction ("close_tab",
                           "Close Tab",
                           "Close current tab",
                           "Tabs",
                           false,
                           [this]() -> bool
                           {
                               tabs->closeActiveTab();

                               if (tabs->getTabCount() == 0)
                                   juce::JUCEApplication::getInstance()->systemRequestedQuit();

                               return true;
                           });

    action.registerAction ("reload_config",
                           "Reload Config",
                           "Reload configuration",
                           "Application",
                           false,
                           [this]() -> bool
                           {
                               const auto reloadError { config.reload() };
                               Whelmed::Config::getContext()->reload();

                               if (reloadError.isEmpty())
                               {
                                   const auto rendererName { AppState::getContext()->getRendererType().toUpperCase() };
                                   messageOverlay->showMessage ("RELOADED (" + rendererName + ")", 1000);
                               }
                               else
                               {
                                   messageOverlay->showMessage (reloadError);
                               }

                               return true;
                           });

    action.registerAction ("zoom_in",
                           "Zoom In",
                           "Increase font size",
                           "View",
                           false,
                           [this]() -> bool
                           {
                               tabs->increaseZoom();
                               return true;
                           });

    action.registerAction ("zoom_out",
                           "Zoom Out",
                           "Decrease font size",
                           "View",
                           false,
                           [this]() -> bool
                           {
                               tabs->decreaseZoom();
                               return true;
                           });

    action.registerAction ("zoom_reset",
                           "Zoom Reset",
                           "Reset font size to default",
                           "View",
                           false,
                           [this]() -> bool
                           {
                               tabs->resetZoom();
                               return true;
                           });

    action.registerAction (
        "new_window",
        "New Window",
        "Open a new terminal window",
        "Window",
        false,
        []() -> bool
        {
            const juce::File app { juce::File::getSpecialLocation (juce::File::currentApplicationFile) };

#if JUCE_MAC
            const juce::String cmd { "open -n \"" + app.getFullPathName() + "\" &" };
            std::system (cmd.toRawUTF8());
#else
            app.startAsProcess();
#endif

            return true;
        });

    action.registerAction ("new_tab",
                           "New Tab",
                           "Open a new terminal tab",
                           "Tabs",
                           false,
                           [this]() -> bool
                           {
                               tabs->addNewTab();
                               return true;
                           });

    action.registerAction ("prev_tab",
                           "Previous Tab",
                           "Switch to previous tab",
                           "Tabs",
                           false,
                           [this]() -> bool
                           {
                               exitActiveTerminalSelectionMode();
                               tabs->selectPreviousTab();
                               return true;
                           });

    action.registerAction ("next_tab",
                           "Next Tab",
                           "Switch to next tab",
                           "Tabs",
                           false,
                           [this]() -> bool
                           {
                               exitActiveTerminalSelectionMode();
                               tabs->selectNextTab();
                               return true;
                           });

    action.registerAction ("split_horizontal",
                           "Split Horizontal",
                           "Split pane horizontally",
                           "Panes",
                           true,
                           [this]() -> bool
                           {
                               tabs->splitHorizontal();
                               return true;
                           });

    action.registerAction ("split_vertical",
                           "Split Vertical",
                           "Split pane vertically",
                           "Panes",
                           true,
                           [this]() -> bool
                           {
                               tabs->splitVertical();
                               return true;
                           });

    action.registerAction ("pane_left",
                           "Focus Left Pane",
                           "Move focus to the left pane",
                           "Panes",
                           true,
                           [this]() -> bool
                           {
                               exitActiveTerminalSelectionMode();
                               tabs->focusPaneLeft();
                               return true;
                           });

    action.registerAction ("pane_down",
                           "Focus Down Pane",
                           "Move focus to the pane below",
                           "Panes",
                           true,
                           [this]() -> bool
                           {
                               exitActiveTerminalSelectionMode();
                               tabs->focusPaneDown();
                               return true;
                           });

    action.registerAction ("pane_up",
                           "Focus Up Pane",
                           "Move focus to the pane above",
                           "Panes",
                           true,
                           [this]() -> bool
                           {
                               exitActiveTerminalSelectionMode();
                               tabs->focusPaneUp();
                               return true;
                           });

    action.registerAction ("pane_right",
                           "Focus Right Pane",
                           "Move focus to the right pane",
                           "Panes",
                           true,
                           [this]() -> bool
                           {
                               exitActiveTerminalSelectionMode();
                               tabs->focusPaneRight();
                               return true;
                           });

    action.registerAction ("newline",
                           "Insert Newline",
                           "Send literal newline (LF) to terminal",
                           "Edit",
                           false,
                           [this]() -> bool
                           {
                               tabs->writeToActivePty ("\n", 1);
                               return true;
                           });

    action.registerAction ("enter_selection",
                           "Enter Selection Mode",
                           "Enter vim-like text selection mode",
                           "Selection",
                           true,
                           [this]() -> bool
                           {
                               if (auto* pane { tabs->getActivePane() }; pane != nullptr)
                                   pane->enterSelectionMode();

                               return true;
                           });

    action.registerAction ("enter_open_file",
                           "Open File",
                           "Enter open-file mode with hint labels",
                           "Navigation",
                           true,
                           [this]() -> bool
                           {
                               if (auto* terminal { tabs->getActiveTerminal() })
                                   terminal->enterOpenFileMode();

                               return true;
                           });

    action.registerAction ("action_list",
                           "Action List",
                           "Open command palette",
                           "Application",
                           true,
                           [this]() -> bool
                           {
                               actionList = std::make_unique<Terminal::ActionList> (*this);

                               return true;
                           });

    action.registerAction (
        "open_markdown",
        "Open Markdown",
        "Open a .md file in a Whelmed pane",
        "Navigation",
        true,
        [this]() -> bool
        {
            auto chooser { std::make_shared<juce::FileChooser> ("Open Markdown File", juce::File {}, "*.md") };

            chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                  [this, chooser] (const juce::FileChooser& fc)
                                  {
                                      const auto result { fc.getResult() };

                                      if (result.existsAsFile())
                                          tabs->openMarkdown (result);
                                  });

            return true;
        });

    // Register popup actions from Config
    for (const auto& pair : config.getPopups())
    {
        const auto& name { pair.first };
        const auto& entry { pair.second };

        auto launchPopup { [this, entry]() -> bool
                           {
                               if (not popup.isActive())
                               {
                                   const auto shell { config.getString (Config::Key::shellProgram) };
                                   const auto shellArgs { juce::String ("-c ") + entry.command
                                                          + (entry.args.isNotEmpty() ? " " + entry.args : "") };

                                   const int cols { entry.cols > 0 ? entry.cols
                                                                   : config.getInt (Config::Key::popupCols) };
                                   const int rows { entry.rows > 0 ? entry.rows
                                                                   : config.getInt (Config::Key::popupRows) };

                                   const auto fm { typeface.calcMetrics (Terminal::Component::dpiCorrectedFontSize()) };
                                   const int pixelWidth  { cols * fm.logicalCellW };
                                   const int pixelHeight { rows * fm.logicalCellH };

                                   const auto cwd { entry.cwd.isNotEmpty() ? entry.cwd
                                                                           : appState.getPwd() };
                                   auto terminal { std::make_unique<Terminal::Component> (
                                       typeface, shell, shellArgs, cwd) };
                                   popup.show (*getTopLevelComponent(), std::move (terminal),
                                              pixelWidth, pixelHeight, glRenderer);
                               }

                               return true;
                           } };

        if (entry.modal.isNotEmpty())
            action.registerAction (
                "popup:" + name, "Popup: " + name, "Open " + name + " popup", "Popups", true, launchPopup);

        if (entry.global.isNotEmpty())
            action.registerAction (
                "popup_global:" + name, "Popup: " + name, "Open " + name + " popup", "Popups", false, launchPopup);
    }

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

        const auto fm { typeface.calcMetrics (Terminal::Component::dpiCorrectedFontSize()) };

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

            const int titleBarHeight { config.getBool (Config::Key::windowButtons) ? 24 : 0 };
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
            statusBarOverlay.updateHintInfo (terminal->getHintPage(), terminal->getHintTotalPages());
            terminal->repaint();
        }

        glRenderer.triggerRepaint();
    };

    // TODO: State restoration disabled — fix after renderer and tabs cleanup
    appState.getTabs().removeAllChildren (nullptr);
    appState.setActiveTabIndex (0);
    AppState::getContext()->setActivePaneType (App::ID::paneTypeTerminal);
    tabs->addNewTab();

    sendLookAndFeelChange();
}

/**
 * @brief Exits selection mode on the active terminal if it is currently modal.
 * @note MESSAGE THREAD.
 * @see Terminal::Component::exitSelectionMode
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
void MainComponent::initialiseMessageOverlay()
{
    messageOverlay = std::make_unique<MessageOverlay>();
    addChildComponent (messageOverlay.get());

    if (const auto& startupError { config.getLoadError() }; startupError.isNotEmpty())
    {
        juce::MessageManager::callAsync (
            [this, error = startupError]
            {
                messageOverlay->showMessage (error);
            });
    }
}

