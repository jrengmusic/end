#include "LookAndFeel.h"

namespace config
{
/*____________________________________________________________________________*/

void LookAndFeel::load (const juce::Identifier& theme)
{
    auto themePath { Theme::getPath (theme.toString()) };

    if (not themePath.exists())
        themePath.createDirectory();

    state.removeAllChildren (nullptr);

    for (auto& [key, value] : Theme::get())
    {
        const auto name { Theme::getName (key) };
        const juce::File file { themePath.getChildFile (name) };

        if (not file.existsAsFile())
        {
            BinaryData::Raw raw (name);

            if (raw.exists())
                file.replaceWithData (raw.data, static_cast<size_t> (raw.size));
        }

        if (file.existsAsFile())
        {
            auto lua { jam::lua::State() };
            auto result { lua.getType (file.loadFileAsString(), file.getFileName()) };

            if (result.wasOk())
            {
                auto tree { jam::Model::fromLua (result.value(), value.toUpperCase()) };

                state.addChild (tree.createCopy(), -1, nullptr);
            }
        }
    }

    state.sendPropertyChangeMessage (ID::theme);
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace config
