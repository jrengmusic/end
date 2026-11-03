/**
 * @file TabView.h
 * @brief Binary-space pane graph for a single tab — hosts EditorView panes
 *        as jam::MatrixComponent children and drives split/join/swap gestures.
 */
#pragma once
#include <JuceHeader.h>
#include "ui/EditorView.h"
#include "generated/Generated.h"
#include "lookAndFeel/ENDLookAndFeel.h"

/**
 * @class TabView
 * @brief Owns one tab's binary-space pane graph — a jam::MatrixComponent
 *        whose children are EditorView panes reachable through jam::PaneEdge
 *        seams.
 *
 * Wires each minted pane's corner menu (Area Options: split, join, swap) to
 * this class's own gesture handlers, and drives the corner drag gesture that
 * previews and commits splits/joins directly on the pane surface.
 */
class TabView : public jam::MatrixComponent
{
public:
    /** @brief Items of the Area Options menu. Each value is the item ID that buildAreaOptionsMenu() adds. */
    enum class AreaOption
    {
        splitVertical = 1, ///< Splits the focused pane along a vertical seam.
        splitHorizontal,   ///< Splits the focused pane along a horizontal seam.
        joinLeft,          ///< Joins the focused pane with its left neighbour.
        joinRight,         ///< Joins the focused pane with its right neighbour.
        joinUp,            ///< Joins the focused pane with its upper neighbour.
        joinDown,          ///< Joins the focused pane with its lower neighbour.
        swapLeft,          ///< Swaps the focused pane with its left neighbour.
        swapRight,         ///< Swaps the focused pane with its right neighbour.
        swapUp,            ///< Swaps the focused pane with its upper neighbour.
        swapDown           ///< Swaps the focused pane with its lower neighbour.
    };

    /**
     * @brief Constructs the tab's pane graph over an existing tab row, registering the tab's name parameter on state.
     *
     * @param newModel      Owning model; source of registered parameters.
     * @param existingState The tab's own row, already in the Model.
     */
    TabView (jam::Model& newModel, juce::ValueTree existingState);
    /** @brief Default destructor. */
    ~TabView() override = default;

    /** @brief Mints a new pane and mounts it as a child of the graph. */
    void add();

    /** @brief Sets the focused child to the neighbouring pane in direction, when one exists.
     *  @param direction Edge to look across — Id::left/right/top/bottom.
     */
    void focusPane (const juce::Identifier& direction);

    /** @brief Joins the focused pane with its neighbour across direction.
     *  @param direction Edge to join across — Id::left/right/top/bottom.
     *  @return Identity of the pane absorbed into the surviving pane, or jam::UUID::none() when no neighbour exists.
     */
    jam::UUID join (const juce::Identifier& direction);

    /** @brief Swaps the focused pane with its neighbour across direction, when one exists.
     *  @param direction Edge to look across — Id::left/right/top/bottom.
     */
    void swap (const juce::Identifier& direction);

    /** @brief Resolves a child pane by identity.
     *  @param uuid Identity of the pane.
     *  @return The matching EditorView.
     */
    EditorView& get (jam::UUID uuid);

    /**
     * @brief Sets the zoom parameter of each pane of the tab with a zoom command.
     * @param command  Zoom command name.
     * @param step     Zoom step that the command uses.
     */
    void setZoom (const juce::Identifier& command, float step);

    /**
     * @brief Repaints the pane surface while a corner drag passes splitDragThreshold, so that paintOverChildren() draws the preview.
     *
     * The dragged corner's dominant axis (larger of |dx|, |dy|) selects horizontal or vertical. The sign of the drag
     * relative to the side of the corner selects a split (inward) or a join (outward). No preview is drawn until the
     * threshold is crossed.
     *
     * @param event Mouse event. It is handled only when its original component is inside a jam::PaneComponent.
     */
    void mouseDrag (const juce::MouseEvent& event) override;

    /**
     * @brief Commits a corner drag that passes splitDragThreshold: a split when the drag is inward, a join when it is outward.
     *
     * An inward drag splits the pane at the cursor proportion along the edge axis, then runs the loadPlugin action with
     * files::evePlugin, which loads EVE into the new pane. An outward drag runs the action that getJoinAction() returns
     * for the edge. The pane surface repaints after the gesture.
     *
     * @param event Mouse event that ends the gesture.
     */
    void mouseUp (const juce::MouseEvent& event) override;

    /**
     * @brief Paints the split or join preview of the corner drag that the main mouse source holds.
     *
     * While the main mouse source drags over a pane, an inward edge paints the split preview at the cursor position, and an
     * outward edge paints the join preview. The function paints nothing when no edge is valid.
     *
     * @param g Graphics context for this paint pass.
     */
    void paintOverChildren (juce::Graphics& g) override;

    /**
     * @brief Returns the four join actions, each paired with the edge that it joins across.
     *
     * registerJoinActions() reads this table to register one action per join direction. mouseUp() reads it through getJoinAction().
     *
     * @return The pairs of join action and edge: joinLeft with left, joinDown with bottom, joinUp with top and joinRight with right.
     */
    static const std::array<std::pair<juce::Identifier, juce::Identifier>, 4>& getJoinDirections();

    /**
     * @brief Returns the join action that joins across an edge.
     * @param edge  Edge to join across: Id::left, Id::right, Id::top or Id::bottom.
     * @return The join action, or an invalid identifier when no join action joins across that edge.
     */
    static juce::Identifier getJoinAction (const juce::Identifier& edge);

    /** @brief Minimum drag distance in pixels, either axis, before a corner
     *  drag arms a split/join preview.
     */
    static constexpr int splitDragThreshold { 8 };

protected:
    /**
     * @brief Mints an EditorView for uuid and wires its corner menu to
     *        this tab's Area Options handlers.
     *
     * @param uuid Identity to bind the new pane to.
     * @return The newly constructed EditorView, owned by the caller.
     */
    std::unique_ptr<jam::OwnedComponent> createChild (jam::UUID uuid) override;

    /**
     * @brief Removes the departing pane's PANE state row, then delegates to
     *        jam::MatrixComponent::childRemoved() for graph collapse.
     *
     * The explicit state.removeChild() call only removes the row when the
     * departing pane is the graph root — a direct child of the TAB row.
     * For a nested pane, the row's removal is left to
     * jam::MatrixComponent::childRemoved(), which collapses the pane's
     * parent EDGE subtree; juce::ValueTree::removeChild() is a no-op when
     * the row is not a direct child of state, so the explicit call above
     * never double-removes a nested pane's row.
     *
     * @param uuid Identity of the pane being removed.
     */
    void childRemoved (jam::UUID uuid) override;

private:
    /** @brief Builds the Area Options popup for the focused pane's corner menu. */
    juce::PopupMenu buildAreaOptionsMenu();

    /**
     * @brief Dispatches an Area Options menu selection to the ENDActions action of the same name.
     *
     * The action runs for splitVertical, splitHorizontal, joinLeft, joinRight, joinUp, joinDown,
     * swapLeft, swapRight, swapUp and swapDown.
     *
     * @param result The chosen item ID from buildAreaOptionsMenu().
     */
    void handleAreaOptionsResult (int result);

    /**
     * @brief Paints the split preview: the cell size of each pane that the split would make, in a message overlay at the split line.
     * @param g         Graphics context for this paint pass.
     * @param edge      Edge name of the split: the key of map::Position.
     * @param position  Split position along the axis of the edge, as a proportion of the focused pane.
     */
    void paintSplitPreview (juce::Graphics& g, const juce::String& edge, float position);

    /**
     * @brief Paints the join preview: the cell size of the union of the focused pane and its neighbour across an edge.
     * @param g     Graphics context for this paint pass.
     * @param edge  Edge name across which the join looks for the neighbour: the key of map::Position.
     */
    void paintJoinPreview (juce::Graphics& g, const juce::String& edge);

    /**
     * @brief Returns the edge that a corner drag starts across.
     * @param pane                 Pane under the drag.
     * @param mouseDownPosition    Position of the mouse-down, in the space of @p pane.
     * @param offset               Offset of the drag from the mouse-down position.
     * @return `Id::left`, `Id::right`, `Id::top` or `Id::bottom`. An invalid identifier when the drag is below splitDragThreshold or starts off a corner.
     */
    juce::Identifier getEdge (const jam::PaneComponent& pane, juce::Point<int> mouseDownPosition, juce::Point<int> offset) const;

    /**
     * @brief Answers whether a corner drag points toward the centre of the pane.
     * @param edge    Edge that the drag starts across.
     * @param offset  Offset of the drag from the mouse-down position.
     * @return True when the drag points toward the centre, which makes a split.
     */
    bool isInward (const juce::Identifier& edge, juce::Point<int> offset) const;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TabView)
};
