#include "State.h"

namespace Whelmed
{ /*____________________________________________________________________________*/

State::State (const juce::File& file_)
    : state (App::ID::DOCUMENT)
    , file (file_)
{
    state.setProperty (App::ID::filePath,    file.getFullPathName(),            nullptr);
    state.setProperty (App::ID::displayName, file.getFileNameWithoutExtension(), nullptr);
    state.setProperty (App::ID::scrollOffset, 0.0f,                             nullptr);

    reload();
}

void State::reload()
{
    auto content { file.loadFileAsString() };
    blocks = jreng::Markdown::Parser::getBlocks (content);
    dirty = true;
}

bool State::consumeDirty() noexcept
{
    bool wasDirty { dirty };
    dirty = false;
    return wasDirty;
}

const jreng::Markdown::Blocks& State::getBlocks() const noexcept
{
    return blocks;
}

juce::ValueTree State::getValueTree() const noexcept
{
    return state;
}

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Whelmed
