#include "Config.h"

namespace config
{
/*____________________________________________________________________________*/

const juce::File File::path {
    juce::File::getSpecialLocation (juce::File::userHomeDirectory).getChildFile (".config/end")
};

const juce::String File::extension { "*.lua" };

const std::unordered_map<int, juce::String> File::map {
    { File::config,  IDref::config  },
    { File::whelmed, IDref::whelmed },
    { File::nexus,   IDref::nexus   },
    { File::display, IDref::display },
    { File::actions, IDref::actions },
    { File::popups,  IDref::popups  },
    { File::keys,    IDref::keys    },
};

const juce::String File::getName (int key) noexcept
{
    return jam::Text::toFileName (map.at (key), extension);
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
                if (auto xml { jam::lua::Xml::from (result.value(), value.toUpperCase()) })
                {
                    auto child { juce::ValueTree::fromXml (*xml) };
                    state.appendChild (child, nullptr);
                }
            }
        }
    }

    jam::ValueTree::applyFunctionRecursively (state,
                                              [this] (const juce::ValueTree& node)
                                              {
                                                  auto mutableNode { node };
                                                  addParametersFromProperties (mutableNode, params);
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
        if (auto xml { jam::lua::Xml::from (
                result.value(), file.getFileNameWithoutExtension().toUpperCase()) })
        {
            auto child { juce::ValueTree::fromXml (*xml) };
            setValuesFrom (child);
        }
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

    return errors;
}

//==============================================================================
/**______________________________END OF NAMESPACE______________________________*/
}// namespace config
