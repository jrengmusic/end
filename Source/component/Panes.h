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
 * Split operations delegate to splitActive. Leaf lookup delegates to
 * jam::PaneManager::findLeaf. Panes are owned by
 * jam::Owner<PaneComponent>; each pane's componentID is its UUID.
 *
 * @see Terminal::Display
 * @see Terminal::Tabs
 * @see jam::PaneManager
 */

#pragma once
#include <JuceHeader.h>
#include <utility>
#include "PaneComponent.h"
#include "TerminalDisplay.h"
#include "../terminal/logic/Processor.h"
#include "../terminal/rendering/ImageAtlas.h"

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
     * @param font          Font spec carrying resolved typeface; provides metrics, shaping, and rasterisation.
     * @param packer        Glyph packer; owns the atlas and rasterization.
     * @param glAtlas       GL texture handle store; threaded through to Screen<GLContext>.
     * @param graphicsAtlas CPU atlas image store; threaded through to Screen<GraphicsContext>.
     * @param imageAtlas    Inline image atlas; threaded through to Display for staged GPU upload.
     * @note MESSAGE THREAD.
     */
    Panes (jam::Font& font, jam::Glyph::Packer& packer, jam::gl::GlyphAtlas& glAtlas,
           jam::GraphicsAtlas& graphicsAtlas, Terminal::ImageAtlas& imageAtlas);

    /**
     * @brief Destructor.
     * @note MESSAGE THREAD.
     */
    ~Panes() override;

    /**
     * @brief Converts a pixel rect into terminal cell dimensions.
     *
     * Pure math: subtracts terminal padding from the rect, divides by
     * effective cell width/height. Used by the restore walker to
     * pre-compute per-pane dims and by splitActive for fresh splits.
     *
     * @param paneRect    Target pixel rect for the pane (AFTER chrome subtracted).
     * @param font        Font spec carrying resolved typeface; provides cell metrics.
     * @return (cols, rows). jasserts cols > 0 and rows > 0.
     * @note Pure math — no instance state. noexcept.
     */
    static std::pair<int, int> cellsFromRect (juce::Rectangle<int> paneRect,
                                              jam::Font& font) noexcept;

    /**
     * @brief Splits a pixel rect by direction and ratio.
     *
     * Pure math: given a parent rect, a direction string ("vertical" or
     * "horizontal" — matching PaneManager idDirection semantics), and a
     * ratio in (0,1), returns the two child rects after the split. The
     * first rect is the "target" (left/top), the second is the "new" (right/bottom).
     * Uses jam::PaneManager::resizerBarSize as SSOT — matches layoutNode arithmetic exactly.
     *
     * @param parent     Parent pixel rect.
     * @param direction  "vertical" or "horizontal" (matches PaneManager idDirection).
     * @param ratio      Split ratio in (0,1).
     * @return {targetRect, newRect}.
     * @note Pure math — no instance state. noexcept.
     */
    static std::pair<juce::Rectangle<int>, juce::Rectangle<int>>
        splitRect (juce::Rectangle<int> parent,
                   const juce::String& direction,
                   double ratio) noexcept;

    /**
     * @brief Create a new terminal session in this pane.
     *
     * In host mode: opens a session via `Nexus::create` and calls `Session::createDisplay()`.
     * In client mode: spawns or attaches a remote session via `Interprocess::Link` and
     * creates a client-mode Display backed by a hosted Terminal::Processor.
     *
     * Wires callbacks, registers with PaneManager via addLeaf, and grafts
     * the session ValueTree into the corresponding PANE node.
     *
     * @param workingDirectory  Initial cwd for the shell. Empty = inherit parent cwd.
     * @param uuid              UUID hint for state restoration. In client mode, if this
     *                          UUID is live on the host the session is attached; otherwise
     *                          a new session is spawned. Empty = always spawn new.
     * @param cols              Terminal column count. Must be > 0.
     * @param rows              Terminal row count. Must be > 0.
     * @return The UUID of the newly created terminal (its componentID).
     * @note MESSAGE THREAD.
     */
    juce::String createTerminal (const juce::String& workingDirectory,
                                 const juce::String& uuid,
                                 int cols,
                                 int rows);

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
    jam::Owner<PaneComponent>& getPanes() noexcept;

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
     * @brief Callback invoked when an image file link is activated.
     *
     * Set by the owning Tabs to handle inline image rendering in the active tab.
     *
     * @note MESSAGE THREAD.
     */
    std::function<void (const juce::File&)> onOpenImage;

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
     * tree. Also used internally by splitActive() for keyboard-shortcut splits.
     *
     * @param targetUuid  UUID of the existing leaf to split.
     * @param newUuid     UUID hint for the new terminal. Empty = generate fresh.
     * @param cwd         Working directory for the new terminal. Empty = inherit.
     * @param direction   "vertical" for left/right divider; "horizontal" for top/bottom.
     * @param isVertical  True when the resizer bar is vertical (direction == "vertical").
     * @param cols        Terminal column count for the new pane. Must be > 0.
     * @param rows        Terminal row count for the new pane. Must be > 0.
     * @note MESSAGE THREAD.
     */
    void splitAt (const juce::String& targetUuid,
                  const juce::String& newUuid,
                  const juce::String& cwd,
                  const juce::String& direction,
                  bool isVertical,
                  int cols,
                  int rows,
                  double ratio = 0.5);

    /**
     * @brief Split the active pane into side-by-side columns.
     *
     * Delegates to splitActive ("vertical", true).
     * @note MESSAGE THREAD.
     */
    void splitHorizontal();

    /**
     * @brief Split the active pane into stacked rows.
     *
     * Delegates to splitActive ("horizontal", false).
     * @note MESSAGE THREAD.
     */
    void splitVertical();

    /**
     * @brief Split the active pane at a given ratio.
     *
     * @param direction  PaneManager direction: "vertical" for side-by-side columns,
     *                   "horizontal" for stacked rows.
     * @param isVertical True when the resizer bar is vertical (direction == "vertical").
     * @param ratio      Split ratio (0.0–1.0). 0.5 = equal halves.
     * @note MESSAGE THREAD.
     */
    void splitActiveWithRatio (const juce::String& direction, bool isVertical, double ratio);

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
    void splitActive (const juce::String& direction, bool isVertical, double ratio);

    jam::Font& font;
    jam::Glyph::Packer& packerRef;
    jam::gl::GlyphAtlas& glAtlasRef;
    jam::GraphicsAtlas& graphicsAtlasRef;
    Terminal::ImageAtlas& imageAtlasRef;
    jam::Owner<PaneComponent> panes;
    jam::PaneManager paneManager;
    jam::Owner<jam::PaneResizerBar> resizerBars;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Panes)
};

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Terminal
