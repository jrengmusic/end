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
    if (end::Boolean::getContext()->contains (value))
        return enumCheck (end::Boolean::getContext());
    if (end::Position::getContext()->contains (value))
        return enumCheck (end::Position::getContext());
    if (end::GpuMode::getContext()->contains (value))
        return enumCheck (end::GpuMode::getContext());
    if (end::DropMode::getContext()->contains (value))
        return enumCheck (end::DropMode::getContext());
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

            if (auto result { lua.getType (BinaryData::getString (File::getName (key))) };
                result.wasOk())
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

void Model::loadFromPath()
{
    juce::String errors;

    for (auto& [key, value] : File::get())
    {
        if (key != File::config)
        {
            const juce::File file { File::path.getChildFile (File::getName (key)) };
            auto lua { jam::lua::State() };

            if (auto result { lua.getType (file.loadFileAsString(), file.getFileName()) };
                result.wasOk())
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
