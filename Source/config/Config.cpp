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
    const auto& fromLua = [this] (int key, const juce::Identifier& type)
    {
        return jam::Model::fromLua (
            BinaryData::getString (File::getName (key)), type, {}, &validators);
    };

    for (auto& [key, value] : File::get())
    {
        juce::Identifier id { jam::Format::toValidID (value, true) };
        juce::ValueTree tree { fromLua (key, id) };
        if (key == File::config)
        {
            state = tree;
            theme.state = state;
            shader.state = state;
        }
        else
        {
            state.appendChild (tree, nullptr);
        }
    }

    jam::Model::applyFunctionRecursively (
        state,
        [this] (const juce::ValueTree& t)
        {
            auto tree { t };
            addProperties (tree, params);

            auto treeType { tree.getType() };

            jam::Model::forEachProperty (
                tree,
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

        if (key == File::config)
        {
            auto configTree { jam::Model::fromLua (
                file.loadFileAsString(), state.getType(), file.getFileName(), &validators, &fileErrors) };

            if (fileErrors.isNotEmpty())
                errors << file.getFileName() << ":\n" << fileErrors;

            if (configTree.isValid())
                setValuesFrom (configTree);
        }
        else
        {
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
    }

    if (errors.isEmpty())
        loadMessage = "RELOAD";
    else
        loadMessage = errors;

    state.sendPropertyChangeMessage (ID::loadMessage);

    theme.load (config::Themes::getPath (state.getProperty (ID::theme).toString()));

    shader.load (config::Shaders::getPath (getValue (IDtype::graphics, ID::background).toString()));
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
    if (event == jam::File::Watcher::Event::fileUpdated and file.hasFileExtension (File::extension))
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
    auto graphics { jam::Model::getChildWithName (state, IDtype::graphics) };

    if (graphics.isValid())
    {
        auto shaderChild { graphics.getOrCreateChildWithName (IDtype::shader, nullptr) };

        if (dir.isDirectory())
        {
            for (auto& [key, value] : config::Shaders::get())
            {
                const juce::File file { dir.getChildFile (value) };

                if (file.existsAsFile())
                    shaderChild.setProperty (
                        juce::Identifier { value }, file.loadFileAsString(), nullptr);
            }
        }
    }

    state.sendPropertyChangeMessage (IDtype::graphics);
}

//==============================================================================
void Theme::initialise()
{
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

        auto flexDir { jam::File::getOrCreateDirectory (dir, IDref::flex) };

        for (auto& [key, value] : config::Flex::get())
            writeWhenNeeded (flexDir, config::Flex::getName (key));
    }
}

void Theme::loadFromPath (const juce::File& dir)
{
    if (dir.isDirectory())
    {
        for (auto& [key, value] : config::Themes::get())
        {
            const juce::File file { dir.getChildFile (config::Themes::getName (key)) };

            if (file.existsAsFile())
            {
                auto tree { jam::Model::fromLua (file.loadFileAsString(), value.toUpperCase()) };

                if (tree.isValid())
                {
                    juce::ValueTree root { state.getType() };
                    root.appendChild (tree, nullptr);
                    setValuesFrom (root);
                }
            }
        }

        auto themeTree { jam::Model::getChildWithName (state, IDtype::theme) };

        if (themeTree.isValid())
        {
            auto flexChild { themeTree.getOrCreateChildWithName (IDtype::flex, nullptr) };

            auto flexDir { dir.getChildFile (IDref::flex) };

            if (flexDir.isDirectory())
            {
                for (auto& [key, value] : config::Flex::get())
                {
                    const juce::File file { flexDir.getChildFile (config::Flex::getName (key)) };

                    if (file.existsAsFile())
                        flexChild.setProperty (
                            juce::Identifier { value }, file.loadFileAsString(), nullptr);
                }
            }
        }
    }

    state.sendPropertyChangeMessage (ID::theme);
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace config
