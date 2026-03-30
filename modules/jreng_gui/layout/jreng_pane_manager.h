#pragma once

#include "jreng_pane_resizer_bar.h"

namespace jreng
{ /*____________________________________________________________________________*/

//==============================================================================
/**
    Owns a binary-tree ValueTree representing the split-pane layout.

    Leaves are PANE nodes (uuid property). Internal nodes are PANES nodes
    (direction + ratio properties, exactly 2 children).

    The static layout function recursively subdivides a rectangle and calls
    setBounds on components matched by getComponentID().

    @tags{GUI}
*/
class PaneManager
{
public:
    //==============================================================================
    /** @brief Constructs with empty PANES root node. */
    PaneManager();

    /** @brief Destructor. */
    ~PaneManager();

    //==============================================================================
    /** @brief Returns the PANES root ValueTree for grafting into AppState. */
    juce::ValueTree& getState() noexcept;

    /** @brief Adds the first leaf PANE node.
        @param uuid Terminal componentID.
    */
    void addLeaf (const juce::String& uuid);

    /** @brief Splits an existing leaf into two.
        @param uuid      Existing leaf to split.
        @param newID     New leaf UUID.
        @param direction Internal divider orientation: "vertical" = left/right divider,
                         "horizontal" = top/bottom divider.
    */
    void split (const juce::String& uuid,
                const juce::String& newID,
                const juce::String& direction);

    /** @brief Removes a leaf and promotes its sibling.
        Handles both nested (grandparent exists) and root-level (parent == state) cases.
    */
    void remove (const juce::String& uuid);

    //==============================================================================
    /** @brief Returns the current pixel position of the divider for the given split node.
        Computed from stored bounds and ratio.
    */
    int getItemCurrentPosition (const juce::ValueTree& splitNode) const;

    /** @brief Updates the split ratio from a new pixel position.
        Clamps ratio to [0.1, 0.9].
    */
    void setItemPosition (const juce::ValueTree& splitNode, int newPosition);

    //==============================================================================
    /** @brief Recursively subdivides bounds according to the binary tree, positioning
        components and resizer bars. Matches components by componentID, resizer bars
        by splitNode identity.
    */
    template <typename ComponentType>
    static void layout (const juce::ValueTree& state,
                        juce::Rectangle<int> bounds,
                        Owner<ComponentType>& components,
                        Owner<PaneResizerBar>& resizerBars)
    {
        layoutNode (state, bounds, components, resizerBars);
    }

    //==============================================================================
    /** @brief Recursively finds a PANE leaf by UUID.
        @param node Starting node for the search.
        @param uuid UUID to search for.
        @return The matching PANE ValueTree, or invalid if not found.
    */
    static juce::ValueTree findLeaf (juce::ValueTree node, const juce::String& uuid);

private:
    //==============================================================================
    template <typename ComponentType>
    static void layoutNode (const juce::ValueTree& node,
                            juce::Rectangle<int> bounds,
                            Owner<ComponentType>& components,
                            Owner<PaneResizerBar>& resizerBars)
    {
        if (node.getType() == idPane)
        {
            const juce::String nodeID { node.getProperty (id).toString() };

            for (auto& component : components)
            {
                if (component->getComponentID() == nodeID)
                    component->setBounds (bounds);
            }
        }
        else if (node.getType() == idPanes)
        {
            const int numChildren { node.getNumChildren() };

            if (numChildren == 1)
            {
                juce::ValueTree mutableNode { node };
                mutableNode.setProperty (ID::x,      bounds.getX(),      nullptr);
                mutableNode.setProperty (ID::y,      bounds.getY(),      nullptr);
                mutableNode.setProperty (ID::width,  bounds.getWidth(),  nullptr);
                mutableNode.setProperty (ID::height, bounds.getHeight(), nullptr);

                layoutNode (node.getChild (0), bounds, components, resizerBars);
            }
            else if (numChildren == 2)
            {
                juce::ValueTree mutableNode { node };
                mutableNode.setProperty (ID::x,      bounds.getX(),      nullptr);
                mutableNode.setProperty (ID::y,      bounds.getY(),      nullptr);
                mutableNode.setProperty (ID::width,  bounds.getWidth(),  nullptr);
                mutableNode.setProperty (ID::height, bounds.getHeight(), nullptr);

                const juce::String direction { node.getProperty (idDirection).toString() };
                const double ratio { static_cast<double> (node.getProperty (idRatio, 0.5)) };

                if (direction == "vertical")
                {
                    const int totalWidth { bounds.getWidth() };
                    const int firstWidth { juce::roundToInt ((totalWidth - resizerBarSize) * ratio) };
                    const int secondWidth { totalWidth - firstWidth - resizerBarSize };

                    const auto firstBounds  { bounds.withWidth (firstWidth) };
                    const auto resizerBound { bounds.withX (bounds.getX() + firstWidth).withWidth (resizerBarSize) };
                    const auto secondBounds { bounds.withX (bounds.getX() + firstWidth + resizerBarSize).withWidth (secondWidth) };

                    for (auto& bar : resizerBars)
                    {
                        if (bar->getSplitNode() == node)
                            bar->setBounds (resizerBound);
                    }

                    layoutNode (node.getChild (0), firstBounds,  components, resizerBars);
                    layoutNode (node.getChild (1), secondBounds, components, resizerBars);
                }
                else
                {
                    const int totalHeight { bounds.getHeight() };
                    const int firstHeight { juce::roundToInt ((totalHeight - resizerBarSize) * ratio) };
                    const int secondHeight { totalHeight - firstHeight - resizerBarSize };

                    const auto firstBounds  { bounds.withHeight (firstHeight) };
                    const auto resizerBound { bounds.withY (bounds.getY() + firstHeight).withHeight (resizerBarSize) };
                    const auto secondBounds { bounds.withY (bounds.getY() + firstHeight + resizerBarSize).withHeight (secondHeight) };

                    for (auto& bar : resizerBars)
                    {
                        if (bar->getSplitNode() == node)
                            bar->setBounds (resizerBound);
                    }

                    layoutNode (node.getChild (0), firstBounds,  components, resizerBars);
                    layoutNode (node.getChild (1), secondBounds, components, resizerBars);
                }
            }
        }
    }

    //==============================================================================
    juce::ValueTree state;

    static constexpr int resizerBarSize { 4 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PaneManager)

public:
    //==============================================================================
    /** @brief Root node type identifier ("PANES"). */
    static const juce::Identifier idPanes;

    /** @brief Leaf node type identifier ("PANE"). */
    static const juce::Identifier idPane;

    /** @brief Property key for split direction ("vertical" or "horizontal"). */
    static const juce::Identifier idDirection;

    /** @brief Property key for the split ratio in [0.0, 1.0]. */
    static const juce::Identifier idRatio;

    /** @brief Property key for the leaf ID matching componentID. */
    static const juce::Identifier id;
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace jreng
