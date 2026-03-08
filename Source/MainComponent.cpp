/**
 * @file MainComponent.cpp
 * @brief Implementation of the root application content component.
 *
 * Constructs the `Terminal::Component`, sets the initial window size from
 * persisted state, and registers a close callback so that window dimensions
 * are saved when the native close button is pressed.
 *
 * Also owns the ApplicationCommandManager and KeyBinding to handle application
 * commands (copy, paste, quit, close tab, reload, zoom) via JUCE command system.
 *
 * @see MainComponent
 * @see Terminal::Component
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
 * 2. Creates `Terminal::Component` and makes it visible.
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

    terminal = std::make_unique<Terminal::Component>();
    addAndMakeVisible (terminal.get());

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
 * @brief Fills the full bounds to Terminal::Component.
 *
 * The terminal applies its own internal insets (horizontal/vertical padding
 * and optional title-bar offset), so we simply hand it the entire local area.
 *
 * @note MESSAGE THREAD — called by JUCE on every resize event.
 */
void MainComponent::resized()
{
    if (terminal != nullptr)
        terminal->setBounds (getLocalBounds());
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
        static_cast<int> (C::zoomReset)
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
            terminal->copySelection();
            return true;
        case static_cast<int> (C::paste):
            terminal->pasteClipboard();
            return true;
        case static_cast<int> (C::quit):
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
            return true;
        case static_cast<int> (C::closeTab):
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
            return true;
        case static_cast<int> (C::reload):
            keyBinding.reload();
            commandManager.registerAllCommandsForTarget (this);
            keyBinding.applyMappings();
            terminal->reloadConfig();
            return true;
        case static_cast<int> (C::zoomIn):
            terminal->increaseZoom();
            return true;
        case static_cast<int> (C::zoomOut):
            terminal->decreaseZoom();
            return true;
        case static_cast<int> (C::zoomReset):
            terminal->resetZoom();
            return true;
        default:
            return false;
    }
}
