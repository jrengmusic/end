/**
 * @file end/Panes.h
 * @brief Per-tab pane container — owns PaneManager, PaneViews, and resizer bars.
 */
#pragma once
#include <JuceHeader.h>
#include "end/PaneView.h"

namespace end
{
/*____________________________________________________________________________*/

/** @class Panes
 *  @brief Per-tab split-pane container — IS the TAB node in the model tree.
 *
 *  Inherits jam::Model::Component (IDtype::tab) so it owns a TAB ValueTree node.
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
    /** @brief Constructs the container with one initial pane.
     *  @param uuid   UUID for both this TAB node and the first PaneView leaf.
     *  @param model  Model reference — forwarded to Component and child Attachments.
     */
    Panes (jam::UUID uuid, jam::Model& model);

    void resized() override;

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

private:
    jam::PaneManager paneManager;
    jam::Owner<PaneView> paneViews;
    jam::Owner<jam::PaneResizerBar> resizerBars;
    jam::Owner<jam::Model::Attachment> attachments;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Panes)
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace end
