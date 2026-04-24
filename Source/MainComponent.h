/**
 * @file MainComponent.h
 * @brief Root JUCE component that hosts the terminal UI.
 *
 * MainComponent is the content component of the application's `Window`.
 * It owns a `Terminal::Tabs` child that manages multiple terminal sessions,
 * and paints the window background with the configured colour and opacity.
 *
 * ### Responsibilities
 * - Sets the initial window size from `AppState` (persisted in `window.state` (standalone)
 *   or `<uuid>.display` (daemon client)).
 * - Delegates all keyboard, mouse, and terminal I/O to `Terminal::Tabs`.
 * - Registers all user-performable action callbacks with `Action::Registry`.
 *
 * @par Thread context
 * All methods are called on the **MESSAGE THREAD**.
 *
 * @see Terminal::Tabs
 * @see Config
 * @see ENDApplication::systemRequestedQuit
 * @see Action::Registry
 */

/*
  ==============================================================================

    END - Ephemeral Nexus Display
    Main application component

    MainComponent.h - Main application content component

  ==============================================================================
*/

#pragma once
#include <JuceHeader.h>
#include "AppState.h"
#include "component/LookAndFeel.h"
#include "component/TerminalWindow.h"
#include "component/PaneComponent.h"
#include "component/MessageOverlay.h"
#include "component/Popup.h"
#include "component/Tabs.h"
#include "config/Config.h"
#include "action/Action.h"
#include "action/ActionList.h"
// jam::Typeface is available via JuceHeader → jam_graphics
#include "component/StatusBarOverlay.h"
#include "whelmed/Component.h"
#include "config/WhelmedConfig.h"
#include "scripting/Scripting.h"

/**
 * @class MainComponent
 * @brief Root content component of the END application window.
 *
 * Placed inside `jam::Window` by `ENDApplication::initialise()`.
 * Owns the `Terminal::Tabs` container and paints the translucent background
 * layer that shows through the native window blur effect.
 *
 * @par Layout
 * `resized()` gives the full local bounds to `Terminal::Tabs`; each terminal
 * applies its own insets and title-bar offset internally.
 *
 * @par Background painting
 * `paint()` fills with `backgroundColour.withAlpha(opacity)`.  The native blur
 * layer beneath the window provides the frosted-glass effect; this fill sets
 * the tint colour and transparency.
 *
 * @see Terminal::Tabs
 * @see Config::Key::windowColour
 * @see Config::Key::windowOpacity
 * @see Action::Registry
 */
class MainComponent : public juce::Component,
                      public juce::ValueTree::Listener
{
public:
    /**
     * @brief Constructs the component, creates Terminal::Tabs, sets initial size.
     *
     * @param fontRegistry  Font slot registry owned by ENDApplication.  Passed
     *                      through to the `Fonts` instance so font slots can be
     *                      populated and resolved without a singleton.
     */
    MainComponent (jam::Typeface::Registry& fontRegistry);

    /** @brief Removes ValueTree listeners and tears down LookAndFeel. */
    ~MainComponent() override;

    /**
     * @brief Fills the full bounds to Terminal::Tabs.
     * @note MESSAGE THREAD — called by JUCE layout system on every resize.
     */
    void resized() override;
    void paint (juce::Graphics& g) override;

    /**
     * @brief Rebuilds actions, applies config to tabs, LookAndFeel, and orientation.
     *
     * Called by `Config::onReload` (wired in Main.cpp) after Config reloads
     * `end.lua`.  Also called once from the constructor for initial setup.
     *
     * @note MESSAGE THREAD.
     * @see Config::onReload
     */
    void applyConfig();

private:
    /**
     * @brief Registers all user-performable actions with `Action::Registry`.
     *
     * Clears existing actions, delegates to grouped register* methods, then
     * rebuilds the key map.
     *
     * @note MESSAGE THREAD.
     * @see Action::Registry
     */
    void registerActions();

    /** @brief Registers copy, paste, and newline actions. @note MESSAGE THREAD. */
    void registerEditActions (Action::Registry& action);

    /** @brief Registers quit, reload_config, new_window, and action_list actions. @note MESSAGE THREAD. */
    void registerApplicationActions (Action::Registry& action);

    /** @brief Registers close_tab, new_tab, prev_tab, and next_tab actions. @note MESSAGE THREAD. */
    void registerTabActions (Action::Registry& action);

    /** @brief Registers split and pane-focus actions. @note MESSAGE THREAD. */
    void registerPaneActions (Action::Registry& action);

    /** @brief Registers zoom, selection, open-file, and open-markdown actions. @note MESSAGE THREAD. */
    void registerNavigationActions (Action::Registry& action);


    /**
     * @brief Sets the renderer type — GL lifecycle, atlas, and terminal switching.
     *
     * @param rendererType  The renderer type to apply.
     * @note MESSAGE THREAD.
     */
    void setRenderer (App::RendererType rendererType);

    /** @brief Cached context references; resolved once, used everywhere. */
    Config& config { *Config::getContext() };
    AppState& appState { *AppState::getContext() };

    /** @brief Application-wide LookAndFeel; set as default, inherited by all children. */
    Terminal::LookAndFeel terminalLookAndFeel;

    /** @brief Global typeface instance; provides font metrics and shaping for all terminals. */
    jam::Typeface typeface;

    /** @brief GL texture handle store; shared by all Screen<GLContext> instances. */
    jam::gl::GlyphAtlas glyphAtlas;

    /** @brief CPU atlas image store; shared by all Screen<GraphicsContext> instances. */
    jam::GraphicsAtlas graphicsAtlas;

    /** @brief Tabbed terminal container; owns all Terminal::Display instances. */
    std::unique_ptr<Terminal::Tabs> tabs;

    /** @brief Transient overlay for grid-size and status messages. */
    std::unique_ptr<MessageOverlay> messageOverlay;

    /** @brief Status bar overlay; shown during any active modal state (selection, open-file, etc.). */
    std::unique_ptr<StatusBarOverlay> statusBarOverlay;

    /** @brief Persistent wrapper for the AppState NEXUS child node. Must outlive the
     *         listener registration — `addListener` stores the listener in the wrapper
     *         instance, not in the underlying shared object. A temporary wrapper would
     *         destroy its listener list at end-of-statement. */
    juce::ValueTree nexusNode;

    /** @brief Persistent wrapper for the AppState SESSIONS child node.  Listener
     *         registration requires the wrapper to outlive it. */
    juce::ValueTree sessionsNode;

    /** @brief Modal popup dialog; shows content in a glass window. */
    Terminal::Popup popup;

    /** @brief Lua scripting engine — loads action.lua, registers popup/custom actions, builds key maps. */
    std::unique_ptr<Scripting::Engine> scriptingEngine;

#if JUCE_WINDOWS
    /** @brief Fires when the native scale factor changes.
     *  Updates AppState and resets zoom on all terminals. */
    juce::NativeScaleFactorNotifier scaleNotifier { this,
                                                    [this] (float)
                                                    {
                                                        if (tabs != nullptr)
                                                            tabs->resetZoom();
                                                    } };
#endif

    /**
     * @brief Creates Terminal::Tabs, wires repaint callback, restores tabs.
     * @note MESSAGE THREAD.
     * @see Terminal::Tabs
     * @see AppState
     */
    void initialiseTabs();

    /**
     * @brief Returns the content area available for terminal panes after subtracting chrome.
     *
     * Subtracts: title bar (when windowButtons is enabled), tab bar (when multiple tabs shown),
     * and terminal padding from each edge. Used by the restore walker and fresh-tab dim
     * computation to guarantee deterministic spawn dims from AppState window size.
     *
     * @param windowWidth   Pixel width of the window (from AppState or getWidth()).
     * @param windowHeight  Pixel height of the window (from AppState or getHeight()).
     * @param tabCount      Number of tabs currently open (determines tab bar visibility).
     * @return Content rect with chrome removed, ready to pass to Panes::cellsFromRect.
     * @note MESSAGE THREAD.
     */
    juce::Rectangle<int> getContentRect (int windowWidth, int windowHeight, int tabCount) const noexcept;

    /**
     * @brief Fires when a property on a listened ValueTree node changes.
     *
     * @note MESSAGE THREAD.
     */
    void valueTreePropertyChanged (juce::ValueTree& tree,
                                   const juce::Identifier& property) override;

    /**
     * @brief Fires when a direct child is added to a listened node.
     *
     * In local mode: SESSIONS node created under nexusNode triggers tab initialisation.
     * In client mode: first SESSION child under sessionsNode triggers tab initialisation.
     *
     * @note MESSAGE THREAD.
     */
    void valueTreeChildAdded (juce::ValueTree& parent,
                              juce::ValueTree& child) override;

    /**
     * @brief Fires when a direct child is removed from a listened node.
     *
     * @note MESSAGE THREAD.
     */
    void valueTreeChildRemoved (juce::ValueTree& parent,
                                juce::ValueTree& child,
                                int index) override;

    //==============================================================================
    /**
     * @brief Exits selection mode on the active terminal if it is currently modal.
     *
     * Called before tab and pane switches so that vim-style selection mode does
     * not persist on a terminal that is no longer focused.
     *
     * @note MESSAGE THREAD.
     */
    void exitActiveTerminalSelectionMode() noexcept;

    /**
     * @brief Creates MessageOverlay, shows startup errors if any.
     * @note MESSAGE THREAD.
     * @see MessageOverlay
     * @see Config::getLoadError()
     */
    void initialiseOverlays();

    /**
     * @brief Computes grid dimensions from font metrics and window bounds, displays "cols * rows" overlay on resize.
     * @note MESSAGE THREAD — called from resized().
     * @see MessageOverlay
     * @see fonts
     */
    void showMessageOverlay();

    //==============================================================================
    // #if JUCE_DEBUG
    //     jam::debug::Widget debug { this, false };
    // #endif
    //==============================================================================

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
