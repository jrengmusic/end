/**
 * @file LinkManager.cpp
 * @brief Implementation of viewport link scanning, hit-testing, and dispatch.
 *
 * @see LinkManager.h
 */

#include "LinkManager.h"
#include <JuceHeader.h>
#include "../data/State.h"
#include "../data/Identifier.h"
#include "../logic/Grid.h"
#include "../../config/Config.h"
#include <unordered_set>

namespace Terminal
{ /*____________________________________________________________________________*/

LinkManager::LinkManager (State& s, const Grid& g,
                          std::function<void (const char*, int)> writeToPtyCallback) noexcept
    : state (s)
    , grid (g)
    , writeToPty (std::move (writeToPtyCallback))
    , promptRowNode         (jreng::ValueTree::getChildWithID (state.get(), ID::promptRow.toString()))
    , activeScreenNode      (jreng::ValueTree::getChildWithID (state.get(), ID::activeScreen.toString()))
    , hyperlinksNode        (state.get().getChildWithName (ID::HYPERLINKS))
    , scrollOffsetNode      (jreng::ValueTree::getChildWithID (state.get(), ID::scrollOffset.toString()))
    , outputBlockBottomNode (jreng::ValueTree::getChildWithID (state.get(), ID::outputBlockBottom.toString()))
{
    promptRowNode.addListener (this);
    activeScreenNode.addListener (this);
    hyperlinksNode.addListener (this);
    scrollOffsetNode.addListener (this);
    outputBlockBottomNode.addListener (this);
}

LinkManager::~LinkManager()
{
    promptRowNode.removeListener (this);
    activeScreenNode.removeListener (this);
    hyperlinksNode.removeListener (this);
    scrollOffsetNode.removeListener (this);
    outputBlockBottomNode.removeListener (this);
}

void LinkManager::scan (const juce::String& cwd, bool outputRowsOnly)
{
    clickableLinks = scanViewport (cwd, outputRowsOnly);
}

void LinkManager::scanForHints (const juce::String& cwd)
{
    hintLinks = scanViewport (cwd, state.hasOutputBlock());
    assignHintLabels (hintLinks);
}

void LinkManager::clearHints() noexcept { hintLinks.clear(); }

const LinkSpan* LinkManager::hitTest (int row, int col) const noexcept
{
    const LinkSpan* result { nullptr };

    for (const auto& span : clickableLinks)
    {
        if (row == span.row and col >= span.col and col < span.col + span.length)
        {
            result = &span;
            break;
        }
    }

    return result;
}

const LinkSpan* LinkManager::hitTestHint (char label) const noexcept
{
    const LinkSpan* result { nullptr };

    for (const auto& span : hintLinks)
    {
        if (span.hintLabel[0] == label)
        {
            result = &span;
            break;
        }
    }

    return result;
}

void LinkManager::dispatch (const LinkSpan& span) const
{
    if (span.type == LinkDetector::LinkType::url)
    {
        juce::URL { span.uri }.launchInDefaultBrowser();
    }
    else
    {
        const juce::String path { span.uri.fromFirstOccurrenceOf ("file://", false, false) };
        const juce::String ext { juce::File (path).getFileExtension().toLowerCase() };

        const juce::String handler { Config::getContext()->getHandler (ext) };

        if (handler == "whelmed" and onOpenMarkdown != nullptr)
        {
            onOpenMarkdown (juce::File (path));
        }
        else
        {
            const juce::String opener { handler.isNotEmpty() and handler != "whelmed"
                                            ? handler
                                            : Config::getContext()->getString (Config::Key::hyperlinksEditor) };
            const juce::String command { opener + " " + path + "\r" };
            writeToPty (command.toRawUTF8(), static_cast<int> (command.getNumBytesAsUTF8()));
        }
    }
}

const std::vector<LinkSpan>& LinkManager::getClickableLinks() const noexcept { return clickableLinks; }

const std::vector<LinkSpan>& LinkManager::getHintLinks() const noexcept { return hintLinks; }

std::vector<LinkSpan> LinkManager::scanViewport (const juce::String& cwd, bool outputRowsOnly) const
{
    std::vector<LinkSpan> spans;

    const int visibleRows { grid.getVisibleRows() };
    const int cols { grid.getCols() };
    const int scrollbackUsed { grid.getScrollbackUsed() };
    const int scrollOffset { state.getScrollOffset() };
    const int visibleBase { scrollbackUsed - scrollOffset };
    const bool hasBlock { state.hasOutputBlock() };
    const int blockTop { outputRowsOnly ? state.getOutputBlockTop() : visibleBase };
    const int blockBottom { outputRowsOnly ? state.getOutputBlockBottom() : visibleBase + visibleRows - 1 };
    const bool normalScreen { state.getActiveScreen() == ActiveScreen::normal };

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

    // Merge OSC 8 explicit hyperlink spans from the State HYPERLINKS ValueTree.
    for (int i { 0 }; i < hyperlinksNode.getNumChildren(); ++i)
    {
        const auto child { hyperlinksNode.getChild (i) };
        const juce::String uri { child.getProperty (ID::uri).toString() };

        if (uri.isNotEmpty())
        {
            const int oscRow      { static_cast<int> (child.getProperty (ID::row)) };
            const int oscStartCol { static_cast<int> (child.getProperty (ID::startCol)) };
            const int oscEndCol   { static_cast<int> (child.getProperty (ID::endCol)) };

            const bool oscInViewport { oscRow >= visibleBase and oscRow < visibleBase + visibleRows };
            const bool isUrl { uri.startsWith ("http://") or uri.startsWith ("https://") };
            const bool oscInOutputBlock { hasBlock and oscRow >= blockTop and oscRow <= blockBottom };
            const bool oscFileAllowed { not isUrl and normalScreen and oscInOutputBlock };

            if (oscInViewport and (isUrl or oscFileAllowed))
            {
                LinkSpan span;
                span.row    = oscRow - visibleBase;
                span.col    = oscStartCol;
                span.length = oscEndCol - oscStartCol;
                span.uri    = uri;

                if (isUrl)
                {
                    span.type = LinkDetector::LinkType::url;
                }
                else
                {
                    span.type = LinkDetector::LinkType::file;

                    if (not uri.startsWith ("file://"))
                        span.uri = "file://" + uri;
                }

                spans.push_back (std::move (span));
            }
        }
    }

    return spans;
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

        // hintLabel[0] is zero-initialized by default (LinkSpan::hintLabel[2] {}).
        // If the inner loop found no usable character, hintLabel[0] remains 0.
        if (span.hintLabel[0] == 0)
        {
            span.labelCol = span.col;
        }
    }
}

// =============================================================================
// ValueTree::Listener
// =============================================================================

void LinkManager::valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property)
{
    juce::ignoreUnused (tree);

    if (property == ID::value)
    {
        if (state.hasOutputBlock())
        {
            const juce::String cwd { state.get().getProperty (ID::cwd).toString() };
            scan (cwd, true);
        }
        else
        {
            clickableLinks.clear();
        }
    }
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Terminal
