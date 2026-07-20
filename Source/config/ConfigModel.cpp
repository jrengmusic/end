#include "ConfigModel.h"

//==============================================================================
// ConfigShader
//==============================================================================

ConfigShader::ConfigShader (juce::Identifier treeType)
    : ConfigDirectory (juce::ValueTree (treeType))
{
}

void ConfigShader::loadFromPath (const juce::var& path, juce::String& errors)
{
    juce::ignoreUnused (errors);

    const juce::File dir { Id::Files::Shaders::getPath (path.toString()) };
    const auto presetFiles { dir.findChildFiles (
        juce::File::findFiles,
        false,
        jam::VulkanShaderFormat::getExtension().at (jam::VulkanShaderFormat::slang)) };
    const auto presetFile { not presetFiles.isEmpty() ? presetFiles.getReference (0)
                                                      : juce::File() };
    const auto preset { jam::VulkanShaderPreset::parse (presetFile.loadFileAsString()) };
    const int format { preset.passes.isEmpty() ? jam::VulkanShaderFormat::shadertoy
                                               : jam::VulkanShaderFormat::slang };

    setValuesFrom (jam::VulkanShaderFormat::load (format, state.getType(), dir));
    state.setProperty (Id::shaderFormat, format, nullptr);
    state.setProperty (Id::path, dir.getFullPathName(), nullptr);

    state.sendPropertyChangeMessage (Id::toType (Id::graphics));
}

//==============================================================================
// ConfigTheme
//==============================================================================

ConfigTheme::ConfigTheme()
    : ConfigDirectory (jam::lua::fromLua (
          Id::toType (Id::themes),
          Id::FileThemes::get(),
          [] (int key)
          {
              return BinaryData::getString (Id::Files::Themes::getName (key));
          },
          &ConfigModel::getValidators()))
{
    auto flex { jam::Model::fromFiles (Id::toType (Id::flex),
                                       Id::FileFlex::get(),
                                       [] (int key)
                                       {
                                           return BinaryData::getString (Id::Files::Flex::getName (key));
                                       }) };

    state.appendChild (flex, nullptr);
}

void ConfigTheme::saveToPath (const juce::var& path)
{
    const juce::File dir { Id::Files::Themes::getPath (path.toString()) };

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

        for (auto& [key, value] : Id::FileThemes::get())
            writeWhenNeeded (dir, Id::Files::Themes::getName (key));

        auto flexDir { jam::File::getOrCreateDirectory (dir, Id::flex) };

        for (auto& [key, value] : Id::FileFlex::get())
            writeWhenNeeded (flexDir, Id::Files::Flex::getName (key));
    }
}

void ConfigTheme::loadFromPath (const juce::var& path, juce::String& errors)
{
    const juce::File dir { Id::Files::Themes::getPath (path.toString()) };

    if (dir.isDirectory())
    {
        auto disk { jam::lua::fromLua (
            Id::toType (Id::themes),
            Id::FileThemes::get(),
            [dir] (int key)
            {
                return dir.getChildFile (Id::Files::Themes::getName (key)).loadFileAsString();
            },
            &ConfigModel::getValidators(),
            &errors) };

        const juce::File flexDir { dir.getChildFile (Id::flex) };
        auto flexDisk { jam::Model::fromFiles (
            Id::toType (Id::flex),
            Id::FileFlex::get(),
            [flexDir] (int key)
            {
                return flexDir.getChildFile (Id::Files::Flex::getName (key)).loadFileAsString();
            }) };

        disk.appendChild (flexDisk, nullptr);

        setValuesFrom (disk);
    }

    state.sendPropertyChangeMessage (Id::theme);
}

//==============================================================================
// ConfigModel
//==============================================================================

ConfigModel::ConfigModel()
    : jam::Model (jam::lua::fromLua (
          Id::toType (Id::config),
          Id::FileConfig::get(),
          [] (int key)
          {
              return BinaryData::getString (Id::Files::Config::getName (key));
          },
          &getValidators()))
{
    // theme, background, and postProcessing members are now constructed — attach their subtrees.
    state.appendChild (theme.state, nullptr);

    auto graphics { jam::Model::getChildWithName (state, Id::toType (Id::graphics)) };
    graphics.appendChild (background.state, nullptr);
    graphics.appendChild (postProcessing.state, nullptr);

    registerParameters();

    saveToPath();
    loadFromPath();
    startWatcher();
}

void ConfigModel::registerParameters()
{
    jam::Model::applyFunctionRecursively (
        state,
        [this] (const juce::ValueTree& tree)
        {
            const auto tag { tree.getType() };

            if (getValidators().contains (tag))
            {
                const auto& tagValidators { getValidators().at (tag) };
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

void ConfigModel::saveToPath()
{
    jam::File::getOrCreateDirectory (
        Id::Files::Config::path.getParentDirectory(), Id::Files::Config::path.getFileName());

    for (auto& [key, value] : Id::FileConfig::get())
    {
        const auto name { Id::Files::Config::getName (key) };
        const juce::File file { Id::Files::Config::path.getChildFile (name) };

        if (not file.existsAsFile())
        {
            BinaryData::Raw raw (name);

            if (raw.exists())
                file.replaceWithData (raw.data, static_cast<size_t> (raw.size));
        }
    }

    theme.saveToPath (getValue (Id::toType (Id::display), Id::theme));
}

void ConfigModel::loadFromPath()
{
    juce::String errors;

    auto disk { jam::lua::fromLua (
        Id::toType (Id::config),
        Id::FileConfig::get(),
        [] (int key)
        {
            return Id::Files::Config::getPath (Id::Files::Config::getName (key)).loadFileAsString();
        },
        &getValidators(),
        &errors) };

    // Load dependent resources BEFORE overlay — setValuesFrom fires parameter
    // notifications and consumers must read fresh source at that point.
    auto diskDisplay { jam::Model::getChildWithName (disk, Id::toType (Id::display)) };
    theme.loadFromPath (diskDisplay.getProperty (Id::theme), errors);

    auto diskGraphics { jam::Model::getChildWithName (disk, Id::toType (Id::graphics)) };
    background.loadFromPath (diskGraphics.getProperty (Id::background), errors);
    postProcessing.loadFromPath (diskGraphics.getProperty (Id::postProcessing), errors);

    setValuesFrom (disk);

    const juce::String message { errors.isEmpty()
                                     ? getValue (Id::toType (Id::display), Id::successMessage).toString()
                                     : errors };
    appModel.setMessage (message);
}

void ConfigModel::startWatcher()
{
    watcher.addFolder (Id::Files::Config::path);
    watcher.coalesceEvents (coalesceMs);
    watcher.addListener (this);
}

void ConfigModel::fileChanged (const juce::File& file, jam::File::Watcher::Event event)
{
    if (event == jam::File::Watcher::Event::fileUpdated
        and file.hasFileExtension (Id::lua))
        loadFromPath();
}
