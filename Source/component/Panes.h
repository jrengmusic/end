/**
 * @file Panes.h
 * @brief Pane container that manages terminal sessions within a single tab.
 *
 * Terminal::Panes subclasses juce::Component to provide a paned layout
 * where one or more terminal sessions are hosted side-by-side or stacked.
 * Each pane owns exactly one Terminal::Display. The ValueTree hierarchy
 * is TAB > PANES > PANE > SESSION, owned by PaneManager and grafted into
 * the application state tree by the owning Tab.
 *
 * Split operations delegate to splitImpl. Leaf lookup delegates to
 * jreng::PaneManager::findLeaf. Panes are owned by
 * jreng::Owner<PaneComponent>; each pane's componentID is its UUID.
 *
 * @see Terminal::Display
 * @see Terminal::Tabs
 * @see jreng::PaneManager
 */

#pragma once
#include <JuceHeader.h>
#include "PaneComponent.h"
#include "TerminalDisplay.h"
#include "../terminal/logic/Processor.h"

namespace Terminal
{ /*____________________________________________________________________________*/
/**
 * @class Panes
 * @brief Pane container managing terminal sessions within a single tab.
 *
 * A Panes instance is created per tab and owns all pane components
 * for that tab. createTerminal() adds a new session and returns
 * its UUID.
 *
 * @note MESSAGE THREAD — all methods.
 */
class Panes : public juce::Component
{
public:
    /**
     * @brief Construct a new Panes container.
     * @param font  Font instance providing metrics, shaping, and rasterisation.
     * @note MESSAGE THREAD.
     */
    explicit Panes (jreng::Typeface& font, jreng::Typeface& whelmedBodyFont, jreng::Typeface& whelmedCodeFont);

    /**
     * @brief Destructor.
     * @note MESSAGE THREAD.
     */
    ~Panes() override;

    /**
     * @brief Create a new terminal session in this pane.
     *
     * In host mode: creates a session via Nexus::Session and calls Session::createDisplay().
     * In client mode: spawns or attaches a remote session via Nexus::Client and
     * creates a client-mode Display backed by a hosted Terminal::Processor.
     *
     * Wires callbacks, registers with PaneManager via addLeaf, and grafts
     * the session ValueTree into the corresponding PANE node.
     *
     * @param workingDirectory  Initial cwd for the shell. Empty = inherit parent cwd.
     * @param uuid              UUID hint for state restoration. In client mode, if this
     *                          UUID is live on the host the session is attached; otherwise
     *                          a new session is spawned. Empty = always spawn new.
     * @return The UUID of the newly created terminal (its componentID).
     * @note MESSAGE THREAD.
     */
    juce::String createTerminal (const juce::String& workingDirectory = {},
                                 const juce::String& uuid = {});

    /**
     * @brief Create a new Whelmed markdown viewer pane and open the given file.
     *
     * @param file  The .md file to open.
     * @return The UUID of the newly created Whelmed pane (its componentID).
     * @note MESSAGE THREAD.
     */
    juce::String createWhelmed (const juce::File& file);

    void closeWhelmed();

    /**
     * @brief Access the owned panes for GL iteration.
     *
     * Returns a reference to the owner container so that the GL renderer
     * can iterate all panes without transferring ownership.
     *
     * @return Reference to the pane owner container.
     * @note MESSAGE THREAD.
     */
    jreng::Owner<PaneComponent>& getPanes() noexcept;

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
     * @brief Callback invoked when a shell exits and all panes in this tab are closed.
     *
     * Fired by setTerminalCallbacks after closePane empties the terminal list.
     * The owning Tabs wires this to its tab-removal logic.
     *
     * @note MESSAGE THREAD (via callAsync).
     */
    std::function<void()> onLastPaneClosed;

    /**
     * @brief Callback invoked when a .md file link is activated.
     *
     * Set by the owning Tabs to spawn a Whelmed pane in the active tab.
     *
     * @note MESSAGE THREAD.
     */
    std::function<void (const juce::File&)> onOpenMarkdown;

    /**
     * @brief Close the pane with the given uuid.
     *
     * Removes the SESSION child from the PANE node, removes the terminal
     * from the owner, removes the paired resizer bar, calls
     * paneManager.remove, and relays out.
     *
     * @param uuid The componentID of the terminal to close.
     * @note MESSAGE THREAD.
     */
    void closePane (const juce::String& uuid);

    /**
     * @brief Split a specific target pane, using an explicit new UUID and cwd.
     *
     * Used by the restore walker in MainComponent to reconstruct a saved split
     * tree. Also used internally by splitImpl() for keyboard-shortcut splits.
     *
     * @param targetUuid  UUID of the existing leaf to split.
     * @param newUuid     UUID hint for the new terminal. Empty = generate fresh.
     * @param cwd         Working directory for the new terminal. Empty = inherit.
     * @param direction   "vertical" for left/right divider; "horizontal" for top/bottom.
     * @param isVertical  True when the resizer bar is vertical (direction == "vertical").
     * @note MESSAGE THREAD.
     */
    void splitAt (const juce::String& targetUuid,
                  const juce::String& newUuid,
                  const juce::String& cwd,
                  const juce::String& direction,
                  bool isVertical);

    /**
     * @brief Split the active pane into side-by-side columns.
     *
     * Delegates to splitImpl ("vertical", true).
     * @note MESSAGE THREAD.
     */
    void splitHorizontal();

    /**
     * @brief Split the active pane into stacked rows.
     *
     * Delegates to splitImpl ("horizontal", false).
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

    // =========================================================================
    /** @name Terminal lifecycle helpers
     *
     * These two helpers centralise session creation and teardown.  All
     * other call sites (createTerminal, splitImpl, MainComponent popup,
     * Tabs::closeActiveTab, Popup::removePopupSession) delegate here.
     * Mode routing (local / daemon / client) is owned by Nexus::Session.
     * @{ */

    /**
     * @brief Result type returned by buildTerminal.
     *
     * `processor` is always non-null and is used to call createDisplay.
     * Processor ownership lives in Nexus::Session (local/daemon) or the
     * Session-internal Client (client mode).  Callers receive only a non-owning pointer.
     */
    struct CreatedTerminal
    {
        juce::String uuid;
        Terminal::Processor* processor;
    };

    /**
     * @brief Constructs a terminal Processor via the unified Nexus::Session API.
     *
     * Delegates to `Nexus::Session::getContext()->create()` which routes
     * internally through local, daemon, or client mode.  Returns a non-owning
     * pointer into the Session (or Client) pool.
     *
     * @param shell      Shell program path.  Empty = use Config default.
     * @param args       Shell arguments.  Empty = use Config default.
     * @param cwd        Initial working directory.  Empty = inherit.
     * @param uuidHint   UUID hint (nexus restore).  Empty = generate new.
     * @return CreatedTerminal with uuid and non-owning processor*.
     * @note MESSAGE THREAD.
     */
    static CreatedTerminal buildTerminal (const juce::String& shell,
                                          const juce::String& args,
                                          const juce::String& cwd,
                                          const juce::String& uuidHint);

    /**
     * @brief Tears down a terminal session.
     *
     * Delegates to `Nexus::Session::getContext()->remove(uuid)` which handles
     * mode routing internally — local, daemon, and client modes are all covered
     * by Session's unified remove() implementation.
     *
     * Display must be erased before this call so no dangling reference exists
     * at destruction time.
     *
     * @param uuid  The UUID of the terminal to tear down.
     * @note MESSAGE THREAD.
     */
    static void teardownTerminal (const juce::String& uuid);

    /** @} */

    /**
     * @brief Handle component resize events.
     *
     * Delegates to PaneManager::layout to recursively position all terminals
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
    void setTerminalCallbacks (Terminal::Display* terminal);

    /**
     * @brief Shared implementation for splitHorizontal and splitVertical.
     *
     * @param direction  PaneManager direction string: "vertical" produces a
     *                   left/right divider (side-by-side columns); "horizontal"
     *                   produces a top/bottom divider (stacked rows).
     * @param isVertical True when the resizer bar is vertical (splitHorizontal).
     * @note MESSAGE THREAD.
     */
    void splitImpl (const juce::String& direction, bool isVertical);

    jreng::Typeface& font;
    jreng::Typeface& whelmedBodyFont;
    jreng::Typeface& whelmedCodeFont;
    jreng::Owner<PaneComponent> panes;
    jreng::PaneManager paneManager;
    jreng::Owner<jreng::PaneResizerBar> resizerBars;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Panes)
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Terminal
