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
#include "../logic/Grid.h"
#include "LinkDetector.h"
#include <unordered_set>

namespace Terminal
{ /*____________________________________________________________________________*/

void LinkManager::scanHeuristicTokens (std::vector<LinkSpan>& spans, const juce::String& cwd,
                                       int visibleRows, int cols, int scrollOffset,
                                       int visibleBase, bool hasBlock, int blockTop, int blockBottom,
                                       bool normalScreen) const
{
    for (int row { 0 }; row < visibleRows; ++row)
    {
        const Cell* rowCells { scrollOffset > 0
                                   ? grid.scrollbackRow (row, scrollOffset)
                                   : grid.activeVisibleRow (row) };

        if (rowCells != nullptr)
        {
            int col { 0 };

            while (col < cols)
            {
                // Skip whitespace / empty cells.
                while (col < cols and (rowCells[col].codepoint == 0 or rowCells[col].codepoint <= 0x20))
                {
                    ++col;
                }

                // Accumulate non-whitespace token.
                if (col < cols)
                {
                    const int tokenStartCol { col };
                    juce::String token;

                    while (col < cols and rowCells[col].codepoint > 0x20)
                    {
                        token += juce::String::charToString (static_cast<juce::juce_wchar> (rowCells[col].codepoint));
                        ++col;
                    }

                    const int tokenLength { col - tokenStartCol };
                    const LinkDetector::LinkType linkType { LinkDetector::classify (token) };

                    const int absoluteRow { visibleBase + row };
                    const bool inOutputBlock { hasBlock and absoluteRow >= blockTop and absoluteRow <= blockBottom };
                    const bool fileAllowed { linkType == LinkDetector::LinkType::file and normalScreen
                                             and inOutputBlock };
                    const bool urlAllowed { linkType == LinkDetector::LinkType::url };

                    if (fileAllowed or urlAllowed)
                    {
                        LinkSpan span;
                        span.row = row;
                        span.col = tokenStartCol;
                        span.length = tokenLength;
                        span.type = linkType;

                        if (linkType == LinkDetector::LinkType::url)
                        {
                            span.uri = token;
                        }
                        else
                        {
                            juce::File resolved;

                            if (juce::File::isAbsolutePath (token))
                            {
                                resolved = juce::File { token };
                            }
                            else
                            {
                                resolved = juce::File { cwd }.getChildFile (token);
                            }

                            span.uri = resolved.getFullPathName().isNotEmpty() ? "file://" + resolved.getFullPathName()
                                                                               : "file://" + token;
                        }

                        spans.push_back (std::move (span));
                    }
                }
            }
        }
    }
}

void LinkManager::scanCellNativeLinks (std::vector<LinkSpan>& spans,
                                       int visibleRows, int cols, int scrollOffset,
                                       int visibleBase, bool hasBlock, int blockTop, int blockBottom,
                                       bool normalScreen) const
{
    for (int row { 0 }; row < visibleRows; ++row)
    {
        const Cell* rowCells { scrollOffset > 0
                                   ? grid.scrollbackRow (row, scrollOffset)
                                   : grid.activeVisibleRow (row) };
        const uint16_t* linkRow { scrollOffset > 0
                                      ? grid.scrollbackLinkIdRow (row, scrollOffset)
                                      : grid.activeVisibleLinkIdRow (row) };

        if (rowCells != nullptr and linkRow != nullptr)
        {
            int col { 0 };

            while (col < cols)
            {
                if (rowCells[col].hasHyperlink())
                {
                    const uint16_t id { linkRow[col] };
                    const int spanStart { col };

                    while (col < cols
                           and rowCells[col].hasHyperlink()
                           and linkRow[col] == id)
                    {
                        ++col;
                    }

                    const juce::String uri { state.getLinkUri (id) };

                    if (uri.isNotEmpty())
                    {
                        const int absoluteRow { visibleBase + row };
                        const bool isUrl { uri.startsWith ("http://")
                                           or uri.startsWith ("https://") };
                        const bool oscInOutputBlock { hasBlock and absoluteRow >= blockTop and absoluteRow <= blockBottom };
                        const bool oscFileAllowed { not isUrl and normalScreen and oscInOutputBlock };

                        if (isUrl or oscFileAllowed)
                        {
                            LinkSpan span;
                            span.row    = row;
                            span.col    = spanStart;
                            span.length = col - spanStart;
                            span.uri    = uri;

                            if (isUrl)
                            {
                                span.type = LinkDetector::LinkType::url;
                            }
                            else
                            {
                                span.type = LinkDetector::LinkType::file;

                                if (not span.uri.startsWith ("file://"))
                                    span.uri = "file://" + span.uri;
                            }

                            spans.push_back (std::move (span));
                        }
                    }
                }
                else
                {
                    ++col;
                }
            }
        }
    }
}

void LinkManager::assignHintLabels (std::vector<LinkSpan>& spans) noexcept
{
    std::unordered_set<char> usedLabels;
    const int scrollOffset { state.getScrollOffset() };

    for (auto& span : spans)
    {
        const int tokenEnd { span.col + span.length };
        const Cell* rowCells { scrollOffset > 0
                                   ? grid.scrollbackRow (span.row, scrollOffset)
                                   : grid.activeVisibleRow (span.row) };

        for (int c { span.col }; c < tokenEnd; ++c)
        {
            if (rowCells != nullptr)
            {
                const uint32_t cp { rowCells[c].codepoint };
                const char lower { static_cast<char> (cp >= 'A' and cp <= 'Z' ? cp + 32 : cp) };

                if (lower >= 'a' and lower <= 'z' and usedLabels.find (lower) == usedLabels.end())
                {
                    span.hintLabel[0] = lower;
                    span.hintLabel[1] = 0;
                    span.labelCol = c;
                    usedLabels.insert (lower);
                    break;
                }
            }
        }

        // If the inner loop found no usable character, hintLabel[0] remains 0.
        // The span is unreachable on this page — user cycles to another page.
    }
}

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Terminal
