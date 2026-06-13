#include "LookAndFeel.h"

namespace config
{
/*____________________________________________________________________________*/

void LookAndFeel::load (const juce::Identifier& theme)
{
    auto path { Theme::getPath (theme.toString()) };

    if (not path.exists())
        path.createDirectory();

    const juce::File themeFile { path.getChildFile (Theme::getName()) };

    if (not themeFile.existsAsFile())
    {
        BinaryData::Raw raw (Theme::getName());

        if (raw.exists())
            themeFile.replaceWithData (raw.data, static_cast<size_t> (raw.size));
    }

    if (themeFile.existsAsFile())
    {
        auto lua { jam::lua::State() };
        auto result { lua.getType (themeFile.loadFileAsString(), themeFile.getFileName()) };

        if (result.wasOk())
        {
            auto tree { jam::Model::fromLua (result.value(), theme.toString().toUpperCase()) };

            state.copyPropertiesAndChildrenFrom (tree, nullptr);
        }
    }
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace config
