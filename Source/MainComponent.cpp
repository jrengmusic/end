/**
 * @file MainComponent.cpp
 * @brief Implementation of the root application content component.
 *
 * Constructs the `Terminal::Tabs` container, sets the initial window size from
 * persisted state, and registers a close callback so that window dimensions
 * are saved when the native close button is pressed.
 *
 * Also owns the ApplicationCommandManager and KeyBinding to handle application
 * commands (copy, paste, quit, tab management, reload, zoom) via JUCE command
 * system.
 *
 * @see MainComponent
 * @see Terminal::Tabs
 * @see Config
 * @see KeyBinding
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
 * - `commandManager`, `keyBinding` — command dispatch
 * - `terminalLookAndFeel` — application-wide LookAndFeel
 * - `glRenderer` — OpenGL renderer
 * - `fonts` — global font context (from config)
 *
 * Constructor body:
 * 1. `setOpaque(false)` — tells JUCE the component has transparency.
 * 2. `initialiseTabs()` — creates Tabs, attaches GL renderer, adds first tab.
 * 3. `initialiseMessageOverlay()` — creates overlay, shows startup errors.
 * 4. `buildCommandActions()` — populates command table, registers keybindings,
 *    sets BackgroundBlur close callback.
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
    buildCommandActions();

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
    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    jreng::BackgroundBlur::setCloseCallback (nullptr);
    removeKeyListener (commandManager.getKeyMappings());
    glRenderer.detach();
}

/**
 * @brief Populates commandActions with one lambda per command.
 * @note MESSAGE THREAD.
 * @see commandActions
 * @see perform()
 */
void MainComponent::buildCommandActions()
{
    commandActions.add (static_cast<int> (KeyBinding::CommandID::copy),
                        [this]() -> bool
                        {
                            tabs->copySelection();
                            return true;
                        });

    commandActions.add (static_cast<int> (KeyBinding::CommandID::paste),
                        [this]() -> bool
                        {
                            tabs->pasteClipboard();
                            return true;
                        });

    commandActions.add (static_cast<int> (KeyBinding::CommandID::quit),
                        [this]() -> bool
                        {
                            juce::JUCEApplication::getInstance()->systemRequestedQuit();
                            return true;
                        });

    commandActions.add (static_cast<int> (KeyBinding::CommandID::closeTab),
                        [this]() -> bool
                        {
                            tabs->closeActiveTab();
                            if (tabs->getTabCount() == 0)
                                juce::JUCEApplication::getInstance()->systemRequestedQuit();
                            return true;
                        });

    commandActions.add (static_cast<int> (KeyBinding::CommandID::reload),
                        [this]() -> bool
                        {
                            keyBinding.reload();
                            commandManager.registerAllCommandsForTarget (this);
                            keyBinding.applyMappings();
                            modalKeyBinding.reload();
                            bindModalActions();
                            const auto reloadError { config.reload() };
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

    commandActions.add (static_cast<int> (KeyBinding::CommandID::zoomIn),
                        [this]() -> bool
                        {
                            tabs->increaseZoom();
                            return true;
                        });

    commandActions.add (static_cast<int> (KeyBinding::CommandID::zoomOut),
                        [this]() -> bool
                        {
                            tabs->decreaseZoom();
                            return true;
                        });

    commandActions.add (static_cast<int> (KeyBinding::CommandID::zoomReset),
                        [this]() -> bool
                        {
                            tabs->resetZoom();
                            return true;
                        });

    commandActions.add (static_cast<int> (KeyBinding::CommandID::newTab),
                        [this]() -> bool
                        {
                            tabs->addNewTab();
                            return true;
                        });

    commandActions.add (static_cast<int> (KeyBinding::CommandID::prevTab),
                        [this]() -> bool
                        {
                            tabs->selectPreviousTab();
                            return true;
                        });

    commandActions.add (static_cast<int> (KeyBinding::CommandID::nextTab),
                        [this]() -> bool
                        {
                            tabs->selectNextTab();
                            return true;
                        });

    //==============================================================================
    bindModalActions();

    //==============================================================================
    commandManager.registerAllCommandsForTarget (this);
    keyBinding.applyMappings();
    addKeyListener (commandManager.getKeyMappings());

    jreng::BackgroundBlur::setCloseCallback (
        [this]()
        {
            appState.setWindowSize (getWidth(), getHeight());
        });
}

/**
 * @brief Binds the six modal pane/split actions to their tab callbacks.
 * @note MESSAGE THREAD.
 * @see modalKeyBinding
 */
void MainComponent::bindModalActions()
{
    modalKeyBinding.setAction (ModalKeyBinding::Action::paneLeft,
                               [this]() { tabs->focusPaneLeft(); });
    modalKeyBinding.setAction (ModalKeyBinding::Action::paneDown,
                               [this]() { tabs->focusPaneDown(); });
    modalKeyBinding.setAction (ModalKeyBinding::Action::paneUp,
                               [this]() { tabs->focusPaneUp(); });
    modalKeyBinding.setAction (ModalKeyBinding::Action::paneRight,
                               [this]() { tabs->focusPaneRight(); });
    modalKeyBinding.setAction (ModalKeyBinding::Action::splitHorizontal,
                               [this]() { tabs->splitHorizontal(); });
    modalKeyBinding.setAction (ModalKeyBinding::Action::splitVertical,
                               [this]() { tabs->splitVertical(); });
}

//==============================================================================
// juce::ApplicationCommandTarget implementation

/**
 * @brief Returns nullptr, indicating no next target.
 * @note MESSAGE THREAD.
 * @return Always nullptr.
 */
juce::ApplicationCommandTarget* MainComponent::getNextCommandTarget() { return nullptr; }

/**
 * @brief Populates the commands array with all registered command IDs.
 * @note MESSAGE THREAD.
 * @param commands Array to receive command IDs.
 * @see commandDefs
 */
void MainComponent::getAllCommands (juce::Array<juce::CommandID>& commands)
{
    for (const auto& def : commandDefs)
    {
        commands.add (static_cast<int> (def.id));
    }
}

/**
 * @brief Populates ApplicationCommandInfo for the given command ID.
 * @note MESSAGE THREAD.
 * @param commandID The command to query.
 * @param result Structure to receive command metadata.
 * @see commandDefs
 * @see keyBinding
 */
void MainComponent::getCommandInfo (juce::CommandID commandID, juce::ApplicationCommandInfo& result)
{
    for (const auto& def : commandDefs)
    {
        if (static_cast<int> (def.id) == commandID)
        {
            result.setInfo (def.name, def.description, def.category, 0);

            if (const auto keypress { keyBinding.getBinding (def.id) }; keypress.isValid())
                result.addDefaultKeypress (keypress.getKeyCode(), keypress.getModifiers());
            break;
        }
    }
}

/**
 * @brief Dispatches the command to its action lambda.
 * @note MESSAGE THREAD.
 * @param info Invocation details containing command ID.
 * @return true if command was found and executed.
 * @see commandActions
 */
bool MainComponent::perform (const juce::ApplicationCommandTarget::InvocationInfo& info)
{
    if (commandActions.contains (info.commandID))
        return commandActions.get (info.commandID);

    return false;
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


