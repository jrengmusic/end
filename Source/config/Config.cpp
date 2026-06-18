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
/* Wraps a Bimap contains() check as a string-enum validator. */
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
        auto child { jam::Model::fromLua (
            BinaryData::getString (File::getName (key)), value.toUpperCase(), {}, &validators) };

        if (child.isValid())
            state.appendChild (child, nullptr);
    }

    jam::Model::applyFunctionRecursively (
        state,
        [this] (const juce::ValueTree& t)
        {
            auto tree { t };
            addProperties (tree, params);

            auto treeType { tree.getType() };

            jam::Model::forEachProperty (tree,
                                         [this, treeType] (const juce::Identifier& name, const juce::var& value)
                                         {
                                             if (value.isString())
                                                 if (auto validator { getEnumValidator (value.toString()) })
                                                     registerValidator (treeType, name, std::move (validator));
                                         });

            return false;
        });
}

//==============================================================================
void Model::saveToPath()
{
    jam::File::getOrCreateDirectory (File::path.getParentDirectory(), File::path.getFileName());

    for (auto& [key, value] : File::get())
    {
        const auto name { File::getName (key) };
        const juce::File file { File::path.getChildFile (name) };

        if (not file.existsAsFile())
        {
            BinaryData::Raw raw (name);

            if (raw.exists())
                file.replaceWithData (raw.data, static_cast<size_t> (raw.size));
        }
    }
}

void Model::loadFromPath()
{
    juce::String errors;

    for (auto& [key, value] : File::get())
    {
        const juce::File file { File::path.getChildFile (File::getName (key)) };
        juce::String fileErrors;

        auto child { jam::Model::fromLua (
            file.loadFileAsString(), value.toUpperCase(), file.getFileName(), &validators, &fileErrors) };

        if (fileErrors.isNotEmpty())
            errors << file.getFileName() << ":\n" << fileErrors;

        if (child.isValid())
        {
            juce::ValueTree root { state.getType() };
            root.appendChild (child, nullptr);
            setValuesFrom (root);
        }
    }

    if (errors.isEmpty())
        loadMessage = "RELOAD";
    else
        loadMessage = errors;

    state.sendPropertyChangeMessage (ID::loadMessage);

    theme.load (config::Themes::getPath (getValue (IDtype::init, ID::theme).toString()));

    shader.load (config::Shaders::getPath (getValue (IDtype::shaders, ID::background).toString()));
}

void Model::startWatcher()
{
    watcher.addFolder (File::path);
    watcher.coalesceEvents (Directory::coalesceMs);
    watcher.addListener (this);
}

//==============================================================================
void Model::fileChanged (const juce::File& file, jam::File::Watcher::Event event)
{
    if (event == jam::File::Watcher::Event::fileUpdated
        and file.hasFileExtension (File::extension))
    {
        loadFromPath();
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
void Shader::loadFromPath (const juce::File& dir)
{
    state.removeAllProperties (nullptr);

    if (dir.isDirectory())
    {
        for (auto& [key, value] : config::Shaders::get())
        {
            const juce::File file { dir.getChildFile (value) };

            if (file.existsAsFile())
                state.setProperty (juce::Identifier { value }, file.loadFileAsString(), nullptr);
        }
    }

    state.sendPropertyChangeMessage (IDtype::shaders);
}

void Shader::fileChanged (const juce::File& file, jam::File::Watcher::Event event)
{
    if (event == jam::File::Watcher::Event::fileUpdated)
        loadFromPath (file.getParentDirectory());
}

//==============================================================================
void Theme::initialise()
{
    state.removeAllChildren (nullptr);

    for (auto& [key, value] : config::Themes::get())
    {
        auto child { jam::Model::fromLua (
            BinaryData::getString (config::Themes::getName (key)), value.toUpperCase()) };

        if (child.isValid())
            state.appendChild (child, nullptr);
    }
}

void Theme::saveToPath (const juce::File& dir)
{
    if (dir.getFullPathName().isNotEmpty())
    {
        auto writeWhenNeeded = [] (const juce::File& folder, const juce::String& fileName)
        {
            const juce::File file { folder.getChildFile (fileName) };

            if (not file.existsAsFile())
            {
                BinaryData::Raw raw (fileName);

                if (raw.exists())
                    file.replaceWithData (raw.data, static_cast<size_t> (raw.size));
            }
        };

        jam::File::getOrCreateDirectory (dir.getParentDirectory(), dir.getFileName());

        for (auto& [key, value] : config::Themes::get())
            writeWhenNeeded (dir, config::Themes::getName (key));

        auto graphicsDir { jam::File::getOrCreateDirectory (dir, jam::IDref::graphics) };

        jam::Model::applyFunctionRecursively (
            state,
            [graphicsDir, &writeWhenNeeded] (const juce::ValueTree& tree)
            {
                for (const auto& fileName : jam::Model::toStringArray (tree.getProperty (ID::graphics)))
                    if (fileName.isNotEmpty())
                        writeWhenNeeded (graphicsDir, fileName);

                return false;
            });
    }
}

void Theme::loadFromPath (const juce::File& dir)
{
    state.removeAllChildren (nullptr);

    if (dir.isDirectory())
    {
        for (auto& [key, value] : config::Themes::get())
        {
            const juce::File file { dir.getChildFile (config::Themes::getName (key)) };

            if (file.existsAsFile())
            {
                auto tree { jam::Model::fromLua (file.loadFileAsString(), value.toUpperCase()) };

                if (tree.isValid())
                    state.appendChild (tree, nullptr);
            }
        }
    }

    buildGraphicsCallbacks();

    state.sendPropertyChangeMessage (ID::theme);
}

void Theme::fileChanged (const juce::File& file, jam::File::Watcher::Event event)
{
    if (event == jam::File::Watcher::Event::fileUpdated)
    {
        if (file.hasFileExtension (config::Themes::extension))
            loadFromPath (file.getParentDirectory());
        else if (file.hasFileExtension (jam::IDref::svg))
        {
            auto fileName { file.getFileName() };

            if (graphicsCallbacks.contains (fileName))
                graphicsCallbacks.get (fileName);
        }
    }
}

void Theme::buildGraphicsCallbacks()
{
    graphicsCallbacks.clear();

    jam::Model::applyFunctionRecursively (
        state,
        [this] (const juce::ValueTree& tree)
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

/**______________________________END OF NAMESPACE______________________________*/
}// namespace config
