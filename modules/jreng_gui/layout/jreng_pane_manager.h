#pragma once

#include "jreng_pane_resizer_bar.h"

namespace jreng
{ /*____________________________________________________________________________*/

//==============================================================================
/**
    Owns a binary-tree ValueTree representing the split-pane layout.

    Leaves are PANE nodes (uuid property). Internal nodes are PANES nodes
    (direction + ratio properties, exactly 2 children).

    The static layOut function recursively subdivides a rectangle and calls
    setBounds on components matched by getComponentID().

    @tags{GUI}
*/
class PaneManager
{
public:
    //==============================================================================
    PaneManager();
    ~PaneManager();

    //==============================================================================
    juce::ValueTree& getState() noexcept;

    void addLeaf (const juce::String& uuid);

    void split (const juce::String& uuid,
                const juce::String& newUuid,
                const juce::String& direction);

    void remove (const juce::String& uuid);

    //==============================================================================
    int getItemCurrentPosition (const juce::ValueTree& splitNode) const;
    void setItemPosition (const juce::ValueTree& splitNode, int newPosition);

    //==============================================================================
    template <typename ComponentType>
    static void layOut (const juce::ValueTree& state,
                        juce::Rectangle<int> bounds,
                        Owner<ComponentType>& components,
                        Owner<PaneResizerBar>& resizerBars)
    {
        layOutNode (state, bounds, components, resizerBars);
    }

private:
    //==============================================================================
    template <typename ComponentType>
    static void layOutNode (const juce::ValueTree& node,
                            juce::Rectangle<int> bounds,
                            Owner<ComponentType>& components,
                            Owner<PaneResizerBar>& resizerBars)
    {
        if (node.getType() == idPane)
        {
            const juce::String nodeUuid { node.getProperty (idUuid).toString() };

            for (auto& component : components)
            {
                if (component->getComponentID() == nodeUuid)
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

                layOutNode (node.getChild (0), bounds, components, resizerBars);
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

                    layOutNode (node.getChild (0), firstBounds,  components, resizerBars);
                    layOutNode (node.getChild (1), secondBounds, components, resizerBars);
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

                    layOutNode (node.getChild (0), firstBounds,  components, resizerBars);
                    layOutNode (node.getChild (1), secondBounds, components, resizerBars);
                }
            }
        }
    }

    //==============================================================================
    static juce::ValueTree findLeaf (juce::ValueTree node, const juce::String& uuid);

    //==============================================================================
    juce::ValueTree state;

    static constexpr int resizerBarSize { 4 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PaneManager)

public:
    //==============================================================================
    static const juce::Identifier idPanes;
    static const juce::Identifier idPane;
    static const juce::Identifier idDirection;
    static const juce::Identifier idRatio;
    static const juce::Identifier idUuid;
};

/**______________________________END OF NAMESPACE______________________________*/
} // namespace jreng
