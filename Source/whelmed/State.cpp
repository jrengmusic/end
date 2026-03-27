#include "State.h"

namespace Whelmed
{ /*____________________________________________________________________________*/

State::State (const juce::File& file_)
    : state (App::ID::DOCUMENT)
    , file (file_)
{
    state.setProperty (App::ID::filePath,     file.getFullPathName(),             nullptr);
    state.setProperty (App::ID::displayName,  file.getFileNameWithoutExtension(), nullptr);
    state.setProperty (App::ID::scrollOffset, 0.0f,                              nullptr);
}

void State::commitDocument (jreng::Markdown::ParsedDocument&& doc)
{
    document = std::move (doc);
    needsFlush.store (true, std::memory_order_release);
}

bool State::flush() noexcept
{
    return needsFlush.exchange (false, std::memory_order_acquire);
}

const jreng::Markdown::ParsedDocument& State::getDocument() const noexcept
{
    return document;
}

juce::ValueTree State::getValueTree() const noexcept
{
    return state;
}

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Whelmed
