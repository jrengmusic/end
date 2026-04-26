/**
 * @file TabsActions.cpp
 * @brief Terminal::Tabs implementation — config, zoom, split, pane focus, and state restore.
 *
 * @see Tabs.h
 */

#include "Tabs.h"
#include "../AppState.h"
#include "../terminal/data/Identifier.h"
#include "../nexus/Nexus.h"

namespace Terminal
{ /*____________________________________________________________________________*/
void Tabs::applyConfig()
{
    for (auto& p : panes)
    {
        for (auto& pane : p->getPanes())
        {
            pane->applyConfig();
        }
    }
}

void Tabs::switchRenderer (App::RendererType type)
{
    for (auto& p : panes)
    {
        for (auto& pane : p->getPanes())
        {
            pane->switchRenderer (type);
        }
    }
}

void Tabs::increaseZoom()
{
    const float current { AppState::getContext()->getWindowZoom() };
    const float newZoom { juce::jlimit (lua::Engine::zoomMin, lua::Engine::zoomMax, current + lua::Engine::zoomStep) };
    AppState::getContext()->setWindowZoom (newZoom);

    for (auto& p : panes)
    {
        for (auto& pane : p->getPanes())
        {
            pane->applyZoom (newZoom);
        }
    }
}

void Tabs::decreaseZoom()
{
    const float current { AppState::getContext()->getWindowZoom() };
    const float newZoom { juce::jlimit (lua::Engine::zoomMin, lua::Engine::zoomMax, current - lua::Engine::zoomStep) };
    AppState::getContext()->setWindowZoom (newZoom);

    for (auto& p : panes)
    {
        for (auto& pane : p->getPanes())
        {
            pane->applyZoom (newZoom);
        }
    }
}

void Tabs::resetZoom()
{
    const float defaultZoom { lua::Engine::zoomMin };
    AppState::getContext()->setWindowZoom (defaultZoom);

    for (auto& p : panes)
    {
        for (auto& pane : p->getPanes())
        {
            pane->applyZoom (defaultZoom);
        }
    }
}

void Tabs::splitHorizontal()
{
    if (auto* active { getActivePanes() }; active != nullptr)
    {
        active->splitHorizontal();
        focusLastTerminal (active);

        if (AppState::getContext()->isDaemonMode())
            AppState::getContext()->save();
    }
}

void Tabs::splitVertical()
{
    if (auto* active { getActivePanes() }; active != nullptr)
    {
        active->splitVertical();
        focusLastTerminal (active);

        if (AppState::getContext()->isDaemonMode())
            AppState::getContext()->save();
    }
}

void Tabs::splitActiveWithRatio (const juce::String& direction, bool isVertical, double ratio)
{
    if (auto* active { getActivePanes() }; active != nullptr)
    {
        active->splitActiveWithRatio (direction, isVertical, ratio);
        focusLastTerminal (active);

        if (AppState::getContext()->isDaemonMode())
            AppState::getContext()->save();
    }
}

void Tabs::focusPaneLeft()
{
    if (auto* active { getActivePanes() }; active != nullptr)
        active->focusPane (-1, 0);
}

void Tabs::focusPaneDown()
{
    if (auto* active { getActivePanes() }; active != nullptr)
        active->focusPane (0, 1);
}

void Tabs::focusPaneUp()
{
    if (auto* active { getActivePanes() }; active != nullptr)
        active->focusPane (0, -1);
}

void Tabs::focusPaneRight()
{
    if (auto* active { getActivePanes() }; active != nullptr)
        active->focusPane (1, 0);
}

/**
 * @brief Walks saved TAB nodes, creates tabs, and replays splits.
 *
 * Caller must pass a deep copy of the saved TABS tree detached from the live
 * AppState tree — this method walks it directly without aliasing live state.
 *
 * For each TAB child: locates the first PANE leaf (DFS, left-first) to obtain
 * uuid and cwd for addNewTab, then recursively descends the PANES subtree
 * pre-order, deriving child rects via Panes::splitRect and emitting one
 * splitAt call per internal PANES node with 2 children. Overwrites the default
 * ratio on each new split node with the saved value.
 *
 * @param savedTabs   Deep copy of the TABS node — not aliased with live AppState.
 * @param contentRect Chrome-subtracted pixel rect for dim computation.
 * @note MESSAGE THREAD.
 */
void Tabs::restore (juce::ValueTree savedTabs, juce::Rectangle<int> contentRect)
{
    // Recursive leaf finder — returns {uuid, cwd} of the first PANE with a SESSION.
    std::function<std::pair<juce::String, juce::String> (const juce::ValueTree&)> findFirstLeaf;
    findFirstLeaf = [&] (const juce::ValueTree& node) -> std::pair<juce::String, juce::String>
    {
        std::pair<juce::String, juce::String> result;

        if (node.getType() == App::ID::PANE)
        {
            const auto sessionNode { node.getChildWithName (Terminal::ID::SESSION) };

            if (sessionNode.isValid())
            {
                result.first = sessionNode.getProperty (jam::ID::id).toString();
                result.second = sessionNode.getProperty (Terminal::ID::cwd).toString();
            }
        }
        else
        {
            for (int i { 0 }; i < node.getNumChildren() and result.first.isEmpty(); ++i)
                result = findFirstLeaf (node.getChild (i));
        }

        return result;
    };

    // Recursive PANES descent — emits splitAt calls pre-order and descends rects.
    // parentRect is the pixel rect assigned to this node (chrome already subtracted).
    std::function<void (const juce::ValueTree&, juce::Rectangle<int>, Panes*)> walkPanes;
    walkPanes = [&] (const juce::ValueTree& node, juce::Rectangle<int> parentRect, Panes* activePanes)
    {
        if (node.getType() == App::ID::PANES and node.getNumChildren() == 2)
        {
            const juce::String direction { node.getProperty (jam::PaneManager::idDirection).toString() };
            const double savedRatio { static_cast<double> (node.getProperty (jam::PaneManager::idRatio, 0.5)) };

            const auto [targetRect, newRect] { Panes::splitRect (parentRect, direction, savedRatio) };

            const auto [targetUuid, targetCwd] { findFirstLeaf (node.getChild (0)) };
            const auto [newUuid, newCwd] { findFirstLeaf (node.getChild (1)) };

            if (targetUuid.isNotEmpty() and newUuid.isNotEmpty())
            {
                const auto [newCols, newRows] { Panes::cellsFromRect (newRect, font) };
                const bool isVertical { direction == "vertical" };
                activePanes->splitAt (targetUuid, newUuid, newCwd, direction, isVertical, newCols, newRows);

                auto newLeafNode { jam::PaneManager::findLeaf (activePanes->getState(), newUuid) };
                jassert (newLeafNode.isValid());

                auto splitNode { newLeafNode.getParent() };
                jassert (splitNode.isValid());

                splitNode.setProperty (jam::PaneManager::idRatio, savedRatio, nullptr);
            }

            walkPanes (node.getChild (0), targetRect, activePanes);
            walkPanes (node.getChild (1), newRect, activePanes);
        }
        else
        {
            // Single-child PANES root (before first split) or PANE leaf — pass rect through.
            for (int i { 0 }; i < node.getNumChildren(); ++i)
                walkPanes (node.getChild (i), parentRect, activePanes);
        }
    };

    for (int t { 0 }; t < savedTabs.getNumChildren(); ++t)
    {
        const auto tabNode { savedTabs.getChild (t) };

        if (tabNode.getType() == App::ID::TAB)
        {
            const auto panesNode { tabNode.getChildWithName (App::ID::PANES) };

            if (panesNode.isValid())
            {
                const auto [firstUuid, firstCwd] { findFirstLeaf (panesNode) };

                if (firstUuid.isNotEmpty())
                {
                    // Compute the first leaf's actual sub-rect by descending the
                    // left branch of the saved split tree.  Each PANES node with
                    // 2 children narrows the rect via splitRect.
                    auto firstLeafRect { contentRect };
                    auto walkNode { panesNode };

                    while (walkNode.getType() == App::ID::PANES and walkNode.getNumChildren() == 2)
                    {
                        const juce::String dir { walkNode.getProperty (jam::PaneManager::idDirection).toString() };
                        const double ratio { static_cast<double> (
                            walkNode.getProperty (jam::PaneManager::idRatio, 0.5)) };
                        const auto [targetRect, newRect] { Panes::splitRect (firstLeafRect, dir, ratio) };
                        firstLeafRect = targetRect;
                        walkNode = walkNode.getChild (0);
                    }

                    const auto [firstCols, firstRows] { Panes::cellsFromRect (firstLeafRect, font) };
                    addNewTab (firstCwd, firstUuid, firstCols, firstRows);

                    auto* activePanes { getActivePanes() };
                    jassert (activePanes != nullptr);

                    walkPanes (panesNode, contentRect, activePanes);
                }
            }
        }
    }
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Terminal
