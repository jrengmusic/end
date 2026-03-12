#include "jreng_pane_manager.h"

namespace jreng
{ /*____________________________________________________________________________*/

const juce::Identifier PaneManager::idPanes     { "PANES" };
const juce::Identifier PaneManager::idPane      { "PANE" };
const juce::Identifier PaneManager::idDirection { "direction" };
const juce::Identifier PaneManager::idRatio     { "ratio" };
const juce::Identifier PaneManager::idUuid      { "uuid" };

//==============================================================================
PaneManager::PaneManager()
    : state (idPanes)
{
}

PaneManager::~PaneManager()
{
}

//==============================================================================
juce::ValueTree& PaneManager::getState() noexcept
{
    return state;
}

//==============================================================================
void PaneManager::addLeaf (const juce::String& uuid)
{
    jassert (state.getNumChildren() <= 1);

    juce::ValueTree leaf { idPane };
    leaf.setProperty (idUuid, uuid, nullptr);
    state.appendChild (leaf, nullptr);
}

//==============================================================================
void PaneManager::split (const juce::String& uuid,
                         const juce::String& newUuid,
                         const juce::String& direction)
{
    juce::ValueTree leaf { findLeaf (state, uuid) };
    jassert (leaf.isValid());

    juce::ValueTree parent { leaf.getParent() };
    jassert (parent.isValid());

    juce::ValueTree newLeaf { idPane };
    newLeaf.setProperty (idUuid, newUuid, nullptr);

    if (parent.getNumChildren() == 1)
    {
        parent.setProperty (idDirection, direction, nullptr);
        parent.setProperty (idRatio, 0.5, nullptr);
        parent.appendChild (newLeaf, nullptr);
    }
    else
    {
        const int leafIndex { parent.indexOf (leaf) };

        juce::ValueTree newPanes { idPanes };
        newPanes.setProperty (idDirection, direction, nullptr);
        newPanes.setProperty (idRatio, 0.5, nullptr);

        parent.removeChild (leaf, nullptr);

        newPanes.appendChild (leaf, nullptr);
        newPanes.appendChild (newLeaf, nullptr);

        parent.addChild (newPanes, leafIndex, nullptr);
    }
}

//==============================================================================
void PaneManager::remove (const juce::String& uuid)
{
    juce::ValueTree leaf { findLeaf (state, uuid) };
    jassert (leaf.isValid());

    juce::ValueTree parent { leaf.getParent() };
    jassert (parent.isValid());

    const int siblingIndex { parent.indexOf (leaf) == 0 ? 1 : 0 };
    juce::ValueTree sibling { parent.getChild (siblingIndex) };

    parent.removeChild (leaf, nullptr);
    parent.removeChild (sibling, nullptr);

    if (parent == state)
    {
        state.removeAllProperties (nullptr);

        if (sibling.getType() == idPane)
        {
            state.appendChild (sibling, nullptr);
        }
        else
        {
            for (int i { sibling.getNumProperties() - 1 }; i >= 0; --i)
            {
                const auto prop { sibling.getPropertyName (i) };
                state.setProperty (prop, sibling.getProperty (prop), nullptr);
            }

            while (sibling.getNumChildren() > 0)
            {
                auto child { sibling.getChild (0) };
                sibling.removeChild (0, nullptr);
                state.appendChild (child, nullptr);
            }
        }
    }
    else
    {
        juce::ValueTree grandparent { parent.getParent() };
        jassert (grandparent.isValid());

        const int parentIndex { grandparent.indexOf (parent) };
        grandparent.removeChild (parent, nullptr);
        grandparent.addChild (sibling, parentIndex, nullptr);
    }
}

//==============================================================================
int PaneManager::getItemCurrentPosition (const juce::ValueTree& splitNode) const
{
    const int x      { static_cast<int> (splitNode.getProperty (ID::x,      0)) };
    const int y      { static_cast<int> (splitNode.getProperty (ID::y,      0)) };
    const int width  { static_cast<int> (splitNode.getProperty (ID::width,  0)) };
    const int height { static_cast<int> (splitNode.getProperty (ID::height, 0)) };
    const double ratio { static_cast<double> (splitNode.getProperty (idRatio, 0.5)) };
    const juce::String direction { splitNode.getProperty (idDirection).toString() };

    if (direction == "vertical")
        return x + juce::roundToInt ((width - resizerBarSize) * ratio);

    return y + juce::roundToInt ((height - resizerBarSize) * ratio);
}

void PaneManager::setItemPosition (const juce::ValueTree& splitNode, int newPosition)
{
    const int x      { static_cast<int> (splitNode.getProperty (ID::x,      0)) };
    const int y      { static_cast<int> (splitNode.getProperty (ID::y,      0)) };
    const int width  { static_cast<int> (splitNode.getProperty (ID::width,  0)) };
    const int height { static_cast<int> (splitNode.getProperty (ID::height, 0)) };
    const juce::String direction { splitNode.getProperty (idDirection).toString() };

    double ratio { 0.5 };

    if (direction == "vertical")
    {
        const int totalSize { width - resizerBarSize };

        if (totalSize > 0)
            ratio = static_cast<double> (newPosition - x) / static_cast<double> (totalSize);
    }
    else
    {
        const int totalSize { height - resizerBarSize };

        if (totalSize > 0)
            ratio = static_cast<double> (newPosition - y) / static_cast<double> (totalSize);
    }

    ratio = juce::jlimit (0.1, 0.9, ratio);

    juce::ValueTree mutableNode { splitNode };
    mutableNode.setProperty (idRatio, ratio, nullptr);
}

//==============================================================================
juce::ValueTree PaneManager::findLeaf (juce::ValueTree node, const juce::String& uuid)
{
    juce::ValueTree result;

    if (node.getType() == idPane and node.getProperty (idUuid).toString() == uuid)
    {
        result = node;
    }
    else
    {
        for (auto child : node)
        {
            result = findLeaf (child, uuid);

            if (result.isValid())
                break;
        }
    }

    return result;
}

/**______________________________END OF NAMESPACE______________________________*/
} // namespace jreng
