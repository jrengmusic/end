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
 * Each tab owns one Panes instance via `jam::Owner<Panes>`. The ValueTree
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
#include "TerminalDisplay.h"
#include "Panes.h"
#include "LookAndFeel.h"
#include "../terminal/rendering/ImageAtlas.h"

namespace Whelmed { class Component; }

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
class Tabs : public jam::TabbedComponent
           , private juce::FocusChangeListener
           , private juce::Value::Listener
{
public:
    /**
     * @brief Construct a new Tabs container.
     * @param font          Font spec carrying resolved typeface; provides metrics, shaping, and rasterisation.
     * @param packer        Glyph packer; owns the atlas and rasterization.
     * @param glAtlas       GL texture handle store; threaded through to Screen<GLContext>.
     * @param graphicsAtlas CPU atlas image store; threaded through to Screen<GraphicsContext>.
     * @param imageAtlas    Inline image atlas; threaded through to Screen for staged GPU upload.
     * @param orientation   The tab bar orientation (top, bottom, left, or right).
     * @note MESSAGE THREAD.
     */
    Tabs (jam::Font& font,
          jam::Glyph::Packer& packer,
          jam::gl::GlyphAtlas& glAtlas,
          jam::GraphicsAtlas& graphicsAtlas,
          Terminal::ImageAtlas& imageAtlas,
          jam::TabbedButtonBar::Orientation orientation);

    /**
     * @brief Destructor.
     * @note MESSAGE THREAD.
     */
    ~Tabs() override;

    /**
     * @brief Callback invoked when any terminal needs a repaint.
     *
     * This callback is set by MainComponent to trigger Window::triggerRepaint(), which
     * delegates to the internal renderer if present and is a no-op in CPU mode.
     * It is forwarded to each new terminal's own onRepaintNeeded callback.
     * MainComponent's binding additionally refreshes `StatusBarOverlay::updateHintInfo()`
     * with the active terminal's hint state.
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
     * @brief Walks saved TAB nodes, creates tabs, and replays splits.
     *
     * Caller must pass a deep copy of the saved TABS tree detached from
     * the live AppState tree — this method walks it directly without aliasing.
     *
     * @param savedTabs   Deep copy of the TABS node from `end-<id>.state`.
     * @param contentRect Chrome-subtracted content rect for dim computation.
     * @note MESSAGE THREAD.
     */
    void restore (juce::ValueTree savedTabs, juce::Rectangle<int> contentRect);

    /**
     * @brief Create and add a new terminal tab with explicit cwd, UUID hint, and spawn dims.
     *
     * Used by the state restoration walker to create a tab whose first
     * terminal attaches to the given UUID (if live on the host) or spawns a new
     * session in @p workingDirectory, with deterministic PTY dimensions derived
     * from the saved split tree rather than zero/fallback bounds.
     *
     * @param workingDirectory  Initial cwd for the first terminal.
     * @param uuid              UUID hint passed through to Panes::createTerminal().
     * @param cols              Terminal column count. Must be > 0.
     * @param rows              Terminal row count. Must be > 0.
     * @note MESSAGE THREAD.
     */
    void addNewTab (const juce::String& workingDirectory, const juce::String& uuid,
                    int cols, int rows);

    /**
     * @brief Close the currently active tab and its terminal.
     *
     * If the last tab is closed, the caller (MainComponent) handles quit.
     *
     * @note MESSAGE THREAD.
     */
    void closeActiveTab();

    /**
     * @brief Close the pane identified by @p uuid across all tabs.
     *
     * Iterates all Panes instances to locate which tab owns the UUID, calls
     * closePane on that Panes instance, and removes the tab if it becomes empty.
     * Handles nexus save/delete identically to the onLastPaneClosed path.
     *
     * @param uuid  The session UUID to close.
     * @note MESSAGE THREAD.
     */
    void closeSession (const juce::String& uuid);

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
     * Returns a pointer to the focused Terminal::Display.
     *
     * @return Pointer to the active terminal, or nullptr if none.
     * @note MESSAGE THREAD.
     */
    Terminal::Display* getActiveTerminal() const noexcept;

    /**
     * @brief Returns the active Panes' pane owner for GL iteration.
     *
     * Returns a reference to a static empty owner if no active pane exists.
     *
     * @return Reference to the active pane owner, or a static empty owner.
     * @note MESSAGE THREAD.
     */
    jam::Owner<PaneComponent>& getPanes() noexcept;

    /**
     * @brief Returns the Panes instance for the currently active tab.
     *
     * Used by MainComponent's restore walker to call splitAt and read getState()
     * on the newly created Panes immediately after addNewTab().
     *
     * @return Pointer to the active Panes, or nullptr if none.
     * @note MESSAGE THREAD.
     */
    Panes* getActivePanes() const noexcept;

    /**
     * @brief Opens the given .md file as a Whelmed pane in the active tab.
     *
     * Delegates to the active Panes::createWhelmed. Does nothing if no tab is active.
     *
     * @param file  The .md file to open.
     * @note MESSAGE THREAD.
     */
    void openMarkdown (const juce::File& file);

    /** @brief Shows an inline rename editor on the specified tab.
        @param tabIndex Zero-based tab index.
        @note MESSAGE THREAD.
    */
    void showRenameEditor (int tabIndex);

    /** @brief Renames the active tab programmatically.
        Empty name clears the user override (reverts to auto name).
        @param name New tab name, or empty to clear override.
        @note MESSAGE THREAD.
    */
    void renameActiveTab (const juce::String& name);

    /**
     * @brief Returns the active pane component regardless of type. @note MESSAGE THREAD.
     */
    PaneComponent* getActivePane() const noexcept;

    /**
     * @brief Returns `true` if the active pane has a non-degenerate selection.
     *
     * Forwards to the active PaneComponent::hasSelection().
     * Returns `false` if there is no active pane.
     *
     * @return `true` if a selection is active and can be copied.
     * @note MESSAGE THREAD.
     */
    bool hasSelection() const noexcept;

    /**
     * @brief Copy the current selection to clipboard.
     *
     * Forwards to the active PaneComponent::copySelection().
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
     * Increases zoom by `lua::Engine::zoomStep` and applies to all panes across all tabs.
     *
     * @note MESSAGE THREAD.
     */
    void increaseZoom();

    /**
     * @brief Decrease the terminal zoom level.
     *
     * Decreases zoom by `lua::Engine::zoomStep` and applies to all panes across all tabs.
     *
     * @note MESSAGE THREAD.
     */
    void decreaseZoom();

    /**
     * @brief Reset the terminal zoom level to default.
     *
     * Resets zoom to the configured default and applies to all panes across all tabs.
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

    /**
     * @brief Split the active pane at a given ratio.
     * @param direction  PaneManager direction string.
     * @param isVertical True when the resizer bar is vertical.
     * @param ratio      Split ratio (0.0–1.0).
     * @note MESSAGE THREAD.
     */
    void splitActiveWithRatio (const juce::String& direction, bool isVertical, double ratio);

    /** @brief Focus the nearest pane to the left of the active pane. @note MESSAGE THREAD. */
    void focusPaneLeft();

    /** @brief Focus the nearest pane below the active pane. @note MESSAGE THREAD. */
    void focusPaneDown();

    /** @brief Focus the nearest pane above the active pane. @note MESSAGE THREAD. */
    void focusPaneUp();

    /** @brief Focus the nearest pane to the right of the active pane. @note MESSAGE THREAD. */
    void focusPaneRight();

    /** @brief Switches all terminals to the given renderer type. */
    void switchRenderer (App::RendererType type);

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
    static jam::TabbedButtonBar::Orientation orientationFromString (const juce::String& position);

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

    /** @brief Tracks focus changes to update the active terminal UUID in AppState. */
    void globalFocusChanged (juce::Component* focusedComponent) override;

    /** @brief Updates the tab name when the bound displayName value changes. */
    void valueChanged (juce::Value& value) override;

    /** @brief Shows a rename editor when the user right-clicks a tab. */
    void popupMenuClickOnTab (int tabIndex, const juce::String& tabName) override;

    /** @brief Sets the active terminal UUID and grabs focus for the last terminal in @p active. */
    void focusLastTerminal (Panes* active);

    /**
     * @brief Returns the content rect available for Panes given a tab-bar depth.
     *
     * Uses jam::PaneManager::resizerBarSize as SSOT — matches layoutNode arithmetic exactly.
     * Falls back to AppState window size when getLocalBounds() is empty (pre-layout on first spawn).
     *
     * @param tabBarDepth  Tab-bar pixel depth to subtract from the base bounds.
     * @return Pixel rect available for the active Panes component.
     * @note MESSAGE THREAD.
     */
    juce::Rectangle<int> computeContentRect (int tabBarDepth) const noexcept;

    jam::Font& font;
    jam::Glyph::Packer& packerRef;
    jam::gl::GlyphAtlas& glAtlasRef;
    jam::GraphicsAtlas& graphicsAtlasRef;
    Terminal::ImageAtlas& imageAtlasRef;
    juce::Value tabName;
    jam::Owner<Panes> panes;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Tabs)
};

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Terminal
