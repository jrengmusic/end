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

    /**
     * @brief Splits the pane graph at an edge, or adds a pane when the edge is invalid.
     * @param edge  Edge to split across: Id::left, Id::right, Id::top or Id::bottom. An invalid identifier adds a pane.
     */
    void addPane (const juce::Identifier& edge);

    /**
     * @brief Sets the plugin property of the focused pane to the path of a plugin file in the plugins folder of the config.
     *
     * The default is `files::evePlugin`, because the default pane holds EVE. A plugin name replaces the default.
     *
     * @param plugin  Name of the plugin file relative to the plugins folder, or an absolute path.
     */
    void loadPlugin (const juce::String& plugin = files::evePlugin);

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
     * @brief Returns the focused pane of the tab. The pane commands of ENDView act on this pane.
     * @return The focused EditorView.
     */
    EditorView& getFocusedPane();

    /**
     * @brief Sets the zoom parameter of each pane of the tab with a zoom command.
     * @param command  Zoom command id: zoomIn, zoomOut or zoomReset.
     * @param step     Zoom step that the command uses.
     */
    void setZoom (juce::CommandID command, float step);

    /**
     * @brief Shows the split or join preview while a corner drag passes splitDragThreshold, and hides it otherwise.
     *
     * The dragged corner's dominant axis (larger of |dx|, |dy|) selects horizontal or vertical. The sign of the drag
     * relative to the side of the corner selects a split (inward) or a join (outward). The preview is shown by
     * showPreview() when getEdge() returns an edge. When it returns no edge, the preview is hidden.
     *
     * @param event Mouse event. It is handled only when its original component is inside a jam::PaneComponent.
     */
    void mouseDrag (const juce::MouseEvent& event) override;

    /**
     * @brief Commits a corner drag that passes splitDragThreshold: a split when the drag is inward, a join when it is outward.
     *
     * An inward drag splits the pane graph at the cursor proportion along the edge axis, then calls loadPlugin() with its default plugin.
     * An outward drag invokes the command that getJoinAction() returns for the edge.
     *
     * @param event Mouse event that ends the gesture.
     */
    void mouseUp (const juce::MouseEvent& event) override;

    /**
     * @brief Returns the four join commands, each paired with the edge that it joins across.
     *
     * ENDView::registerJoinActions() reads this table to register one command per join direction. getJoinAction() searches it by edge.
     *
     * @return The pairs of join command and edge: joinLeft with left, joinDown with bottom, joinUp with top and joinRight with right.
     */
    static const std::array<std::pair<juce::CommandID, juce::Identifier>, 4>& getJoinDirections();

    /**
     * @brief Returns the join command that joins across an edge.
     * @param edge  Edge to join across: Id::left, Id::right, Id::top or Id::bottom.
     * @return The join command, or a default-constructed CommandID when no join command joins across that edge.
     */
    static juce::CommandID getJoinAction (const juce::Identifier& edge);

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
     * @brief Collapses the pane graph, removes the plugin of the pane, and removes the tab when no pane remains.
     *
     * The editor of the pane is already destroyed when this hook runs.
     * The matrix collapses the graph first. The session then removes the plugin of the pane.
     * When no pane remains, the call removes this tab from its session view. That call destroys this tab, so nothing follows it.
     *
     * @param uuid  Identity of the pane being removed.
     * @param row   Pane row of the removed pane. The matrix reads it to collapse the graph.
     */
    void childRemoved (jam::UUID uuid, const juce::ValueTree& row) override;

private:
    /** @brief Command manager that invokes the Area Options commands and the corner drag commands. */
    jam::SharedInstance<jam::CommandManager> commandManager { std::in_place };

    /**
     * @brief Overlay that paints the corner-drag preview of its owner tab.
     *
     * The overlay is non-opaque and does not intercept mouse clicks. showPreview() adds it to the desktop.
     */
    struct Preview : public juce::Component
    {
        /**
         * @brief Makes the overlay non-opaque and stops it from intercepting mouse clicks.
         * @param newOwner  Tab whose corner drag the overlay paints.
         */
        explicit Preview (TabView& newOwner);

        /**
         * @brief Paints the preview of the owner through TabView::paintPreview().
         * @param g  Graphics context of the paint pass.
         */
        void paint (juce::Graphics& g) override;

        /** @brief Tab whose corner drag the overlay paints. */
        TabView& owner;
    };

    /** @brief Corner-drag preview overlay of this tab. It is added to the desktop by the first showPreview() call. */
    Preview preview { *this };

    /**
     * @brief Shows the preview overlay over this tab, and adds the overlay to the desktop on the first call.
     *
     * The first call adds the overlay to the desktop with windowIgnoresMouseClicks and windowIgnoresKeyPresses, attached to
     * the native handle of the peer of this tab. Each call sets the overlay bounds to the area of the peer that covers this
     * tab, shows the overlay, brings it to front and repaints it. The call does nothing when this tab has no peer.
     */
    void showPreview();

    /**
     * @brief Paints the split or join preview of the corner drag in progress.
     *
     * The main mouse source must be dragging over a pane, and nothing is painted otherwise. The edge comes from getEdge().
     * An inward drag paints the split preview, and an outward drag paints the join preview.
     *
     * @param g  Graphics context of the overlay paint pass.
     */
    void paintPreview (juce::Graphics& g);

    /** @brief Builds the Area Options popup for the focused pane's corner menu. */
    juce::PopupMenu buildAreaOptionsMenu();

    /**
     * @brief Invokes the command of the same name as the Area Options item that a menu result selects.
     *
     * The command is invoked through commandManager for splitVertical, splitHorizontal, joinLeft, joinRight, joinUp, joinDown,
     * swapLeft, swapRight, swapUp and swapDown. A result that matches no item invokes nothing.
     *
     * @param result The chosen item ID from buildAreaOptionsMenu().
     */
    void handleAreaOptionsResult (int result);

    /**
     * @brief Paints the split preview: the cell size of each pane that the split would make, in a message overlay at the split line.
     * @param g         Graphics context for this paint pass.
     * @param edge      Edge to split across: Id::left, Id::right, Id::top or Id::bottom.
     * @param position  Split position along the axis of the edge, as a proportion of the focused pane.
     */
    void paintSplitPreview (juce::Graphics& g, const juce::Identifier& edge, float position);

    /**
     * @brief Paints the join preview: the cell size of the union of the focused pane and its neighbour across an edge.
     * @param g     Graphics context for this paint pass.
     * @param edge  Edge across which the join looks for the neighbour: Id::left, Id::right, Id::top or Id::bottom.
     */
    void paintJoinPreview (juce::Graphics& g, const juce::Identifier& edge);

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

    /**
     * @brief Answers whether an edge is crossed by a horizontal drag.
     * @param edge  Edge to test.
     * @return True for the left and the right edge.
     */
    static bool isHorizontal (const juce::Identifier& edge) noexcept;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TabView)
};
