#include "ConfigModel.h"

//==============================================================================
// ConfigShader
//==============================================================================

ConfigShader::ConfigShader (juce::Identifier treeType)
    : ConfigDirectory (juce::ValueTree (treeType))
{
}

juce::String ConfigShader::loadFromPath (const juce::var& path)
{
    const juce::File dir { ConfigDirectory::Shaders::getPath (path.toString()) };
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

    return {};
}

//==============================================================================
// addTables — shared parse/validate/gated-merge for a bimap-keyed markdown table set
//==============================================================================

static juce::String addTables (juce::ValueTree target, juce::Identifier rootTag,
                                const jam::Bimap<int>& files,
                                const std::function<juce::String (int)>& getName,
                                const std::function<juce::String (const juce::String&)>& read)
{
    juce::String errors;

    for (auto& [key, stem] : files.get())
    {
        const auto fileName { getName (key) };
        const auto document { jam::ConfigDocument::parse (read (fileName), fileName) };
        const auto result { jam::ConfigValidator::isValid (document) };

        if (result.wasOk())
        {
            auto fileTree { document.getValueTree (rootTag) };

            while (fileTree.getNumChildren() > 0)
                target.appendChild (fileTree.getChild (0), nullptr);
        }
        else
        {
            errors << result.getErrorMessage() << "\n";
        }
    }

    return errors;
}

//==============================================================================
// ConfigTheme
//==============================================================================

ConfigTheme::ConfigTheme()
    : ConfigDirectory ([]
      {
          juce::ValueTree themes { Id::toType (Id::themes) };
          const auto errors { addTables (themes, Id::toType (Id::themes),
              *map::FileThemes::getInstance(),
              ConfigDirectory::Themes::getName,
              [] (const juce::String& fileName) { return BinaryData::getString (fileName); }) };

          jassert (errors.isEmpty());
          juce::ignoreUnused (errors);

          return themes;
      }())
{
    auto flex { jam::Model::fromFiles (Id::toType (Id::flex),
                                       map::FileFlex::getInstance()->get(),
                                       [] (int key)
                                       {
                                           return BinaryData::getString (ConfigDirectory::Flex::getName (key));
                                       }) };

    state.appendChild (flex, nullptr);
}

void ConfigTheme::saveToPath (const juce::var& path)
{
    const juce::File dir { ConfigDirectory::Themes::getPath (path.toString()) };

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

        for (auto& [key, stem] : map::FileThemes::getInstance()->get())
            writeWhenNeeded (dir, ConfigDirectory::Themes::getName (key));

        auto flexDir { jam::File::getOrCreateDirectory (dir, Id::flex) };

        for (auto& [key, stem] : map::FileFlex::getInstance()->get())
            writeWhenNeeded (flexDir, ConfigDirectory::Flex::getName (key));
    }
}

juce::String ConfigTheme::loadFromPath (const juce::var& path)
{
    const juce::File dir { ConfigDirectory::Themes::getPath (path.toString()) };
    juce::String errors;

    if (dir.isDirectory())
    {
        juce::ValueTree disk { Id::toType (Id::themes) };

        errors << addTables (disk, Id::toType (Id::themes),
            *map::FileThemes::getInstance(),
            ConfigDirectory::Themes::getName,
            [dir] (const juce::String& fileName) { return dir.getChildFile (fileName).loadFileAsString(); });

        const juce::File flexDir { dir.getChildFile (Id::flex) };
        auto flexDisk { jam::Model::fromFiles (
            Id::toType (Id::flex),
            map::FileFlex::getInstance()->get(),
            [flexDir] (int key)
            {
                return flexDir.getChildFile (ConfigDirectory::Flex::getName (key)).loadFileAsString();
            }) };

        disk.appendChild (flexDisk, nullptr);

        setValuesFrom (disk);
    }

    state.sendPropertyChangeMessage (Id::theme);

    return errors;
}

//==============================================================================
// ConfigModel
//==============================================================================

ConfigModel::ConfigModel()
    : jam::Model ([]
      {
          juce::ValueTree config { Id::toType (Id::config) };
          const auto errors { addTables (config, Id::toType (Id::config),
              *map::FileConfig::getInstance(),
              ConfigDirectory::Config::getName,
              [] (const juce::String& fileName) { return BinaryData::getString (fileName); }) };

          jassert (errors.isEmpty());
          juce::ignoreUnused (errors);

          return config;
      }())
{
    // theme, background, and postProcessing members are now constructed — attach their subtrees.
    state.appendChild (theme.state, nullptr);

    auto graphics { jam::Model::getChildWithName (state, Id::toType (Id::graphics)) };
    graphics.appendChild (background.state, nullptr);
    graphics.appendChild (postProcessing.state, nullptr);

    saveToPath();
    loadFromPath();
    startWatcher();
}

void ConfigModel::saveToPath()
{
    jam::File::getOrCreateDirectory (
        ConfigDirectory::Config::path.getParentDirectory(), ConfigDirectory::Config::path.getFileName());

    for (auto& [key, stem] : map::FileConfig::getInstance()->get())
    {
        const auto name { ConfigDirectory::Config::getName (key) };
        const juce::File file { ConfigDirectory::Config::path.getChildFile (name) };

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
    juce::ValueTree disk { Id::toType (Id::config) };
    juce::String errors { addTables (disk, Id::toType (Id::config),
        *map::FileConfig::getInstance(),
        ConfigDirectory::Config::getName,
        [] (const juce::String& fileName) { return ConfigDirectory::Config::getPath (fileName).loadFileAsString(); }) };

    // Load dependent resources BEFORE overlay — setValuesFrom fires parameter
    // notifications and consumers must read fresh source at that point.
    auto diskDisplay { jam::Model::getChildWithName (disk, Id::toType (Id::display)) };
    errors << theme.loadFromPath (diskDisplay.getProperty (Id::theme));

    auto diskGraphics { jam::Model::getChildWithName (disk, Id::toType (Id::graphics)) };
    errors << background.loadFromPath (diskGraphics.getProperty (Id::background));
    errors << postProcessing.loadFromPath (diskGraphics.getProperty (Id::postProcessing));

    setValuesFrom (disk);

    const juce::String message { errors.isEmpty()
                                     ? getValue (Id::toType (Id::display), Id::successMessage).toString()
                                     : errors };
    appModel.setMessage (message);
}

void ConfigModel::startWatcher()
{
    watcher.addFolder (ConfigDirectory::Config::path);
    watcher.coalesceEvents (coalesceMs);
    watcher.addListener (this);
}

void ConfigModel::fileChanged (const juce::File& file, jam::File::Watcher::Event event)
{
    if (event == jam::File::Watcher::Event::fileUpdated
        and file.hasFileExtension (Extensions::md))
        loadFromPath();
}
