/**
 * @file LinkManager.cpp
 * @brief Implementation of viewport link scanning, hit-testing, and dispatch.
 *
 * @see LinkManager.h
 */

#include "LinkManager.h"
#include "../logic/Session.h"
#include "../logic/Grid.h"
#include "../data/State.h"
#include "../data/Identifier.h"
#include "../../config/Config.h"
#include <unordered_set>

namespace Terminal
{ /*____________________________________________________________________________*/

LinkManager::LinkManager (Session& s) noexcept
    : session (s)
{
}

void LinkManager::scan (const juce::String& cwd, bool outputRowsOnly)
{
    session.getParser().clearOsc8Links();

    try
    {
        clickableLinks = scanViewport (session.getGrid(), cwd, outputRowsOnly);
    }
    catch (...)
    {
        clickableLinks.clear();
    }

    scanNeeded = false;
}

void LinkManager::scanForHints (const juce::String& cwd)
{
    session.getParser().clearOsc8Links();

    try
    {
        hintLinks = scanViewport (session.getGrid(), cwd, session.getState().hasOutputBlock());
        assignHintLabels (hintLinks, session.getGrid());
    }
    catch (...)
    {
        hintLinks.clear();
    }
}

void LinkManager::clearHints() noexcept
{
    hintLinks.clear();
}

void LinkManager::invalidate() noexcept
{
    scanNeeded = true;
}

bool LinkManager::needsScan() const noexcept
{
    return scanNeeded;
}

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
        const juce::String opener { handler.isNotEmpty() ? handler : Config::getContext()->getString (Config::Key::hyperlinksEditor) };
        const juce::String command { opener + " " + path + "\r" };
        session.writeToPty (command.toRawUTF8(), static_cast<int> (command.getNumBytesAsUTF8()));
    }
}

const std::vector<LinkSpan>& LinkManager::getClickableLinks() const noexcept
{
    return clickableLinks;
}

const std::vector<LinkSpan>& LinkManager::getHintLinks() const noexcept
{
    return hintLinks;
}

std::vector<LinkSpan> LinkManager::scanViewport (const Grid& grid,
                                                  const juce::String& cwd,
                                                  bool outputRowsOnly) const
{
    std::vector<LinkSpan> spans;

    const int visibleRows { grid.getVisibleRows() };
    const int cols { grid.getCols() };
    const bool hasBlock { session.getState().hasOutputBlock() };
    const int blockTop { outputRowsOnly ? session.getState().getOutputBlockTop() : 0 };
    const int blockBottom { outputRowsOnly ? session.getState().getOutputBlockBottom() : visibleRows - 1 };
    const bool normalScreen { session.getState().getActiveScreen() == ActiveScreen::normal };

    for (int row = 0; row < visibleRows; ++row)
    {

        const Cell* rowCells { grid.activeVisibleRow (row) };

        if (rowCells == nullptr)
            continue;

        int col { 0 };

        while (col < cols)
        {
            // Skip whitespace / empty cells.
            while (col < cols
                   and (rowCells[col].codepoint == 0
                        or rowCells[col].codepoint <= 0x20))
            {
                ++col;
            }

            if (col >= cols)
                break;

            // Accumulate non-whitespace token.
            const int tokenStartCol { col };
            juce::String token;

            while (col < cols
                   and rowCells[col].codepoint > 0x20)
            {
                token += juce::String::charToString (
                    static_cast<juce::juce_wchar> (rowCells[col].codepoint));
                ++col;
            }

            const int tokenLength { col - tokenStartCol };
            const LinkDetector::LinkType linkType { LinkDetector::classify (token) };

            const bool inOutputBlock { hasBlock and row >= blockTop and row <= blockBottom };
            const bool fileAllowed { linkType == LinkDetector::LinkType::file
                                     and normalScreen and inOutputBlock };
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

                    span.uri = resolved.getFullPathName().isNotEmpty()
                                   ? "file://" + resolved.getFullPathName()
                                   : "file://" + token;
                }

                spans.push_back (std::move (span));
            }
        }
    }

    // Merge OSC 8 explicit hyperlink spans accumulated by the parser.
    for (const auto& osc : session.getParser().getOsc8Links())
    {
        const juce::String uri { osc.uri };

        if (uri.isEmpty())
            continue;

        const bool isUrl { uri.startsWith ("http://") or uri.startsWith ("https://") };
        const bool oscInOutputBlock { hasBlock and osc.row >= blockTop and osc.row <= blockBottom };
        const bool oscFileAllowed { not isUrl and normalScreen and oscInOutputBlock };

        if (isUrl or oscFileAllowed)
        {
            LinkSpan span;
            span.row = osc.row;
            span.col = osc.startCol;
            span.length = osc.endCol - osc.startCol;
            span.uri = uri;

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

    return spans;
}

/*static*/ void LinkManager::assignHintLabels (std::vector<LinkSpan>& spans, const Grid& grid) noexcept
{
    std::unordered_set<char> usedLabels;

    for (auto& span : spans)
    {
        const int tokenEnd { span.col + span.length };
        bool assigned { false };

        for (int c { span.col }; c < tokenEnd and not assigned; ++c)
        {
            const Cell* rowCells { grid.activeVisibleRow (span.row) };

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
                    assigned = true;
                }
            }
        }

        if (not assigned)
        {
            span.hintLabel[0] = 0;
            span.labelCol = span.col;
        }
    }
}

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Terminal
