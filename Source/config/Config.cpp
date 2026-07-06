#include "Config.h"

namespace config
{
/*____________________________________________________________________________*/

//==============================================================================
// Shader
//==============================================================================

Shader::Shader (juce::Identifier treeType)
    : Directory (juce::ValueTree (treeType))
{
}

void Shader::loadFromPath (const juce::var& path, juce::String& errors)
{
    juce::ignoreUnused (errors);

    const juce::File dir { file::Shaders::getPath (path.toString()) };
    const auto presetFiles { dir.findChildFiles (
        juce::File::findFiles,
        false,
        jam::vulkan::ShaderFormat::getExtension().at (jam::vulkan::ShaderFormat::slang)) };
    const auto presetFile { not presetFiles.isEmpty() ? presetFiles.getReference (0)
                                                      : juce::File() };
    const auto preset { jam::vulkan::ShaderPreset::parse (presetFile.loadFileAsString()) };
    const int format { preset.passes.isEmpty() ? jam::vulkan::ShaderFormat::shadertoy
                                               : jam::vulkan::ShaderFormat::slang };

    setValuesFrom (jam::vulkan::ShaderFormat::load (format, state.getType(), dir));
    state.setProperty (ID::shaderFormat, format, nullptr);
    state.setProperty (jam::ID::path, dir.getFullPathName(), nullptr);

    state.sendPropertyChangeMessage (IDtype::graphics);
}

//==============================================================================
// Theme
//==============================================================================

Theme::Theme()
    : Directory (jam::Model::fromLua (
          IDtype::themes,
          file::Themes::get(),
          [] (int key)
          {
              return BinaryData::getString (file::Themes::getName (key));
          },
          &config::Model::validators))
{
    auto flex { jam::Model::fromFiles (IDtype::flex,
                                       file::Flex::get(),
                                       [] (int key)
                                       {
                                           return BinaryData::getString (file::Flex::getName (key));
                                       }) };

    state.appendChild (flex, nullptr);
}

void Theme::saveToPath (const juce::var& path)
{
    const juce::File dir { file::Themes::getPath (path.toString()) };

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

void Theme::loadFromPath (const juce::var& path, juce::String& errors)
{
    const juce::File dir { file::Themes::getPath (path.toString()) };

    if (dir.isDirectory())
    {
        auto disk { jam::Model::fromLua (
            IDtype::themes,
            file::Themes::get(),
            [dir] (int key)
            {
                return dir.getChildFile (file::Themes::getName (key)).loadFileAsString();
            },
            &config::Model::validators,
            &errors) };

        const juce::File flexDir { dir.getChildFile (IDref::flex) };
        auto flexDisk { jam::Model::fromFiles (
            IDtype::flex,
            file::Flex::get(),
            [flexDir] (int key)
            {
                return flexDir.getChildFile (file::Flex::getName (key)).loadFileAsString();
            }) };

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
          IDtype::config,
          file::Config::get(),
          [] (int key)
          {
              return BinaryData::getString (file::Config::getName (key));
          },
          &validators))
{
    // theme, background, and postProcessing members are now constructed — attach their subtrees.
    state.appendChild (theme.state, nullptr);

    auto graphics { jam::Model::getChildWithName (state, IDtype::graphics) };
    graphics.appendChild (background.state, nullptr);
    graphics.appendChild (postProcessing.state, nullptr);

    registerParameters();

    saveToPath();
    loadFromPath();
    startWatcher();
}

void Model::registerParameters()
{
    jam::Model::applyFunctionRecursively (
        state,
        [this] (const juce::ValueTree& tree)
        {
            const auto tag { tree.getType() };

            if (validators.contains (tag))
            {
                const auto& tagValidators { validators.at (tag) };
                auto target { tree };

                jam::Model::forEachProperty (
                    tree,
                    [this, &tagValidators, &target] (
                        const juce::Identifier& id, const juce::var& value)
                    {
                        if (tagValidators.contains (id) and tagValidators.at (id).create)
                            tagValidators.at (id).create (*this, target, id, value);
                    });
            }

            return false;
        });

    // Shader properties (GLSL source) have no validator — fromFiles does not populate them.
    // Register each as ParameterText with glslBufferSize. The two-level key
    // (treeType, propertyId) prevents collision between (BACKGROUND, Image) and
    // (POST_PROCESSING, Image).
    jam::Model::forEachProperty (
        background.state,
        [this] (const juce::Identifier& id, const juce::var& value)
        {
            if (value.isString())
                createAndAddParameter<jam::ParameterText> (
                    background.state, id, value.toString(), glslBufferSize);
        });

    jam::Model::forEachProperty (
        postProcessing.state,
        [this] (const juce::Identifier& id, const juce::var& value)
        {
            if (value.isString())
                createAndAddParameter<jam::ParameterText> (
                    postProcessing.state, id, value.toString(), glslBufferSize);
        });
}

void Model::saveToPath()
{
    jam::File::getOrCreateDirectory (
        file::Config::path.getParentDirectory(), file::Config::path.getFileName());

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

    theme.saveToPath (getValue (IDtype::display, ID::theme));
}

void Model::loadFromPath()
{
    juce::String errors;

    auto disk { jam::Model::fromLua (
        IDtype::config,
        file::Config::get(),
        [] (int key)
        {
            return file::Config::getPath (file::Config::getName (key)).loadFileAsString();
        },
        &validators,
        &errors) };

    // Load dependent resources BEFORE overlay — setValuesFrom fires parameter
    // notifications and consumers must read fresh source at that point.
    auto diskDisplay { jam::Model::getChildWithName (disk, IDtype::display) };
    theme.loadFromPath (diskDisplay.getProperty (ID::theme), errors);

    auto diskGraphics { jam::Model::getChildWithName (disk, IDtype::graphics) };
    background.loadFromPath (diskGraphics.getProperty (ID::background), errors);
    postProcessing.loadFromPath (diskGraphics.getProperty (ID::postProcessing), errors);

    setValuesFrom (disk);

    const juce::String message { errors.isEmpty()
                                     ? getValue (IDtype::display, ID::successMessage).toString()
                                     : errors };
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
    if (event == jam::File::Watcher::Event::fileUpdated
        and file.hasFileExtension (file::Config::extension))
        loadFromPath();
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace config
