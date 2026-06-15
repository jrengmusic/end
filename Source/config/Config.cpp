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
    if (end::Position::getInstance()->contains (value))
        return enumCheck (end::Position::getInstance());
    if (end::DropMode::getInstance()->contains (value))
        return enumCheck (end::DropMode::getInstance());
    return {};
}

//==============================================================================
void Model::initialise()
{
    for (auto& [key, value] : File::get())
    {
        auto lua { jam::lua::State() };

        auto result { lua.getType (BinaryData::getString (File::getName (key))) };

        if (result.wasOk())
        {
            auto child { jam::Model::fromLua (result.value(), value.toUpperCase(), &validators) };
            state.appendChild (child, nullptr);
        }
    }

    // Theme lua files — build theme tree so saveToPath can walk it for SVG filenames.
    for (auto& [key, value] : File::Theme::get())
    {
        auto lua { jam::lua::State() };

        auto result { lua.getType (BinaryData::getString (File::Theme::getName (key))) };

        if (result.wasOk())
        {
            auto child { jam::Model::fromLua (result.value(), value.toUpperCase()) };
            theme.state.appendChild (child, nullptr);
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

    auto themeName { getValue (IDtype::end, ID::theme).toString() };
    auto themePath { File::Theme::getPath (themeName) };

    writeWhenNeeded (themePath, *File::Theme::getInstance());

    // Seed SVG graphics assets — walk theme tree for filenames.
    auto graphicsPath { themePath.getChildFile (jam::IDref::graphics) };

    if (not graphicsPath.exists())
        graphicsPath.createDirectory();

    jam::Model::applyFunctionRecursively (
        theme.state,
        [&] (const juce::ValueTree& tree)
        {
            for (const auto& fileName : jam::Model::toStringArray (tree.getProperty (ID::graphics)))
            {
                if (fileName.isNotEmpty())
                {
                    juce::File file { graphicsPath.getChildFile (fileName) };

                    if (not file.existsAsFile())
                    {
                        BinaryData::Raw raw (fileName);

                        if (raw.exists())
                            file.replaceWithData (raw.data, static_cast<size_t> (raw.size));
                    }
                }
            }

            return false;
        });
}

void Model::loadFromPath()
{
    juce::String errors;

    for (auto& [key, value] : File::get())
    {
        const juce::File file { File::path.getChildFile (File::getName (key)) };
        auto lua { jam::lua::State() };

        auto result { lua.getType (file.loadFileAsString(), file.getFileName()) };

        if (result.wasOk())
        {
            juce::String fileErrors;
            lua.getLineMapBuilder().flushRoot (value.toUpperCase());
            auto child { jam::Model::fromLua (
                result.value(), value.toUpperCase(), &validators, &fileErrors, &lua.getLineMap()) };

            if (fileErrors.isNotEmpty())
                errors << file.getFileName() << ":\n" << fileErrors;

            setValuesFrom (child);
        }
        else
        {
            errors << file.getFileName() << ": " << result.getErrorMessage() << "\n";
        }
    }

    //==============================================================================
    buildGraphicsCallbacks();

    if (errors.isEmpty())
        loadMessage = "RELOAD";
    else
        loadMessage = errors;

    state.sendPropertyChangeMessage (ID::loadMessage);

    theme.load (juce::Identifier { getValue (IDtype::end, ID::theme) });
}

void Model::buildGraphicsCallbacks()
{
    graphicsCallbacks.clear();

    jam::Model::applyFunctionRecursively (
        theme.state,
        [&] (const juce::ValueTree& tree)
        {
            for (const auto& fileName : jam::Model::toStringArray (tree.getProperty (ID::graphics)))
            {
                if (fileName.isNotEmpty())
                {
                    auto stem { jam::Format::getFilenameWithoutExtension (fileName) };
                    auto suffix { stem.fromLastOccurrenceOf ("_", false, false) };

                    juce::Identifier id {
                        suffix.isNotEmpty()
                                and jam::map::ButtonState::getInstance()->contains (suffix)
                            ? suffix
                            : stem
                    };

                    graphicsCallbacks.add<juce::ValueTree> (fileName,
                                                            [id] (juce::ValueTree t)
                                                            {
                                                                t.sendPropertyChangeMessage (id);
                                                            });
                }
            }

            return false;
        });
}

void Model::startWatcher()
{
    watcher.addFolder (File::path);

    auto themeName { getValue (IDtype::end, ID::theme).toString() };
    watcher.addFolder (File::Theme::getPath (themeName));

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
        else if (file.hasFileExtension (jam::IDref::svg))
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

//==============================================================================
void Theme::load (const juce::Identifier& themeName)
{
    auto themePath { File::Theme::getPath (themeName.toString()) };

    if (not themePath.exists())
        themePath.createDirectory();

    state.removeAllChildren (nullptr);

    for (auto& [key, value] : File::Theme::get())
    {
        const auto name { File::Theme::getName (key) };
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
