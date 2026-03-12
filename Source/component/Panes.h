/**
 * @file Panes.h
 * @brief Pane container that manages terminal sessions within a single tab.
 *
 * Terminal::Panes subclasses juce::Component to provide a paned layout
 * where one or more terminal sessions are hosted side-by-side or stacked.
 * Each pane owns exactly one Terminal::Component. The ValueTree defines
 * hierarchy (TAB > PANES > PANE > SESSION).
 *
 * @par Ownership
 * Terminals are owned by `jreng::Owner<Terminal::Component>`. Each terminal's
 * componentID is set to its UUID for lookup. The PANES ValueTree is owned by
 * PaneManager and grafted into the application state tree by the owning Tab.
 *
 * @see Terminal::Component
 * @see Terminal::Tabs
 */

#pragma once
#include <JuceHeader.h>
#include "TerminalComponent.h"

namespace Terminal
{ /*____________________________________________________________________________*/
/**
 * @class Panes
 * @brief Pane container managing terminal sessions within a single tab.
 *
 * A Panes instance is created per tab and owns all Terminal::Component
 * instances for that tab. createTerminal() adds a new session and returns
 * its UUID.
 *
 * @note MESSAGE THREAD — all methods.
 */
class Panes : public juce::Component
{
public:
    /**
     * @brief Construct a new Panes container.
     * @note MESSAGE THREAD.
     */
    Panes();

    /**
     * @brief Destructor.
     * @note MESSAGE THREAD.
     */
    ~Panes() override;

    /**
     * @brief Create a new terminal session in this pane.
     *
     * Creates a new Terminal::Component, adds it to the terminals owner,
     * wires callbacks, registers it with PaneManager via addLeaf, and grafts
     * its SESSION ValueTree into the corresponding PANE node.
     *
     * @return The UUID of the newly created terminal (its componentID).
     * @note MESSAGE THREAD.
     */
    juce::String createTerminal();

    /**
     * @brief Access the owned terminals for GL iteration.
     *
     * Returns a reference to the owner container so that the GL renderer
     * can iterate all terminals without transferring ownership.
     *
     * @return Reference to the terminal owner container.
     * @note MESSAGE THREAD.
     */
    jreng::Owner<Terminal::Component>& getTerminals() noexcept;

    /**
     * @brief Access the PANES ValueTree for attachment to AppState.
     *
     * Delegates to PaneManager::getState(). The returned tree has type PANES
     * and contains the binary split-pane hierarchy.
     *
     * @return Reference to the PANES ValueTree owned by PaneManager.
     * @note MESSAGE THREAD.
     */
    juce::ValueTree& getState() noexcept;

    /**
     * @brief Callback invoked when any terminal needs a repaint.
     *
     * Set by the owning Tabs (which receives it from MainComponent).
     * Forwarded to each new terminal's own onRepaintNeeded callback.
     *
     * @note MESSAGE THREAD.
     */
    std::function<void()> onRepaintNeeded;

    /**
     * @brief Close the pane with the given uuid.
     *
     * Removes the terminal, its resizer bar, and updates PaneManager.
     * 
     * @param uuid The componentID of the terminal to close.
     * @note MESSAGE THREAD.
     */
    void closePane (const juce::String& uuid);

    /**
     * @brief Split the active pane horizontally (side by side columns).
     * @note MESSAGE THREAD.
     */
    void splitHorizontal();

    /**
     * @brief Split the active pane vertically (stacked rows).
     * @note MESSAGE THREAD.
     */
    void splitVertical();

    /**
     * @brief Focus the nearest pane in the given direction.
     *
     * Spatial lookup based on terminal component bounds.
     *
     * @param deltaX  -1 for left, +1 for right, 0 for vertical.
     * @param deltaY  -1 for up, +1 for down, 0 for horizontal.
     * @note MESSAGE THREAD.
     */
    void focusPane (int deltaX, int deltaY);

    /**
     * @brief Handle component resize events.
     *
     * Delegates to PaneManager::layOut to recursively position all terminals
     * and resizer bars according to the binary split tree.
     *
     * @note MESSAGE THREAD.
     */
    void resized() override;

    /**
     * @brief Propagates visibility to child terminals.
     *
     * When Panes becomes visible, makes all terminals visible.
     * When hidden, hides all terminals.
     *
     * @note MESSAGE THREAD.
     */
    void visibilityChanged() override;

private:
    /**
     * @brief Wire a terminal's callbacks after creation.
     * @param terminal  The terminal to wire.
     * @note MESSAGE THREAD.
     */
    void setTerminalCallbacks (Terminal::Component* terminal);

    /**
     * @brief Find a PANE node by uuid in the given tree.
     *
     * Recursively walks the tree. Returns an invalid ValueTree if not found.
     *
     * @param root  Root of the tree to search.
     * @param uuid  UUID to match against the "uuid" property.
     * @return The matching PANE ValueTree, or an invalid ValueTree.
     * @note MESSAGE THREAD.
     */
    static juce::ValueTree findPaneNode (const juce::ValueTree& root, const juce::String& uuid);

    jreng::Owner<Terminal::Component> terminals;
    jreng::PaneManager paneManager;
    jreng::Owner<jreng::PaneResizerBar> resizerBars;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Panes)
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Terminal
