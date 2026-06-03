/**
 * @file LinkManager.cpp
 * @brief Implementation of viewport link scanning, hit-testing, and dispatch.
 *
 * @see LinkManager.h
 */

#include "LinkManager.h"
#include "../AppModel.h"

namespace terminal
{
/*____________________________________________________________________________*/

LinkManager::LinkManager (Model& s,
                          std::function<void (const char*, int)> writeToPtyCallback) noexcept
    : state (s)
    , writeToPty (std::move (writeToPtyCallback))
    , promptRowNode         (jam::ValueTree::getChildWithID (state.getRootTree(), id::promptRow.toString()))
    , activeScreenNode      (jam::ValueTree::getChildWithID (state.getRootTree(), id::activeScreen.toString()))
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
    state.storeValue (terminal::id::SESSION, terminal::id::hintPage, 0);
    state.storeValue (terminal::id::SESSION, terminal::id::hintTotalPages, 0);
}

void LinkManager::advanceHintPage() noexcept
{
    const juce::ValueTree root { state.getRootTree() };
    auto hintTotalNode { jam::ValueTree::getChildWithID (root, terminal::id::hintTotalPages.toString()) };
    auto hintPageNode  { jam::ValueTree::getChildWithID (root, terminal::id::hintPage.toString()) };
    const int total    { hintTotalNode.isValid() ? static_cast<int> (hintTotalNode.getProperty (terminal::id::value)) : 0 };

    if (total > 1)
    {
        const int current  { hintPageNode.isValid() ? static_cast<int> (hintPageNode.getProperty (terminal::id::value)) : 0 };
        const int nextPage { (current + 1) % total };
        state.storeValue (terminal::id::SESSION, terminal::id::hintPage, nextPage);
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

    state.storeValue (terminal::id::SESSION, terminal::id::hintTotalPages, static_cast<int> (pageBreaks.size()));
}

void LinkManager::assignCurrentPage() noexcept
{
    const juce::ValueTree assignRoot { state.getRootTree() };
    auto assignPageNode  { jam::ValueTree::getChildWithID (assignRoot, terminal::id::hintPage.toString()) };
    auto assignTotalNode { jam::ValueTree::getChildWithID (assignRoot, terminal::id::hintTotalPages.toString()) };
    const int page  { assignPageNode.isValid()  ? static_cast<int> (assignPageNode.getProperty (terminal::id::value))  : 0 };
    const int total { assignTotalNode.isValid() ? static_cast<int> (assignTotalNode.getProperty (terminal::id::value)) : 0 };

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

        const juce::String handler { AppModel::getContext()->getHandler (ext) };

        if (handler == Map::LinkHandler::getContext()->get (Map::LinkHandler::whelmed))
        {
            // Write path to AppModel WINDOW — Tabs::valueTreePropertyChanged consumes it.
            AppModel::getContext()->getWindow().setProperty (app::id::pendingMarkdownFile, path, nullptr);
        }
        else if (handler == Map::LinkHandler::getContext()->get (Map::LinkHandler::image))
        {
            // Write path to AppModel WINDOW — Tabs::valueTreePropertyChanged consumes it.
            AppModel::getContext()->getWindow().setProperty (app::id::pendingImageFile, path, nullptr);
        }
        else
        {
            const juce::String opener { handler.isNotEmpty() and handler != Map::LinkHandler::getContext()->get (Map::LinkHandler::whelmed)
                                            ? handler
                                            : AppModel::getContext()->getValue<juce::String> (app::id::NEXUS_LUA, app::id::hyperlinkEditor) };
            // Replaces getMode() — reads bracketedPaste from MODES node directly.
            const juce::ValueTree dispatchModes { state.getChildWithName (terminal::id::MODES) };
            const bool bracketed { static_cast<int> (jam::ValueTree::getValueFromChildWithID (dispatchModes, terminal::id::bracketedPaste).getValue()) != 0 };
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

    if (property == id::value)
    {
        const juce::ValueTree root { state.getRootTree() };

        // Replaces hasOutputBlock() — reads outputBlockTop, promptRow, activeScreen directly.
        auto blockTopNode    { jam::ValueTree::getChildWithID (root, terminal::id::outputBlockTop.toString()) };
        auto promptNode      { jam::ValueTree::getChildWithID (root, terminal::id::promptRow.toString()) };
        auto activeScrnNode  { jam::ValueTree::getChildWithID (root, terminal::id::activeScreen.toString()) };
        const int blockTop   { blockTopNode.isValid()   ? static_cast<int> (blockTopNode.getProperty (terminal::id::value))   : -1 };
        const int promptRow  { promptNode.isValid()     ? static_cast<int> (promptNode.getProperty (terminal::id::value))     : -1 };
        const int activeScr  { activeScrnNode.isValid() ? static_cast<int> (activeScrnNode.getProperty (terminal::id::value)) : 0 };
        const bool hasBlock  { blockTop >= 0 and promptRow > blockTop and activeScr == Map::Screen::normal };

        if (hasBlock)
        {
            const juce::String cwd { root.getProperty (terminal::id::cwd).toString() };
            scan (cwd, true);
        }
        else
        {
            clickableLinks.clear();
        }
    }
}

/**______________________________END OF NAMESPACE______________________________*/
} // namespace terminal
