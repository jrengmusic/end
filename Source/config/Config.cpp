#include "Config.h"

namespace config
{
/*____________________________________________________________________________*/

const juce::File File::path {
    juce::File::getSpecialLocation (juce::File::userHomeDirectory).getChildFile (".config/end")
};

const juce::String File::extension { "*.lua" };

const std::unordered_map<int, juce::String> File::map {
    { File::config,   IDref::config   },
    { File::whelmed,  IDref::whelmed  },
    { File::nexus,    IDref::nexus    },
    { File::display,  IDref::display  },
    { File::graphics, IDref::graphics },
    { File::actions,  IDref::actions  },
    { File::popups,   IDref::popups   },
    { File::keys,     IDref::keys     },
};

const juce::String File::getName (int key) noexcept
{
    return jam::Format::toFileName (map.at (key), extension);
}

//==============================================================================
Model::Model()
    : jam::Model (IDtype::config)
{
    for (auto& [key, value] : File::map)
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

const juce::ValueTree Model::get() noexcept { return getContext()->state; }

//==============================================================================
void Model::load (const juce::File& file, juce::String& errorOut)
{
    errorOut.clear();
    auto text { file.loadFileAsString() };
    jassert (text.isNotEmpty());

    auto lua { jam::lua::State() };

    if (auto result { lua.getType (text) }; result.wasOk())
    {
        auto child { jam::Model::fromLua (
            result.value(), file.getFileNameWithoutExtension().toUpperCase()) };
        setValuesFrom (child);
    }
    else
    {
        errorOut = file.getFullPathName() + ": " + result.getErrorMessage();
    }
}

//==============================================================================
juce::StringArray Model::loadPath (const juce::File& dir)
{
    juce::StringArray errors;
    dir.createDirectory();
    dir.getChildFile (jam::IDref::graphics).createDirectory();

    for (auto& [key, value] : File::map)
    {
        if (key != File::config)
        {
            const juce::File file { dir.getChildFile (File::getName (key)) };

            if (file.existsAsFile())
            {
                juce::String errorOut;
                load (file, errorOut);

                if (errorOut.isNotEmpty())
                {
                    errors.add (errorOut);
                    errorOut.clear();
                }
            }
            else
            {
                file.replaceWithText (BinaryData::getString (File::getName (key)));
            }
        }
    }

    watcher.addFolder (dir);
    watcher.coalesceEvents (300);
    watcher.addListener (this);

    return errors;
}

//==============================================================================
void Model::fileChanged (const juce::File& file, jam::File::Watcher::Event event)
{
    if (event == jam::File::Watcher::Event::fileUpdated)
    {
        if (file.hasFileExtension (File::extension))
        {
            juce::String errorOut;
            load (file, errorOut);
        }
        else if (file.hasFileExtension ("svg"))
        {
            auto graphics { state.getChildWithName (IDtype::graphics) };

            if (graphics.isValid())
            {
                auto fileName { file.getFileName() };

                if (fileName == graphics.getProperty (ID::tabBar).toString())
                    graphics.sendPropertyChangeMessage (ID::tabBar);
                else if (fileName == graphics.getProperty (ID::tabInactive).toString())
                    graphics.sendPropertyChangeMessage (ID::tabInactive);
                else if (fileName == graphics.getProperty (ID::tabActive).toString())
                    graphics.sendPropertyChangeMessage (ID::tabActive);
            }
        }
    }
}

const juce::Rectangle<int> Model::getInitWindowSize()
{
    auto window { get().getChildWithName (IDtype::display).getChildWithName (IDtype::window) };
    auto csv { window.getProperty (ID::size).toString() };
    auto size { juce::StringArray::fromTokens (csv, ",", "") };
    enum
    {
        width,
        height
    };

    juce::Rectangle<int> init;

    return init.withSize (size[width].getIntValue(), size[height].getIntValue());
}

//==============================================================================
/**______________________________END OF NAMESPACE______________________________*/
}// namespace config
