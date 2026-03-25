/**
 * @file Tabs.h
 * @brief Tab container that manages one Terminal::Panes instance per tab.
 *
 * Terminal::Tabs subclasses juce::TabbedComponent to provide a tabbed
 * interface where each tab hosts a Terminal::Panes component. Panes owns
 * all terminal sessions for its tab. Content components are NOT passed to
 * JUCE's addTab() — instead Panes instances are managed as direct children
 * with visibility toggling, so that hidden sessions stay alive and VBlank
 * stops naturally.
 *
 * @par Ownership
 * Each tab owns one Panes instance via `jreng::Owner<Panes>`. The ValueTree
 * defines hierarchy (TAB > PANES > SESSION).
 *
 * @par Tab bar visibility
 * The tab bar is hidden (depth 0) when only one tab exists, and shown
 * at the configured height when multiple tabs are present.
 *
 * @see Terminal::Panes
 * @see Terminal::LookAndFeel
 */

#pragma once
#include <JuceHeader.h>
#include "RendererType.h"
#include "TerminalComponent.h"
#include "Panes.h"
#include "LookAndFeel.h"

namespace Terminal
{ /*____________________________________________________________________________*/
/**
 * @class Tabs
 * @brief Tabbed container managing terminal sessions — one Panes per tab.
 *
 * Tabs are added with addNewTab() and removed with closeActiveTab().
 * Each tab owns a Panes instance that manages its terminal sessions.
 *
 * @note MESSAGE THREAD — all methods.
 */
class Tabs : public juce::TabbedComponent
           , private juce::FocusChangeListener
           , private juce::Value::Listener
{
public:
    /**
     * @brief Construct a new Tabs container.
     * @param font        Font instance providing metrics, shaping, and rasterisation.
     * @param orientation The tab bar orientation (top, bottom, left, or right).
     * @note MESSAGE THREAD.
     */
    Tabs (jreng::Typeface& font, juce::TabbedButtonBar::Orientation orientation);

    /**
     * @brief Destructor.
     * @note MESSAGE THREAD.
     */
    ~Tabs() override;

    /**
     * @brief Callback invoked when any terminal needs a repaint.
     *
     * This callback is set by MainComponent to trigger GLRenderer::triggerRepaint().
     * It is forwarded to each new terminal's own onRepaintNeeded callback.
     *
     * @note MESSAGE THREAD.
     */
    std::function<void()> onRepaintNeeded;

    /**
     * @brief Create and add a new terminal tab.
     *
     * Creates a new Panes instance, creates its first terminal, grafts the
     * PANES tree into AppState, and switches to the new tab.
     *
     * @note MESSAGE THREAD.
     */
    void addNewTab();

    /**
     * @brief Close the currently active tab and its terminal.
     *
     * If the last tab is closed, the caller (MainComponent) handles quit.
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
     * @brief Get the currently active terminal component.
     *
     * Returns a pointer to the focused Terminal::Component.
     *
     * @return Pointer to the active terminal, or nullptr if none.
     * @note MESSAGE THREAD.
     */
    Terminal::Component* getActiveTerminal() const noexcept;

    /**
     * @brief Returns the active Panes' terminal owner for GL iteration.
     *
     * Returns a reference to a static empty owner if no active pane exists.
     *
     * @return Reference to the active terminal owner, or a static empty owner.
     * @note MESSAGE THREAD.
     */
    jreng::Owner<Terminal::Component>& getTerminals() noexcept;

    /**
     * @brief Returns `true` if the active terminal has a non-degenerate selection.
     *
     * Forwards to `Terminal::Component::hasSelection()` on the active terminal.
     * Returns `false` if there is no active terminal.
     *
     * @return `true` if a selection is active and can be copied.
     * @note MESSAGE THREAD.
     */
    bool hasSelection() const noexcept;

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
     * @brief Writes raw bytes to the active terminal's PTY.
     *
     * Forwards to the active terminal's session. Does nothing if no
     * terminal is active.
     *
     * @param data  Pointer to the bytes to write.
     * @param len   Number of bytes.
     *
     * @note MESSAGE THREAD.
     */
    void writeToActivePty (const char* data, int len);

    /**
     * @brief Applies the current config to all terminal sessions.
     *
     * Iterates all terminals across all tabs and calls applyConfig() on each.
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
     * @brief Split the active tab horizontally (side by side).
     * @note MESSAGE THREAD.
     */
    void splitHorizontal();

    /**
     * @brief Split the active tab vertically (stacked).
     * @note MESSAGE THREAD.
     */
    void splitVertical();

    /** @brief Focus the nearest pane to the left of the active pane. @note MESSAGE THREAD. */
    void focusPaneLeft();

    /** @brief Focus the nearest pane below the active pane. @note MESSAGE THREAD. */
    void focusPaneDown();

    /** @brief Focus the nearest pane above the active pane. @note MESSAGE THREAD. */
    void focusPaneUp();

    /** @brief Focus the nearest pane to the right of the active pane. @note MESSAGE THREAD. */
    void focusPaneRight();

    /** @brief Switches all terminals to the given renderer type. */
    void switchRenderer (Terminal::RendererType type);

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
     * @brief Handle tab change events from JUCE.
     *
     * Called when the user switches tabs. Toggles visibility of components
     * so only the current tab's terminal is visible.
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
     * @brief Returns the Panes instance for the active tab.
     * @return Pointer to the active Panes, or nullptr if none.
     * @note MESSAGE THREAD.
     */
    Panes* getActivePanes() const noexcept;

    /** @brief Tracks focus changes to update the active terminal UUID in AppState. */
    void globalFocusChanged (juce::Component* focusedComponent) override;

    /** @brief Updates the tab name when the bound displayName value changes. */
    void valueChanged (juce::Value& value) override;

    /** @brief Sets the active terminal UUID and grabs focus for the last terminal in @p active. */
    void focusLastTerminal (Panes* active);

    jreng::Typeface& font;
    juce::Value tabName;
    jreng::Owner<Panes> panes;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Tabs)
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Terminal
