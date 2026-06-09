#include "Config.h"

namespace config
{
/*____________________________________________________________________________*/

Model::Model()
    : jam::Model (IDtype::config)
{
    initialise();
    writeToPath();
}

//==============================================================================
juce::Rectangle<int> Model::getInitWindowSize() const noexcept
{
    auto window { jam::Model::getChildWithName (state, IDtype::window) };
    auto size { juce::StringArray::fromTokens (window.getProperty (ID::size).toString(), ',') };
    enum
    {
        width,
        height
    };

    return { size[width].getIntValue(), size[height].getIntValue() };
}

//==============================================================================
void Model::initialise()
{
    for (auto& [key, value] : File::get())
    {
        if (key != File::config)
        {
            auto lua { jam::lua::State() };

            if (auto result { lua.getType (BinaryData::getString (File::getName (key))) };
                result.wasOk())
            {
                auto child { jam::Model::fromLua (result.value(), value.toUpperCase()) };
                state.appendChild (child, nullptr);
            }
        }
    }

    jam::Model::applyFunctionRecursively (state,
                                          [this] (const juce::ValueTree& node)
                                          {
                                              auto mutableNode { node };
                                              addProperties (mutableNode, params);
                                              return false;
                                          });
}

//==============================================================================
void Model::writeToPath()
{
    auto writeWhenNeeded = [] (const juce::File& path, const auto& map) -> bool
    {
        if (not path.exists())
            path.createDirectory();

        for (auto& [key, value] : map.get())
        {
            const auto name { map.getName (key) };
            const juce::File file { path.getChildFile (name) };

            if (not file.existsAsFile())
            {
                BinaryData::Raw raw (name);
                file.replaceWithData (raw.data, static_cast<size_t> (raw.size));
            }
        }
    };

    writeWhenNeeded (File::path, *File::getContext());

    auto graphics { jam::Model::getChildWithName (state, IDtype::graphics) };
    auto path { Graphics::path.getChildFile (graphics.getProperty (jam::ID::path).toString()) };
    writeWhenNeeded (path, *Graphics::getContext());
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace config
