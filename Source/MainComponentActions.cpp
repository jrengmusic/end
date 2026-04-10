/**
 * @file MainComponentActions.cpp
 * @brief Action registration method definitions for MainComponent.
 *
 * Contains the six register* methods that wire user-performable actions into
 * Action::Registry. Separated from MainComponent.cpp to keep the root
 * component file focused on lifecycle and layout concerns.
 *
 * @see MainComponent::registerActions
 * @see Action::Registry
 */

/*
  ==============================================================================

    END - Ephemeral Nexus Display
    Main application component

    MainComponentActions.cpp - Action registration method definitions

  ==============================================================================
*/

#include "MainComponent.h"

/** @note MESSAGE THREAD. */
void MainComponent::registerEditActions (Action::Registry& action)
{
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
}

/** @note MESSAGE THREAD. */
void MainComponent::registerApplicationActions (Action::Registry& action)
{
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

    action.registerAction ("action_list",
                           "Action List",
                           "Open command palette",
                           "Application",
                           true,
                           [this]() -> bool
                           {
                               actionList = std::make_unique<Action::List> (*this);
                               actionList->onModalDismissed = [this]
                               {
                                   actionList.reset();
                               };

                               return true;
                           });
}

/** @note MESSAGE THREAD. */
void MainComponent::registerTabActions (Action::Registry& action)
{
    action.registerAction ("close_tab",
                           "Close Tab",
                           "Close current tab",
                           "Tabs",
                           false,
                           [this]() -> bool
                           {
                               tabs->closeActiveTab();

                               if (tabs->getTabCount() == 0)
                               {
                                   if (appState.isNexusMode())
                                       appState.getStateFile().deleteFile();

                                   juce::JUCEApplication::getInstance()->systemRequestedQuit();
                               }
                               else if (appState.isNexusMode())
                               {
                                   appState.save();
                               }

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
}

/** @note MESSAGE THREAD. */
void MainComponent::registerPaneActions (Action::Registry& action)
{
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
}

/** @note MESSAGE THREAD. */
void MainComponent::registerNavigationActions (Action::Registry& action)
{
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
}

/** @note MESSAGE THREAD. */
void MainComponent::registerPopupActions (Action::Registry& action)
{
    for (const auto& pair : config.getPopups())
    {
        const auto& name { pair.first };
        const auto& entry { pair.second };

        auto launchPopup {
            [this, entry]() -> bool
            {
                if (not popup.isActive())
                {
                    const auto shell { config.getString (Config::Key::shellProgram) };
                    const auto configShellArgs { config.getString (Config::Key::shellArgs) };
                    auto shellArgs { (configShellArgs.isNotEmpty() ? configShellArgs + " " : juce::String())
                                    + "-c " + entry.command
                                    + (entry.args.isNotEmpty() ? " " + entry.args : "") };

                    const int cols { entry.cols > 0 ? entry.cols : config.getInt (Config::Key::popupCols) };
                    const int rows { entry.rows > 0 ? entry.rows : config.getInt (Config::Key::popupRows) };

                    const auto fm { typeface.calcMetrics (Terminal::Display::dpiCorrectedFontSize()) };

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

                    const auto cwd { entry.cwd.isNotEmpty() ? entry.cwd : appState.getPwd() };

                    auto termSession { Terminal::Session::create (cwd, cols, rows, shell, shellArgs) };

                    termSession->onExit = [this]
                    {
                        juce::MessageManager::callAsync ([this] { popup.dismiss(); });
                    };

                    auto terminal { termSession->getProcessor().createDisplay (typeface) };

                    popup.show (*getTopLevelComponent(), std::move (terminal), pixelWidth, pixelHeight, glRenderer);
                    popup.terminalSession = std::move (termSession);
                }

                return true;
            }
        };

        if (entry.modal.isNotEmpty())
            action.registerAction (
                "popup:" + name, "Popup: " + name, "Open " + name + " popup", "Popups", true, launchPopup);

        if (entry.global.isNotEmpty())
            action.registerAction (
                "popup_global:" + name, "Popup: " + name, "Open " + name + " popup", "Popups", false, launchPopup);
    }
}
