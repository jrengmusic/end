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
 *  @brief Per-tab split-pane container.
 *
 *  Each tab owns one Panes instance. Panes owns the PaneManager (binary tree),
 *  the PaneView pool, and the resizer bar pool. PaneManager::layout() positions
 *  PaneViews and reconciles resizer bars on every resized().
 */
class Panes : public juce::Component
{
public:
    /** @brief Constructs the container with one initial pane.
     *  @param firstPaneUUID  UUID for the first PaneView leaf.
     */
    explicit Panes (const juce::String& firstPaneUUID);

    void resized() override;

    /** @brief Splits the pane identified by uuid.
     *  @param uuid       Existing pane to split.
     *  @param direction  "vertical" (side-by-side) or "horizontal" (stacked).
     */
    void split (const juce::String& uuid, const juce::String& direction);

    /** @brief Removes a pane and promotes its sibling.
     *  @param uuid  Pane to remove.
     */
    void removePane (const juce::String& uuid);

    /** @brief Returns the number of pane views. */
    int getPaneCount() const noexcept;

private:
    jam::PaneManager paneManager;
    jam::Owner<PaneView> paneViews;
    jam::Owner<jam::PaneResizerBar> resizerBars;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Panes)
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace end
