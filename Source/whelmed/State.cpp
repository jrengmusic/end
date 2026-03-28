#include "State.h"

namespace Whelmed
{ /*____________________________________________________________________________*/

State::State()
    : state (App::ID::DOCUMENT)
{
    state.setProperty (App::ID::filePath,      "",    nullptr);
    state.setProperty (App::ID::displayName,   "",    nullptr);
    state.setProperty (App::ID::scrollOffset,  0.0f,  nullptr);
    state.setProperty (App::ID::blockCount,    0,     nullptr);
    state.setProperty (App::ID::parseComplete, false, nullptr);
    state.setProperty (App::ID::pendingPrefix, 0,     nullptr);
    state.setProperty (App::ID::totalBlocks,   0,     nullptr);

    startTimerHz (60);
}

State::~State()
{
    stopTimer();
}

void State::setDocument (jreng::Markdown::ParsedDocument&& doc)
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
        state.setProperty (App::ID::blockCount, lastFlushedBlockCount, nullptr);
        updated = true;
    }

    if (parseComplete.exchange (false, std::memory_order_acquire))
    {
        state.setProperty (App::ID::parseComplete, true, nullptr);
        updated = true;
    }

    return updated;
}

const jreng::Markdown::ParsedDocument& State::getDocument() const noexcept
{
    return document;
}

jreng::Markdown::ParsedDocument& State::getDocumentForWriting() noexcept
{
    return document;
}

juce::ValueTree State::getValueTree() const noexcept
{
    return state;
}

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Whelmed
