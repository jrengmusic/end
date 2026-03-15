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
MainComponent::MainComponent()
{
    setOpaque (false);

    //==============================================================================
    initialiseTabs();
    initialiseMessageOverlay();
    registerActions();

    //==============================================================================
    setSize (appState.getWindowWidth(), appState.getWindowHeight());
    setLookAndFeel (&terminalLookAndFeel);
    juce::LookAndFeel::setDefaultLookAndFeel (&terminalLookAndFeel);

}

/**
 * @brief Fills the full bounds to Terminal::Tabs.
 *
 * The tab container handles its own tab bar layout and passes the content
 * area to each Terminal::Component, which applies its own internal insets.
 *
 * @note MESSAGE THREAD — called by JUCE on every resize event.
 */
void MainComponent::resized()
{
    if (tabs != nullptr)
        tabs->setBounds (getLocalBounds());

    showMessageOverlay();
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
 * @see action
 * @see Terminal::Action::registerAction
 */
void MainComponent::registerActions()
{
    action.registerAction ("copy", "Copy", "Copy selection to clipboard", "Edit", false,
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

    action.registerAction ("paste", "Paste", "Paste from clipboard", "Edit", false,
        [this]() -> bool
        {
            tabs->pasteClipboard();
            return true;
        });

    action.registerAction ("quit", "Quit", "Quit application", "Application", false,
        [this]() -> bool
        {
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
            return true;
        });

    action.registerAction ("close_tab", "Close Tab", "Close current tab", "Tabs", false,
        [this]() -> bool
        {
            tabs->closeActiveTab();

            if (tabs->getTabCount() == 0)
                juce::JUCEApplication::getInstance()->systemRequestedQuit();

            return true;
        });

    action.registerAction ("reload_config", "Reload Config", "Reload configuration", "Application", false,
        [this]() -> bool
        {
            const auto reloadError { config.reload() };
            action.reload();
            tabs->applyConfig();
            terminalLookAndFeel.setColours();
            sendLookAndFeelChange();
            tabs->applyOrientation();

            if (reloadError.isEmpty())
                messageOverlay->showMessage ("RELOADED", 1000);
            else
                messageOverlay->showMessage (reloadError);

            return true;
        });

    action.registerAction ("zoom_in", "Zoom In", "Increase font size", "View", false,
        [this]() -> bool
        {
            tabs->increaseZoom();
            return true;
        });

    action.registerAction ("zoom_out", "Zoom Out", "Decrease font size", "View", false,
        [this]() -> bool
        {
            tabs->decreaseZoom();
            return true;
        });

    action.registerAction ("zoom_reset", "Zoom Reset", "Reset font size to default", "View", false,
        [this]() -> bool
        {
            tabs->resetZoom();
            return true;
        });

    action.registerAction ("new_tab", "New Tab", "Open a new terminal tab", "Tabs", false,
        [this]() -> bool
        {
            tabs->addNewTab();
            return true;
        });

    action.registerAction ("prev_tab", "Previous Tab", "Switch to previous tab", "Tabs", false,
        [this]() -> bool
        {
            tabs->selectPreviousTab();
            return true;
        });

    action.registerAction ("next_tab", "Next Tab", "Switch to next tab", "Tabs", false,
        [this]() -> bool
        {
            tabs->selectNextTab();
            return true;
        });

    action.registerAction ("split_horizontal", "Split Horizontal", "Split pane horizontally", "Panes", true,
        [this]() -> bool
        {
            tabs->splitHorizontal();
            return true;
        });

    action.registerAction ("split_vertical", "Split Vertical", "Split pane vertically", "Panes", true,
        [this]() -> bool
        {
            tabs->splitVertical();
            return true;
        });

    action.registerAction ("pane_left", "Focus Left Pane", "Move focus to the left pane", "Panes", true,
        [this]() -> bool
        {
            tabs->focusPaneLeft();
            return true;
        });

    action.registerAction ("pane_down", "Focus Down Pane", "Move focus to the pane below", "Panes", true,
        [this]() -> bool
        {
            tabs->focusPaneDown();
            return true;
        });

    action.registerAction ("pane_up", "Focus Up Pane", "Move focus to the pane above", "Panes", true,
        [this]() -> bool
        {
            tabs->focusPaneUp();
            return true;
        });

    action.registerAction ("pane_right", "Focus Right Pane", "Move focus to the right pane", "Panes", true,
        [this]() -> bool
        {
            tabs->focusPaneRight();
            return true;
        });

    //==============================================================================
    action.reload();

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

        const auto fm { fonts.calcMetrics (Terminal::Component::dpiCorrectedFontSize()) };

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
            content.removeFromTop (titleBarHeight);
            content = content.reduced (10, 10);

            const int cols { content.getWidth() / fm.logicalCellW };
            const int rows { content.getHeight() / fm.logicalCellH };

            if (cols > 0 and rows > 0 and isShowing())
            {
                messageOverlay->showMessage (juce::String (cols) + " col * " + juce::String (rows) + " row", 1000);
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
        Terminal::Tabs::orientationFromString (config.getString (Config::Key::tabPosition)));
    addAndMakeVisible (tabs.get());

    glRenderer.setComponentIterator ([this] (std::function<void (jreng::GLComponent&)> renderComponent)
    {
        for (auto& terminal : tabs->getTerminals())
        {
            if (terminal->isVisible())
            {
                renderComponent (*terminal);
            }
        }
    });
    glRenderer.setComponentPaintingEnabled (true);
    glRenderer.attachTo (*this);

    tabs->onRepaintNeeded = [this]
    {
        glRenderer.triggerRepaint();
    };

    // TODO: State restoration disabled — fix after renderer and tabs cleanup
    appState.getTabs().removeAllChildren (nullptr);
    appState.setActiveTabIndex (0);
    tabs->addNewTab();

    sendLookAndFeelChange();
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


