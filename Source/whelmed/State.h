/**
 * @file State.h
 * @brief Whelmed document state — ValueTree SSOT, parsed document, atomic block counter.
 *
 * Whelmed::State is the file-backed state model for markdown panes.
 * Pure ValueTree, message thread only (except parser-thread methods).
 *
 * Threading model:
 * - Message thread calls setDocument() (synchronous parse result), then parser->start()
 * - Parser thread calls getDocumentForWriting() to resolve styles, appendBlock() per block, setParseComplete()
 * - Message thread calls flush() at 60 Hz — reads atomics, updates ValueTree properties one block per tick
 * - Component listens to ValueTree property changes and applies styling to pre-created components
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

class State : private juce::Timer
{
public:
    State();
    ~State() override;

    /**
     * @brief Stores the parsed document. Does not signal flush.
     * Called from openFile before the parser thread starts.
     * @note MESSAGE THREAD.
     */
    void setDocument (jreng::Markdown::ParsedDocument&& doc);

    /**
     * @brief Increments the completed block counter with release fence.
     * Called once per block after setDocument().
     * @note PARSER THREAD.
     */
    void appendBlock() noexcept;

    /**
     * @brief Signals that parsing is complete with release fence.
     * Called once after all appendBlock() calls, even if content was empty.
     * @note PARSER THREAD.
     */
    void setParseComplete() noexcept;

    /**
     * @brief Returns the parsed markdown document.
     * @note MESSAGE THREAD.
     */
    const jreng::Markdown::ParsedDocument& getDocument() const noexcept;

    /**
     * @brief Returns a mutable reference to the parsed document for style resolution.
     * Only the parser thread calls this, after setDocument() and before setParseComplete().
     * @note PARSER THREAD.
     */
    jreng::Markdown::ParsedDocument& getDocumentForWriting() noexcept;

    /**
     * @brief Returns the DOCUMENT ValueTree for grafting into AppState.
     * @note MESSAGE THREAD.
     */
    juce::ValueTree getValueTree() const noexcept;

private:
    void timerCallback() override;

    /**
     * @brief Reads atomics and updates ValueTree properties when changed.
     * Returns true if anything was updated.
     * @note MESSAGE THREAD (called from timerCallback).
     */
    bool flush();

    juce::ValueTree state;
    jreng::Markdown::ParsedDocument document;

    std::atomic<int>  completedBlockCount { 0 };   ///< Written by parser thread (release), read by message thread (acquire)
    std::atomic<bool> parseComplete       { false }; ///< Written by parser thread (release), read by message thread (acquire)

    int lastFlushedBlockCount { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (State)
};

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Whelmed
