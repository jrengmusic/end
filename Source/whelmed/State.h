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

    void setDocument (jreng::Markdown::ParsedDocument&& doc);
    void setInitialBlockCount (int count) noexcept;
    void appendBlock() noexcept;
    void setParseComplete() noexcept;

    const jreng::Markdown::ParsedDocument& getDocument() const noexcept;
    jreng::Markdown::ParsedDocument& getDocumentForWriting() noexcept;
    juce::ValueTree getValueTree() const noexcept;

private:
    void timerCallback() override;
    bool flush();

    juce::ValueTree state;
    jreng::Markdown::ParsedDocument document;

    std::atomic<int>  completedBlockCount { 0 };
    std::atomic<bool> parseComplete       { false };
    int lastFlushedBlockCount { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (State)
};

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Whelmed
