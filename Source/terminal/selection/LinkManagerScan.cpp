/**
 * @file LinkManagerScan.cpp
 * @brief Viewport scan passes: heuristic token scan, cell-native OSC 8 scan,
 *        and hint label assignment for LinkManager.
 *
 * Implements the three scan methods split from the original monolithic
 * `scanViewport` for Lean compliance, plus the `assignHintLabels` helper
 * that operates on an already-collected span vector.
 *
 * @see LinkManager.h
 */

#include "LinkManager.h"
#include <JuceHeader.h>
#include "../data/State.h"
#include "../data/Identifier.h"
#include "LinkDetector.h"
#include <unordered_set>

namespace Terminal
{ /*____________________________________________________________________________*/

void LinkManager::scanHeuristicTokens (std::vector<LinkSpan>& spans, const juce::String& cwd,
                                       int visibleRows, int cols,
                                       int visibleBase, bool hasBlock, int blockTop, int blockBottom,
                                       bool normalScreen) const
{
    // TODO Step 7: migrate to Screen
    juce::ignoreUnused (spans, cwd, visibleRows, cols, visibleBase, hasBlock, blockTop, blockBottom, normalScreen);
}

void LinkManager::scanCellNativeLinks (std::vector<LinkSpan>& spans,
                                       int visibleRows, int cols,
                                       int visibleBase, bool hasBlock, int blockTop, int blockBottom,
                                       bool normalScreen) const
{
    // TODO Step 7: migrate to Screen
    juce::ignoreUnused (spans, visibleRows, cols, visibleBase, hasBlock, blockTop, blockBottom, normalScreen);
}

void LinkManager::assignHintLabels (std::vector<LinkSpan>& spans) noexcept
{
    // TODO Step 7: migrate to Screen
    juce::ignoreUnused (spans);
}

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Terminal
