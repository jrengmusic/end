/**
 * @file end/Panes.h
 * @brief Per-tab pane container — owns PaneManager, PaneViews, and resizer bars.
 */
#pragma once
#include <JuceHeader.h>
#include "end/PaneView.h"
#include "terminal/View.h"
#include "Nexus.h"
#include "lookAndFeel/LookAndFeel.h"

namespace end
{
/*____________________________________________________________________________*/

/** @class Panes
 *  @brief Per-tab split-pane container — IS the TAB tree in the model.
 *
 *  Inherits jam::Model::Component (IDtype::tab) so it owns a TAB ValueTree.
 *  Sets Name and componentID directly in the constructor body. Owns PaneManager
 *  (binary tree), PaneView pool, resizer
 *  bar pool, and Attachments for each PaneView child.
 *  PaneManager::layout() positions PaneViews and reconciles resizer bars on resized().
 */
class Panes
    : public juce::Component
    , public jam::Model::Component
{
public:
    /** @brief Constructs the container — pane creation deferred to createPane().
     *  @param uuid   UUID for this TAB tree.
     *  @param model  Model reference — forwarded to Component and child Attachments.
     */
    Panes (jam::UUID uuid, jam::Model& model);

    /** @brief Creates the initial pane. Call AFTER Panes is parented and showing.
     *  @param uuid  UUID for the first PaneView leaf.
     */
    void createPane (jam::UUID uuid);

    void resized() override;
    void visibilityChanged() override;

    /** @brief Applies resizer bar thickness from the LAF when theme properties
     *  change. Reads end::LookAndFeel::getPaneResizerBarSize() and applies it
     *  to paneManager.
     */
    void lookAndFeelChanged() override;

    /** @brief Splits the pane identified by uuid.
     *  @param uuid       Existing pane to split.
     *  @param direction  ID::vertical or ID::horizontal.
     */
    void split (jam::UUID uuid, const juce::Identifier& direction);

    /** @brief Removes a pane and promotes its sibling.
     *  @param uuid  Pane to remove.
     */
    void removePane (jam::UUID uuid);

    /** @brief Moves keyboard focus to the adjacent pane in the given direction.
     *  @param direction One of ID::paneLeft, ID::paneRight, ID::paneUp, ID::paneDown.
     *  Does nothing if no adjacent pane exists in that direction.
     */
    void focusPane (const juce::Identifier& direction);

    /** @brief Returns the number of pane views. */
    int getPaneCount() const noexcept;

    /** @brief Returns the UUID of the first pane, or a null UUID if no panes exist. */
    jam::UUID getFirstPaneUUID() const noexcept;

private:
    jam::PaneManager paneManager;
    jam::Owner<PaneView> panes;
    jam::Owner<jam::PaneResizerBar> resizerBars;
    jam::Owner<jam::Model::Attachment> attachments;

    /** @brief Per-direction candidate/distance predicate — keyed by ID::paneLeft/
     *  paneRight/paneUp/paneDown. Populated once by registerEvents().
     *  Each entry compares a candidate pane's bounds against the focused pane's
     *  bounds and returns {isCandidate, distance} for focusPane()'s
     *  nearest-pane search (MANIFESTO L — 3-branch max, replaced with a
     *  direct lookup).
     */
    jam::Function::Map<juce::Identifier, std::pair<bool, int>> events;

    /** @brief Populates events with the four directional predicates.
     *  Called once at construction. Defined in Panes.cpp.
     */
    void registerEvents();

    /** @brief Returns the pane currently holding keyboard focus.
     *  @return The focused PaneView, or nullptr if none holds focus.
     */
    PaneView* findFocusedPane() const;

    /** @brief Finds the nearest pane to @p focused in the given direction,
     *  using events's per-direction candidate/distance predicate.
     *  @param direction One of ID::paneLeft, ID::paneRight, ID::paneUp, ID::paneDown.
     *  @param focused   The pane to search from; excluded from candidates.
     *  @return The nearest candidate pane, or nullptr if none qualifies.
     */
    PaneView* findNearestPane (const juce::Identifier& direction, PaneView* focused) const;

    /** @brief Constructs, parents, and attaches a new PaneView/terminal::View
     *  pair — the sequence shared by createPane() and split(). Session
     *  creation, View construction, addChildComponent, Attachment, and
     *  panes ownership all happen here; visibility is set from
     *  isShowing(). Caller wires PaneManager topology (addLeaf/split) and
     *  resized().
     *  @param uuid  Identifier for the new pane.
     *  @return Reference to the constructed terminal::View.
     */
    terminal::View& addPaneView (jam::UUID uuid);

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Panes)
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace end
