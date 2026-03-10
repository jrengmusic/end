/**
 * @file Tabs.h
 * @brief Tab container that manages multiple Terminal::Component instances.
 *
 * Terminal::Tabs subclasses juce::TabbedComponent to provide a tabbed
 * interface where each tab hosts an independent terminal session. Content
 * components are NOT passed to JUCE's addTab() — instead they are managed
 * as direct children with visibility toggling, so that hidden sessions
 * stay alive and VBlank stops naturally.
 *
 * @par Ownership
 * Terminal::Component instances are owned by `jreng::Owner<Component>`.
 * The TabbedComponent base only knows about tab names and colours.
 *
 * @par Tab bar visibility
 * The tab bar is hidden (depth 0) when only one tab exists, and shown
 * at the configured height when multiple tabs are present.
 *
 * @see Terminal::Component
 * @see Terminal::LookAndFeel
 * @see jreng::Owner
 */

#pragma once
#include <JuceHeader.h>
#include "TerminalComponent.h"
#include "LookAndFeel.h"

namespace Terminal
{ /*____________________________________________________________________________*/
/**
 * @class Tabs
 * @brief Tabbed container managing multiple terminal sessions.
 *
 * Each tab corresponds to a Terminal::Component with its own Session,
 * PTY, Grid, Screen, and OpenGL context. Tabs are added with addNewTab()
 * and removed with closeActiveTab(). The active terminal is the only
 * visible child; all others remain as hidden children.
 *
 * @note MESSAGE THREAD — all methods.
 */
class Tabs : public juce::TabbedComponent
{
public:
    /**
     * @brief Construct a new Tabs container.
     * @param orientation The tab bar orientation (top, bottom, left, or right).
     * @note MESSAGE THREAD.
     */
    explicit Tabs (juce::TabbedButtonBar::Orientation orientation);

    /**
     * @brief Destructor.
     * @note MESSAGE THREAD.
     */
    ~Tabs() override;

    /**
     * @brief Callback invoked when any terminal needs a repaint.
     *
     * This callback is set by MainComponent to trigger GLRenderer::triggerRepaint().
     * It is forwarded to each new terminal's own onRepaintNeeded callback in addNewTab().
     *
     * @note MESSAGE THREAD.
     */
    std::function<void()> onRepaintNeeded;

    /**
     * @brief Create and add a new terminal tab.
     *
     * Creates a new Terminal::Component, adds it to the terminals owner,
     * adds a tab to the JUCE tab bar, and wires the onRepaintNeeded callback.
     *
     * @note MESSAGE THREAD.
     */
    void addNewTab();

    /**
     * @brief Close the currently active tab.
     *
     * Removes the active tab and its terminal. If the last tab is closed,
     * the caller (MainComponent) handles quit.
     *
     * @note MESSAGE THREAD.
     */
    void closeActiveTab();

    /**
     * @brief Select the previous tab, wrapping to the last tab if at the beginning.
     * @note MESSAGE THREAD.
     */
    void selectPreviousTab();

    /**
     * @brief Select the next tab, wrapping to the first tab if at the end.
     * @note MESSAGE THREAD.
     */
    void selectNextTab();

    /**
     * @brief Get the number of open tabs.
     * @return The tab count.
     * @note MESSAGE THREAD.
     */
    int getTabCount() const noexcept;

    /**
     * @brief Copy the current selection to clipboard.
     *
     * Forwards to getActiveTerminal().
     *
     * @note MESSAGE THREAD.
     */
    void copySelection();

    /**
     * @brief Paste from clipboard to active terminal.
     *
     * Forwards to getActiveTerminal().
     *
     * @note MESSAGE THREAD.
     */
    void pasteClipboard();

    /**
     * @brief Applies the current config to all terminal sessions.
     *
     * Iterates all terminals and calls applyConfig() on each.
     *
     * @note MESSAGE THREAD.
     */
    void applyConfig();

    /**
     * @brief Increase the terminal zoom level.
     *
     * Forwards to getActiveTerminal().
     *
     * @note MESSAGE THREAD.
     */
    void increaseZoom();

    /**
     * @brief Decrease the terminal zoom level.
     *
     * Forwards to getActiveTerminal().
     *
     * @note MESSAGE THREAD.
     */
    void decreaseZoom();

    /**
     * @brief Reset the terminal zoom level to default.
     *
     * Forwards to getActiveTerminal().
     *
     * @note MESSAGE THREAD.
     */
    void resetZoom();

    /**
     * @brief Get the owner container for all terminal components.
     *
     * Returns a reference to the terminals owner so GLRenderer can iterate
     * all GL components for rendering.
     *
     * @return Reference to the GLComponent owner.
     * @note MESSAGE THREAD.
     */
    jreng::Owner<jreng::GLComponent>& getComponents() noexcept { return terminals; }

    /**
     * @brief Get the custom LookAndFeel for the tab bar.
     * @return Reference to the tab LookAndFeel instance.
     * @note MESSAGE THREAD.
     */
    LookAndFeel& getTabLookAndFeel() noexcept { return tabLookAndFeel; }

    /**
     * @brief Refreshes the look and feel by triggering a repaint.
     *
     * Called after LookAndFeel::setColours() to refresh the UI.
     *
     * @note MESSAGE THREAD.
     */
    void refreshLookAndFeel() noexcept;

    /**
     * @brief Reads tab.position from Config and applies the orientation.
     *
     * Maps "top", "bottom", "left", "right" to JUCE TabbedButtonBar
     * orientation. Updates tab bar depth after orientation change.
     *
     * @note MESSAGE THREAD.
     */
    void applyOrientation();

    /**
     * @brief Maps a position string to JUCE tab bar orientation.
     * @param position  One of "top", "bottom", "left", "right".
     * @return The corresponding JUCE orientation; defaults to TabsAtLeft.
     */
    static juce::TabbedButtonBar::Orientation orientationFromString (const juce::String& position);

    /**
     * @brief Handle component resize events.
     * @note MESSAGE THREAD.
     */
    void resized() override;

private:
    /**
     * @brief Get the currently active terminal component.
     *
     * Returns a pointer to the active Terminal::Component by downcasting
     * the currently visible GLComponent via static_cast.
     *
     * @return Pointer to the active terminal, or nullptr if none.
     * @note MESSAGE THREAD.
     */
    Terminal::Component* getActiveTerminal() const noexcept;

    /**
     * @brief Handle tab change events from JUCE.
     *
     * Called when the user switches tabs. Toggles visibility of terminal
     * components so only the active one is visible.
     *
     * @param newIndex The index of the newly selected tab.
     * @param name The name of the newly selected tab.
     * @note MESSAGE THREAD.
     */
    void currentTabChanged (int newIndex, const juce::String& name) override;

    /**
     * @brief Update tab bar visibility based on tab count.
     *
     * Hides the tab bar (depth 0) when only one tab exists.
     * Shows the tab bar at the height derived from the configured tab
     * font size when multiple tabs exist.
     *
     * @note MESSAGE THREAD.
     */
    void updateTabBarVisibility();

    /**
     * @brief Owner container for all terminal components.
     *
     * Stores Terminal::Component instances as base GLComponent (Pabrik pattern).
     * @note MESSAGE THREAD.
     */
    jreng::Owner<jreng::GLComponent> terminals;

    /**
     * @brief Custom look-and-feel for the tab bar.
     * @note MESSAGE THREAD.
     */
    LookAndFeel tabLookAndFeel;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Tabs)
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Terminal
