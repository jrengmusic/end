#include "State.h"

namespace whelmed
{
/*____________________________________________________________________________*/

State::State()
    : state (app::id::DOCUMENT)
{
    state.setProperty (app::id::filePath,      "",    nullptr);
    state.setProperty (app::id::displayName,   "",    nullptr);
    state.setProperty (app::id::scrollOffset,  0.0f,  nullptr);
    state.setProperty (app::id::blockCount,    0,     nullptr);
    state.setProperty (app::id::parseComplete, false, nullptr);
    state.setProperty (app::id::totalBlocks,   0,     nullptr);

    startTimerHz (60);
}

State::~State()
{
    stopTimer();
}

void State::setDocument (jam::Markdown::ParsedDocument&& doc)
{
    document = std::move (doc);
    completedBlockCount.store (0, std::memory_order_relaxed);
    lastFlushedBlockCount = 0;
}

void State::setInitialBlockCount (int count) noexcept
{
    completedBlockCount.store (count, std::memory_order_relaxed);
    lastFlushedBlockCount = count;
}

void State::appendBlock() noexcept
{
    completedBlockCount.fetch_add (1, std::memory_order_release);
}

void State::setParseComplete() noexcept
{
    parseComplete.store (true, std::memory_order_release);
}

void State::timerCallback()
{
    static constexpr int flushHz { 120 };
    static constexpr int idleHz  { 60 };

    const bool anythingUpdated { flush() };

    const int interval { anythingUpdated ? 1000 / flushHz : 1000 / idleHz };
    startTimer (interval);
}

bool State::flush()
{
    bool updated { false };

    const int currentCount { completedBlockCount.load (std::memory_order_acquire) };

    if (currentCount > lastFlushedBlockCount)
    {
        ++lastFlushedBlockCount;
        state.setProperty (app::id::blockCount, lastFlushedBlockCount, nullptr);
        updated = true;
    }

    if (parseComplete.exchange (false, std::memory_order_acquire))
    {
        state.setProperty (app::id::parseComplete, true, nullptr);
        updated = true;
    }

    return updated;
}

const jam::Markdown::ParsedDocument& State::getDocument() const noexcept
{
    return document;
}

jam::Markdown::ParsedDocument& State::getDocumentForWriting() noexcept
{
    return document;
}

juce::ValueTree State::getValueTree() const noexcept
{
    return state;
}

/**______________________________END OF NAMESPACE______________________________*/
} // namespace whelmed
