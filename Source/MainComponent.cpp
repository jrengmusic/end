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
 * Construction order:
 * 1. `setOpaque(false)` — tells JUCE the component has transparency.
 * 2. Creates `Terminal::Tabs` with top orientation and adds initial tab.
 * 3. Registers this component with the command manager and adds key listener.
 * 4. Reads `windowWidth` / `windowHeight` from Config (which already applied
 *    `state.lua` overrides) and calls `setSize()`.
 * 5. Registers a `BackgroundBlur` close callback that persists the current
 *    window size to `state.lua` when the native close button is clicked.
 *
 * @note MESSAGE THREAD — called from ENDApplication::initialise().
 */
MainComponent::MainComponent()
{
    setOpaque (false);

    const auto* cfg { Config::getContext() };

    fonts = std::make_unique<Fonts> (cfg->getString (Config::Key::fontFamily),
                                     cfg->getFloat (Config::Key::fontSize));

    tabs = std::make_unique<Terminal::Tabs> (Terminal::Tabs::orientationFromString (
        cfg->getString (Config::Key::tabPosition)));
    addAndMakeVisible (tabs.get());

    auto& lf { tabs->getTabLookAndFeel() };
    juce::LookAndFeel::setDefaultLookAndFeel (&lf);

    glRenderer.setComponents (tabs->getComponents());
    glRenderer.setComponentPaintingEnabled (true);
    glRenderer.attachTo (*this);

    tabs->onRepaintNeeded = [this] { glRenderer.triggerRepaint(); };
    tabs->addNewTab();

    messageOverlay = std::make_unique<MessageOverlay>();
    addChildComponent (messageOverlay.get());

    buildCommandActions();

    commandManager.registerAllCommandsForTarget (this);
    keyBinding.applyMappings();
    addKeyListener (commandManager.getKeyMappings());

    setSize (cfg->getInt (Config::Key::windowWidth),
             cfg->getInt (Config::Key::windowHeight));

    const auto& startupError { cfg->getLoadError() };

    if (startupError.isNotEmpty())
    {
        juce::MessageManager::callAsync (
            [this, error = startupError]
            {
                messageOverlay->showMessage (error);
            });
    }

    jreng::BackgroundBlur::setCloseCallback (
        [this]()
        {
            Config::getContext()->saveWindowSize (getWidth(), getHeight());
        });
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

    if (messageOverlay != nullptr)
    {
        messageOverlay->setBounds (getLocalBounds());

        const auto fm { Fonts::getContext()->calcMetrics (
            Config::getContext()->getFloat (Config::Key::fontSize)) };

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

            const int titleBarHeight { Config::getContext()->getBool (Config::Key::windowButtons) ? 24 : 0 };
            content.removeFromTop (titleBarHeight);
            content = content.reduced (10, 10);

            const int cols { content.getWidth() / fm.logicalCellW };
            const int rows { content.getHeight() / fm.logicalCellH };

            if (cols > 0 and rows > 0 and isShowing())
            {
                messageOverlay->showMessage (
                    juce::String (cols) + " col * " + juce::String (rows) + " row", 1000);
            }
        }
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
    glRenderer.detach();
    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    messageOverlay.reset();
    tabs.reset();
    fonts.reset();
    removeKeyListener (commandManager.getKeyMappings());
    jreng::BackgroundBlur::setCloseCallback (nullptr);
}

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
            const auto reloadError { Config::getContext()->reload() };
            tabs->applyConfig();
            tabs->getTabLookAndFeel().setColours();
            tabs->sendLookAndFeelChange();
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
}

//==============================================================================
// juce::ApplicationCommandTarget implementation

juce::ApplicationCommandTarget* MainComponent::getNextCommandTarget()
{
    return nullptr;
}

void MainComponent::getAllCommands (juce::Array<juce::CommandID>& commands)
{
    for (const auto& def : commandDefs)
    {
        commands.add (static_cast<int> (def.id));
    }
}

void MainComponent::getCommandInfo (juce::CommandID commandID, juce::ApplicationCommandInfo& result)
{
    for (const auto& def : commandDefs)
    {
        if (static_cast<int> (def.id) == commandID)
        {
            result.setInfo (def.name, def.description, def.category, 0);
            const auto kp { keyBinding.getBinding (def.id) };
            if (kp.isValid())
                result.addDefaultKeypress (kp.getKeyCode(), kp.getModifiers());
            break;
        }
    }
}

bool MainComponent::perform (const juce::ApplicationCommandTarget::InvocationInfo& info)
{
    if (commandActions.contains (info.commandID))
        return commandActions.get (info.commandID);

    return false;
}
