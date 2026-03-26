/**
 * @file State.h
 * @brief Whelmed document state — ValueTree SSOT, parsed blocks, dirty tracking.
 *
 * Whelmed::State is the file-backed state model for markdown panes.
 * Pure ValueTree, message thread only. Grafts into AppState as
 * ID::DOCUMENT child of the PANE node.
 *
 * @see Whelmed::Component
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
     * @brief Re-reads the file from disk and re-parses blocks.
     * Sets dirty flag for re-render.
     * @note MESSAGE THREAD.
     */
    void reload();

    /**
     * @brief Returns dirty flag and clears it.
     * @return true if state has changed since last consume.
     * @note MESSAGE THREAD.
     */
    bool consumeDirty() noexcept;

    /**
     * @brief Returns the parsed markdown blocks.
     * @note MESSAGE THREAD.
     */
    const jreng::Markdown::Blocks& getBlocks() const noexcept;

    /**
     * @brief Returns the DOCUMENT ValueTree for grafting into AppState.
     * @note MESSAGE THREAD.
     */
    juce::ValueTree getValueTree() const noexcept;

private:
    juce::ValueTree state;
    juce::File file;
    jreng::Markdown::Blocks blocks;
    bool dirty { true };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (State)
};

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Whelmed
