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

    tabs = std::make_unique<Terminal::Tabs> (juce::TabbedButtonBar::TabsAtTop);
    addAndMakeVisible (tabs.get());

    glRenderer.setComponents (tabs->getComponents());
    glRenderer.setComponentPaintingEnabled (true);
    glRenderer.attachTo (*this);

    tabs->onRepaintNeeded = [this] { glRenderer.triggerRepaint(); };
    tabs->addNewTab();

    commandManager.registerAllCommandsForTarget (this);
    keyBinding.applyMappings();
    addKeyListener (commandManager.getKeyMappings());

    const auto* cfg { Config::getContext() };
    setSize (cfg->getInt (Config::Key::windowWidth),
             cfg->getInt (Config::Key::windowHeight));

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
    tabs.reset();
    removeKeyListener (commandManager.getKeyMappings());
    jreng::BackgroundBlur::setCloseCallback (nullptr);
}

//==============================================================================
// juce::ApplicationCommandTarget implementation

juce::ApplicationCommandTarget* MainComponent::getNextCommandTarget()
{
    return nullptr;
}

void MainComponent::getAllCommands (juce::Array<juce::CommandID>& commands)
{
    using C = KeyBinding::CommandID;

    commands.addArray ({
        static_cast<int> (C::copy),
        static_cast<int> (C::paste),
        static_cast<int> (C::quit),
        static_cast<int> (C::closeTab),
        static_cast<int> (C::reload),
        static_cast<int> (C::zoomIn),
        static_cast<int> (C::zoomOut),
        static_cast<int> (C::zoomReset),
        static_cast<int> (C::newTab),
        static_cast<int> (C::prevTab),
        static_cast<int> (C::nextTab)
    });
}

void MainComponent::getCommandInfo (juce::CommandID commandID, juce::ApplicationCommandInfo& result)
{
    using C = KeyBinding::CommandID;

    switch (commandID)
    {
        case static_cast<int> (C::copy):
        {
            result.setInfo ("Copy", "Copy selection to clipboard", "Edit", 0);
            const auto kp { keyBinding.getBinding (C::copy) };
            if (kp.isValid())
                result.addDefaultKeypress (kp.getKeyCode(), kp.getModifiers());
            break;
        }
        case static_cast<int> (C::paste):
        {
            result.setInfo ("Paste", "Paste from clipboard", "Edit", 0);
            const auto kp { keyBinding.getBinding (C::paste) };
            if (kp.isValid())
                result.addDefaultKeypress (kp.getKeyCode(), kp.getModifiers());
            break;
        }
        case static_cast<int> (C::quit):
        {
            result.setInfo ("Quit", "Quit application", "Application", 0);
            const auto kp { keyBinding.getBinding (C::quit) };
            if (kp.isValid())
                result.addDefaultKeypress (kp.getKeyCode(), kp.getModifiers());
            break;
        }
        case static_cast<int> (C::closeTab):
        {
            result.setInfo ("Close Tab", "Close current tab", "Application", 0);
            const auto kp { keyBinding.getBinding (C::closeTab) };
            if (kp.isValid())
                result.addDefaultKeypress (kp.getKeyCode(), kp.getModifiers());
            break;
        }
        case static_cast<int> (C::reload):
        {
            result.setInfo ("Reload", "Reload configuration", "Application", 0);
            const auto kp { keyBinding.getBinding (C::reload) };
            if (kp.isValid())
                result.addDefaultKeypress (kp.getKeyCode(), kp.getModifiers());
            break;
        }
        case static_cast<int> (C::zoomIn):
        {
            result.setInfo ("Zoom In", "Increase font size", "View", 0);
            const auto kp { keyBinding.getBinding (C::zoomIn) };
            if (kp.isValid())
                result.addDefaultKeypress (kp.getKeyCode(), kp.getModifiers());
            break;
        }
        case static_cast<int> (C::zoomOut):
        {
            result.setInfo ("Zoom Out", "Decrease font size", "View", 0);
            const auto kp { keyBinding.getBinding (C::zoomOut) };
            if (kp.isValid())
                result.addDefaultKeypress (kp.getKeyCode(), kp.getModifiers());
            break;
        }
        case static_cast<int> (C::zoomReset):
        {
            result.setInfo ("Zoom Reset", "Reset font size to default", "View", 0);
            const auto kp { keyBinding.getBinding (C::zoomReset) };
            if (kp.isValid())
                result.addDefaultKeypress (kp.getKeyCode(), kp.getModifiers());
            break;
        }
        case static_cast<int> (C::newTab):
        {
            result.setInfo ("New Tab", "Open a new terminal tab", "Tabs", 0);
            const auto kp { keyBinding.getBinding (C::newTab) };
            if (kp.isValid())
                result.addDefaultKeypress (kp.getKeyCode(), kp.getModifiers());
            break;
        }
        case static_cast<int> (C::prevTab):
        {
            result.setInfo ("Previous Tab", "Switch to previous tab", "Tabs", 0);
            const auto kp { keyBinding.getBinding (C::prevTab) };
            if (kp.isValid())
                result.addDefaultKeypress (kp.getKeyCode(), kp.getModifiers());
            break;
        }
        case static_cast<int> (C::nextTab):
        {
            result.setInfo ("Next Tab", "Switch to next tab", "Tabs", 0);
            const auto kp { keyBinding.getBinding (C::nextTab) };
            if (kp.isValid())
                result.addDefaultKeypress (kp.getKeyCode(), kp.getModifiers());
            break;
        }
        default:
            break;
    }
}

bool MainComponent::perform (const juce::ApplicationCommandTarget::InvocationInfo& info)
{
    using C = KeyBinding::CommandID;

    switch (info.commandID)
    {
        case static_cast<int> (C::copy):
            tabs->copySelection();
            return true;
        case static_cast<int> (C::paste):
            tabs->pasteClipboard();
            return true;
        case static_cast<int> (C::quit):
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
            return true;
        case static_cast<int> (C::closeTab):
            tabs->closeActiveTab();
            if (tabs->getTabCount() == 0)
                juce::JUCEApplication::getInstance()->systemRequestedQuit();
            return true;
        case static_cast<int> (C::reload):
            keyBinding.reload();
            commandManager.registerAllCommandsForTarget (this);
            keyBinding.applyMappings();
            tabs->reloadConfig();
            return true;
        case static_cast<int> (C::zoomIn):
            tabs->increaseZoom();
            return true;
        case static_cast<int> (C::zoomOut):
            tabs->decreaseZoom();
            return true;
        case static_cast<int> (C::zoomReset):
            tabs->resetZoom();
            return true;
        case static_cast<int> (C::newTab):
            tabs->addNewTab();
            return true;
        case static_cast<int> (C::prevTab):
            tabs->selectPreviousTab();
            return true;
        case static_cast<int> (C::nextTab):
            tabs->selectNextTab();
            return true;
        default:
            return false;
    }
}
