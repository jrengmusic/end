#include "Config.h"

namespace config
{
/*____________________________________________________________________________*/

Model::Model()
    : jam::Model (IDtype::config)
{
    initialise();
    saveToPath();
    loadFromPath();
    startWatcher();
}

//==============================================================================
/* Wraps a Map::Instance contains() check as a string-enum validator. */
template<typename MapType>
static std::function<bool (const juce::var&)> enumCheck (MapType* map)
{
    return [map] (const juce::var& v)
    {
        return v.isString() and map->contains (v.toString());
    };
}

/* Resolves a BinaryData default string to its owning Map's validator.
   Returns an empty function when the value belongs to no known Map. */
static std::function<bool (const juce::var&)> getEnumValidator (const juce::String& value)
{
    if (end::Boolean::getInstance()->contains (value))
        return enumCheck (end::Boolean::getInstance());
    if (end::Position::getInstance()->contains (value))
        return enumCheck (end::Position::getInstance());
    if (end::GpuMode::getInstance()->contains (value))
        return enumCheck (end::GpuMode::getInstance());
    if (end::DropMode::getInstance()->contains (value))
        return enumCheck (end::DropMode::getInstance());
    return {};
}

//==============================================================================
void Model::initialise()
{
    for (auto& [key, value] : File::get())
    {
        if (key != File::config)
        {
            auto lua { jam::lua::State() };

            auto result { lua.getType (BinaryData::getString (File::getName (key))) };

            if (result.wasOk())
            {
                auto child { jam::Model::fromLua (
                    result.value(), value.toUpperCase(), &validators) };
                state.appendChild (child, nullptr);
            }
        }
    }

    jam::Model::applyFunctionRecursively (
        state,
        [this] (const juce::ValueTree& t)
        {
            auto tree { t };
            addProperties (tree, params);

            auto treeType { tree.getType() };

            for (int i { 0 }; i < tree.getNumProperties(); ++i)
            {
                auto name { tree.getPropertyName (i) };
                auto value { tree.getProperty (name) };

                if (value.isString())
                    if (auto validator { getEnumValidator (value.toString()) })
                        registerValidator (treeType, name, std::move (validator));
            }

            return false;
        });
}

//==============================================================================
void Model::saveToPath()
{
    auto writeWhenNeeded = [] (const juce::File& path, const auto& map)
    {
        if (not path.exists())
            path.createDirectory();

        for (auto& [key, value] : map.get())
        {
            const auto name { map.getName (key) };
            const juce::File file { path.getChildFile (name) };
            const bool existed { file.existsAsFile() };

            if (not existed)
            {
                BinaryData::Raw raw (name);

                if (raw.exists())
                    file.replaceWithData (raw.data, static_cast<size_t> (raw.size));
            }
        }
    };

    writeWhenNeeded (File::path, *File::getInstance());

    auto path { Graphics::path.getChildFile (getValue (IDtype::graphics, jam::ID::path).toString()) };
    writeWhenNeeded (path, *Graphics::getInstance());
}

void Model::loadFromPath()
{
    juce::String errors;

    for (auto& [key, value] : File::get())
    {
        if (key != File::config)
        {
            const juce::File file { File::path.getChildFile (File::getName (key)) };
            auto lua { jam::lua::State() };

            auto result { lua.getType (file.loadFileAsString(), file.getFileName()) };

            if (result.wasOk())
            {
                juce::String fileErrors;
                lua.getLineMapBuilder().flushRoot (value.toUpperCase());
                auto child { jam::Model::fromLua (result.value(),
                                                  value.toUpperCase(),
                                                  &validators,
                                                  &fileErrors,
                                                  &lua.getLineMap()) };

                if (fileErrors.isNotEmpty())
                    errors << file.getFileName() << ":\n" << fileErrors;

                setValuesFrom (child);
            }
            else
            {
                errors << file.getFileName() << ": " << result.getErrorMessage() << "\n";
            }
        }
    }

    //==============================================================================
#if JUCE_WINDOWS
    const float scale { jam::Typeface::getDisplayScale() };

    auto font { getCode() };
    float fontSize { font.getProperty (ID::fontSize) };

    if (scale > 0.0f)
    {
        fontSize /= scale;
        font.setProperty (fontSize);
    }
#endif

    //==============================================================================
    buildGraphicsCallbacks();

    if (errors.isEmpty())
        loadMessage = "RELOAD";
    else
        loadMessage = errors;

    state.sendPropertyChangeMessage (ID::loadMessage);
}

void Model::buildGraphicsCallbacks()
{
    graphicsCallbacks.clear();

    if (auto graphics { state.getChildWithName (IDtype::graphics) }; graphics.isValid())
    {
        for (auto& [key, value] : Graphics::get())
        {
            auto id { juce::Identifier (value) };
            auto fileName { graphics.getProperty (id).toString() };

            if (fileName.isNotEmpty())
            {
                graphicsCallbacks.add<juce::ValueTree> (fileName,
                                                        [id] (juce::ValueTree t)
                                                        {
                                                            t.sendPropertyChangeMessage (id);
                                                        });
            }
        }

        // Walk the nested tab_button child: register one callback per state slot
        // present in the config, in jam::map::ButtonState order.
        // Each callback fires sendPropertyChangeMessage on the tab_button child
        // (not the graphics root) keyed by the state identifier.
        if (auto tabButton { graphics.getChildWithName (IDtype::tabButton) }; tabButton.isValid())
        {
            const juce::Identifier stateIds[] {
                jam::ID::normal,   jam::ID::over,   jam::ID::down,   jam::ID::disabled,
                jam::ID::normalOn, jam::ID::overOn, jam::ID::downOn, jam::ID::disabledOn,
            };

            for (auto& stateId : stateIds)
            {
                auto fileName { tabButton.getProperty (stateId).toString() };

                if (fileName.isNotEmpty())
                {
                    graphicsCallbacks.add<juce::ValueTree> (
                        fileName,
                        [stateId] (juce::ValueTree t)
                        {
                            t.sendPropertyChangeMessage (stateId);
                        });
                }
            }
        }
    }
}

void Model::startWatcher()
{
    watcher.addFolder (File::path);
    watcher.coalesceEvents (300);
    watcher.addListener (this);
}
//==============================================================================
void Model::fileChanged (const juce::File& file, jam::File::Watcher::Event event)
{
    if (event == jam::File::Watcher::Event::fileUpdated)
    {
        if (file.hasFileExtension (File::extension))
        {
            loadFromPath();
        }
        else if (file.hasFileExtension (Graphics::extension))
        {
            auto fileName { file.getFileName() };

            if (graphicsCallbacks.contains (fileName))
                graphicsCallbacks.get (fileName);
        }
    }
}

//==============================================================================
void Model::registerValidator (juce::Identifier treeType,
                               juce::Identifier propertyName,
                               std::function<bool (const juce::var&)> validator)
{
    validators.try_emplace (treeType).first->second.insert_or_assign (
        propertyName, std::move (validator));
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace config
