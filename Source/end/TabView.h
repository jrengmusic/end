/**
 * @file TabView.h
 * @brief Binary-space pane graph for a single tab — hosts EditorView panes
 *        as jam::MatrixComponent children and drives split/join/swap gestures.
 */
#pragma once
#include <JuceHeader.h>
#include "end/EditorView.h"
#include "generated/Lexicon.h"
#include "end/MessageOverlay.h"
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
    /**
     * @brief Constructs the tab's pane graph, registering the tab's own
     *        name/edge/position parameters on state.
     *
     * @param uuid         Identity of this tab.
     * @param model        Owning model; source of registered parameters.
     * @param sessionState Session's own tree to adopt/append this tab's row under.
     */
    TabView (jam::UUID uuid, jam::Model& model, juce::ValueTree sessionState);
    /** @brief Default destructor. */
    ~TabView() override = default;

    /** @brief Mints and mounts a new pane as the graph's root or first child.
     *  @return Identity of the newly minted pane.
     */
    jam::UUID add();

    /** @brief Moves keyboard focus to the neighbouring pane in direction, when one exists.
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
     * @brief Resolves a pending swap-pick target, or cancels swap-pick on right-click.
     *
     * Only acts while state's edge property carries the "swap:" prefix (set by
     * handleAreaOptionsResult()'s Swap Areas entry). A popup-menu click cancels
     * the pending pick; any other click on a jam::PaneComponent swaps that pane
     * with the pick's source pane.
     */
    void mouseDown (const juce::MouseEvent& event) override;

    /**
     * @brief Tracks a corner drag past splitDragThreshold and arms a
     *        split/join preview on state's edge/position properties.
     *
     * The dragged corner's dominant axis (larger of |dx|, |dy|) selects
     * horizontal or vertical; the drag's sign relative to which side of the
     * pane the corner sits on selects inward (toward the pane's centre, a
     * split preview at the cursor's proportion) or outward (away from the
     * pane, a join preview — position is written as -1.0f as the join
     * sentinel). No preview arms until the threshold is crossed.
     *
     * @param event Mouse event; originalComponent must be a jam::PaneComponent.
     */
    void mouseDrag (const juce::MouseEvent& event) override;

    /**
     * @brief Commits the gesture armed by mouseDrag(): a split when
     *        position is a valid proportion, a join when position carries
     *        the -1.0f sentinel.
     *
     * Join dispatches through ENDActions (joinLeft/Right/Up/Down) rather
     * than calling join() directly, keeping the action registry as the
     * single dispatch point. Clears state's edge property either way.
     *
     * @param event Mouse event that ends the gesture.
     */
    void mouseUp (const juce::MouseEvent& event) override;

    /**
     * @brief Renders the active split/join/swap-pick preview overlay on top
     *        of the pane children.
     *
     * Dispatches on state's edge property: the "swap:" prefix draws the
     * swap-pick prompt; a valid position proportion draws the split preview
     * with each resulting pane's cell dimensions; the -1.0f join sentinel
     * draws the join preview over the union of the focused pane and its
     * target neighbour. No-op when edge is empty.
     *
     * @param g Graphics context for this paint pass.
     */
    void paintOverChildren (juce::Graphics& g) override;

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
     * @param uuid Identity of the pane being removed.
     */
    void childRemoved (jam::UUID uuid) override;

private:
    /** @brief Builds the Blender-style Area Options popup for the focused pane's corner menu. */
    juce::PopupMenu buildAreaOptionsMenu();

    /**
     * @brief Dispatches an Area Options menu selection.
     *
     * Splits and joins run directly through split() and ENDActions; Swap Areas
     * instead arms swap-pick by writing the "swap:" prefix plus the focused
     * pane's UUID into state's edge property, consumed by mouseDown().
     *
     * @param result The chosen item ID from buildAreaOptionsMenu().
     */
    void handleAreaOptionsResult (int result);

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TabView)
};
