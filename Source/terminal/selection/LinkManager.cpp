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
#include "../../lua/Engine.h"
#include <unordered_set>

namespace Terminal
{ /*____________________________________________________________________________*/

LinkManager::LinkManager (State& s,
                          std::function<void (const char*, int)> writeToPtyCallback) noexcept
    : state (s)
    , writeToPty (std::move (writeToPtyCallback))
    , promptRowNode         (jam::ValueTree::getChildWithID (state.get(), ID::promptRow.toString()))
    , activeScreenNode      (jam::ValueTree::getChildWithID (state.get(), ID::activeScreen.toString()))
{
    promptRowNode.addListener (this);
    activeScreenNode.addListener (this);
}

LinkManager::~LinkManager()
{
    promptRowNode.removeListener (this);
    activeScreenNode.removeListener (this);
}

void LinkManager::scan (const juce::String& cwd, bool outputRowsOnly)
{
    // TODO Step 7: migrate to Screen
    juce::ignoreUnused (cwd, outputRowsOnly);
}

void LinkManager::scanForHints (const juce::String& cwd)
{
    // TODO Step 7: migrate to Screen
    juce::ignoreUnused (cwd);
}

void LinkManager::clearHints() noexcept
{
    hintLinks.clear();
    pageBreaks.clear();
    activeStart = 0;
    activeCount = 0;
    state.setHintPage (0);
    state.setHintTotalPages (0);
}

void LinkManager::advanceHintPage() noexcept
{
    const int total { state.getHintTotalPages() };

    if (total > 1)
    {
        const int nextPage { (state.getHintPage() + 1) % total };
        state.setHintPage (nextPage);
        assignCurrentPage();
    }
}

void LinkManager::buildPages() noexcept
{
    pageBreaks.clear();
    pageBreaks.push_back (0);

    // Label all spans upfront and record page boundaries.
    assignHintLabels (hintLinks);

    std::unordered_set<char> usedLabels;

    // TODO Step 7: migrate to Screen — page building reads grid data
    for (size_t i { 0 }; i < hintLinks.size(); ++i)
    {
        const char label { hintLinks.at (i).hintLabel[0] };

        if (label == 0 or usedLabels.find (label) != usedLabels.end())
        {
            usedLabels.clear();
            pageBreaks.push_back (static_cast<int> (i));
            usedLabels.insert (hintLinks.at (i).hintLabel[0]);
        }
        else
        {
            usedLabels.insert (label);
        }
    }

    state.setHintTotalPages (static_cast<int> (pageBreaks.size()));
}

void LinkManager::assignCurrentPage() noexcept
{
    const int page { state.getHintPage() };
    const int total { state.getHintTotalPages() };

    activeStart = pageBreaks.at (static_cast<size_t> (page));
    activeCount = (page + 1 < total)
                      ? pageBreaks.at (static_cast<size_t> (page + 1)) - activeStart
                      : static_cast<int> (hintLinks.size()) - activeStart;
}

const LinkSpan* LinkManager::hitTest (cell row, cell col) const noexcept
{
    const LinkSpan* result { nullptr };

    for (const auto& span : clickableLinks)
    {
        if (row.value == span.row.value and col.value >= span.col.value and col.value < span.col.value + span.length)
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

    for (int i { activeStart }; i < activeStart + activeCount; ++i)
    {
        if (hintLinks.at (static_cast<size_t> (i)).hintLabel[0] == label)
        {
            result = &hintLinks.at (static_cast<size_t> (i));
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

        const auto* cfg { lua::Engine::getContext() };
        const juce::String handler { cfg->getHandler (ext) };

        if (handler == "whelmed" and onOpenMarkdown != nullptr)
        {
            onOpenMarkdown (juce::File (path));
        }
        else if (handler == "image" and onOpenImage != nullptr)
        {
            onOpenImage (juce::File (path), span.row);
        }
        else
        {
            const juce::String opener { handler.isNotEmpty() and handler != "whelmed"
                                            ? handler
                                            : cfg->nexus.hyperlinks.editor };
            const bool bracketed { state.getMode (ID::bracketedPaste) };
            const juce::String payload { opener + " \"" + path + "\"" };

            juce::String command;

            if (bracketed)
            {
                static constexpr const char open[]  { "\x1b[200~" };
                static constexpr const char close[] { "\x1b[201~" };
                command = juce::String (open) + payload + juce::String (close) + "\r";
            }
            else
            {
                command = payload + "\r";
            }

            writeToPty (command.toRawUTF8(), static_cast<int> (command.getNumBytesAsUTF8()));
        }
    }
}

const std::vector<LinkSpan>& LinkManager::getClickableLinks() const noexcept { return clickableLinks; }

const std::vector<LinkSpan>& LinkManager::getHintLinks() const noexcept { return hintLinks; }

const LinkSpan* LinkManager::getActiveHintsData() const noexcept { return hintLinks.data() + activeStart; }
int LinkManager::getActiveHintsCount() const noexcept { return activeCount; }

std::vector<LinkSpan> LinkManager::scanViewport (const juce::String& cwd, bool outputRowsOnly) const
{
    // TODO Step 7: migrate to Screen
    juce::ignoreUnused (cwd, outputRowsOnly);
    return {};
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
            const juce::String cwd { state.getCwd() };
            scan (cwd, true);
        }
        else
        {
            clickableLinks.clear();
        }
    }
}

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Terminal
