/**
 * @file State.h
 * @brief Whelmed document state — ValueTree SSOT, parsed document, atomic dirty tracking.
 *
 * Whelmed::State is the file-backed state model for markdown panes.
 * Pure ValueTree, message thread only (except commitDocument which is called
 * from parser thread with atomic release fence).
 *
 * Threading model identical to Terminal::State:
 * - Parser thread writes document, then needsFlush.store(true, release)
 * - Message thread needsFlush.exchange(false, acquire), then reads document
 *
 * @see Whelmed::Component
 * @see Whelmed::Parser
 * @see jreng::Markdown::Parser
 */

#pragma once
#include <JuceHeader.h>
#include "../AppIdentifier.h"

namespace Whelmed
{ /*____________________________________________________________________________*/

class State
{
public:
    explicit State (const juce::File& file);

    /**
     * @brief Commits a parsed document from the parser thread.
     * Moves the document and releases needsFlush atomic.
     * @note PARSER THREAD.
     */
    void commitDocument (jreng::Markdown::ParsedDocument&& doc);

    /**
     * @brief Exchanges needsFlush to false with acquire fence.
     * All writes before the parser's release are visible after this returns true.
     * @return true if document was committed since last flush.
     * @note MESSAGE THREAD.
     */
    bool flush() noexcept;

    /**
     * @brief Returns the parsed markdown document.
     * Only valid after flush() returns true.
     * @note MESSAGE THREAD.
     */
    const jreng::Markdown::ParsedDocument& getDocument() const noexcept;

    /**
     * @brief Returns the DOCUMENT ValueTree for grafting into AppState.
     * @note MESSAGE THREAD.
     */
    juce::ValueTree getValueTree() const noexcept;

private:
    juce::ValueTree state;
    juce::File file;
    jreng::Markdown::ParsedDocument document;

    std::atomic<bool> needsFlush { false };  ///< Written by parser thread (release), exchanged by message thread (acquire)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (State)
};

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Whelmed
