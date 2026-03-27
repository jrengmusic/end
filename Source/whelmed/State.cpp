#include "State.h"

namespace Whelmed
{ /*____________________________________________________________________________*/

State::State()
    : state (App::ID::DOCUMENT)
{
    state.setProperty (App::ID::filePath,     "", nullptr);
    state.setProperty (App::ID::displayName,  "", nullptr);
    state.setProperty (App::ID::scrollOffset, 0.0f, nullptr);
}

void State::setDocument (jreng::Markdown::ParsedDocument&& doc)
{
    document = std::move (doc);
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
