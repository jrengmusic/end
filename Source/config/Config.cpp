#include "Config.h"

namespace config
{
/*____________________________________________________________________________*/

//==============================================================================
// Shader
//==============================================================================

Shader::Shader()
    : Directory (jam::Model::fromFiles (
          IDtype::shader, file::Shaders::get(), [] (int) { return juce::String(); }))
{
}

void Shader::loadFromPath (juce::String& errors)
{
    // errors is unused — shader source has no lua parse step.
    // The param satisfies the Directory contract; leave it untouched.
    juce::ignoreUnused (errors);

    const juce::String project { config::Model::getInstance()->getValue (IDtype::graphics, ID::background).toString() };
    const juce::File dir { file::Shaders::getPath (project) };

    if (dir.isDirectory())
    {
        auto disk { jam::Model::fromFiles (IDtype::shader, file::Shaders::get(),
            [dir] (int key) { return dir.getChildFile (file::Shaders::get().at (key)).loadFileAsString(); }) };

        setValuesFrom (disk);
    }

    state.sendPropertyChangeMessage (IDtype::graphics);
}

//==============================================================================
// Theme
//==============================================================================

Theme::Theme()
    : Directory (jam::Model::fromLua (
          IDtype::themes, file::Themes::get(),
          [] (int key) { return BinaryData::getString (file::Themes::getName (key)); }))
{
    auto flex { jam::Model::fromFiles (
        IDtype::flex, file::Flex::get(),
        [] (int key) { return BinaryData::getString (file::Flex::getName (key)); }) };

    state.appendChild (flex, nullptr);
}

void Theme::saveToPath()
{
    const auto themeName { config::Model::getInstance()->getValue (IDtype::display, ID::theme).toString() };
    const juce::File dir { file::Themes::getPath (themeName) };

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

        for (auto& [key, value] : file::Themes::get())
            writeWhenNeeded (dir, file::Themes::getName (key));

        auto flexDir { jam::File::getOrCreateDirectory (dir, IDref::flex) };

        for (auto& [key, value] : file::Flex::get())
            writeWhenNeeded (flexDir, file::Flex::getName (key));
    }
}

void Theme::loadFromPath (juce::String& errors)
{
    const juce::File dir { file::Themes::getPath (
        config::Model::getInstance()->getValue (IDtype::display, ID::theme).toString()) };

    if (dir.isDirectory())
    {
        auto disk { jam::Model::fromLua (IDtype::themes, file::Themes::get(),
            [dir] (int key) { return dir.getChildFile (file::Themes::getName (key)).loadFileAsString(); },
            &config::Model::getValidators(), &errors) };

        const juce::File flexDir { dir.getChildFile (IDref::flex) };
        auto flexDisk { jam::Model::fromFiles (IDtype::flex, file::Flex::get(),
            [flexDir] (int key) { return flexDir.getChildFile (file::Flex::getName (key)).loadFileAsString(); }) };

        disk.appendChild (flexDisk, nullptr);

        setValuesFrom (disk);
    }

    state.sendPropertyChangeMessage (ID::theme);
}

//==============================================================================
// Model
//==============================================================================

Model::Model()
    : jam::Model (jam::Model::fromLua (
          IDtype::config, file::Config::get(),
          [] (int key) { return BinaryData::getString (file::Config::getName (key)); }))
{
    // theme and shader members are now constructed — attach their subtrees.
    state.appendChild (theme.state, nullptr);

    auto graphics { jam::Model::getChildWithName (state, IDtype::graphics) };
    graphics.appendChild (shader.state, nullptr);

    saveToPath();
    loadFromPath();
    startWatcher();
}

void Model::saveToPath()
{
    jam::File::getOrCreateDirectory (file::Config::path.getParentDirectory(), file::Config::path.getFileName());

    for (auto& [key, value] : file::Config::get())
    {
        const auto name { file::Config::getName (key) };
        const juce::File file { file::Config::path.getChildFile (name) };

        if (not file.existsAsFile())
        {
            BinaryData::Raw raw (name);

            if (raw.exists())
                file.replaceWithData (raw.data, static_cast<size_t> (raw.size));
        }
    }

    theme.saveToPath();
}

void Model::loadFromPath()
{
    juce::String errors;

    auto disk { jam::Model::fromLua (IDtype::config, file::Config::get(),
        [] (int key) { return file::Config::getPath (file::Config::getName (key)).loadFileAsString(); },
        &validators, &errors) };

    setValuesFrom (disk);

    theme.loadFromPath (errors);
    shader.loadFromPath (errors);

    const juce::String message { errors.isEmpty() ? getValue (IDtype::display, ID::successMessage).toString() : errors };
    appModel.setMessage (message);
}

void Model::startWatcher()
{
    watcher.addFolder (file::Config::path);
    watcher.coalesceEvents (coalesceMs);
    watcher.addListener (this);
}

void Model::fileChanged (const juce::File& file, jam::File::Watcher::Event event)
{
    if (event == jam::File::Watcher::Event::fileUpdated and file.hasFileExtension (file::Config::extension))
        loadFromPath();
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace config
