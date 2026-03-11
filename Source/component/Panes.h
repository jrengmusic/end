/**
 * @file Panes.h
 * @brief Pane container that manages terminal sessions within a single tab.
 *
 * Terminal::Panes subclasses juce::Component to provide a paned layout
 * where one or more terminal sessions are hosted side-by-side or stacked.
 * Each pane owns exactly one Terminal::Component. The ValueTree defines
 * hierarchy (TAB > PANES > SESSION).
 *
 * @par Ownership
 * Terminals are owned by `jreng::Owner<Terminal::Component>`. Each terminal's
 * componentID is set to its UUID for lookup. The PANES ValueTree is grafted
 * into the application state tree by the owning Tab.
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
     * wires callbacks, and appends its SESSION ValueTree to the PANES state.
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
     * The returned tree has type App::ID::PANES and accumulates SESSION
     * children as terminals are created.
     *
     * @return Reference to the PANES ValueTree.
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
     * @brief Split the active pane vertically (side by side columns).
     * @note MESSAGE THREAD.
     */
    void splitVertical();

    /**
     * @brief Split the active pane horizontally (stacked rows).
     * @note MESSAGE THREAD.
     */
    void splitHorizontal();

    /**
     * @brief Handle component resize events.
     *
     * Lays out all visible terminals within the component bounds.
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
     * @brief Rebuild the PaneManager layout and resizer bars for the current terminal count.
     * @note MESSAGE THREAD.
     */
    void rebuildLayout();

    jreng::Owner<Terminal::Component> terminals;
    jreng::PaneManager paneManager;
    jreng::Owner<jreng::PaneResizerBar> resizerBars;
    juce::ValueTree state;

    bool isVertical { true };
    bool hasSplitDirection { false };
    static constexpr int resizerBarSize { 4 };

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Panes)
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Terminal
