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
// addFileTables — per-file loop over a bimap-keyed markdown table set
//==============================================================================

static juce::String addFileTables (juce::ValueTree target,
                                   const jam::Bimap<int>& files,
                                   const std::function<juce::String (int)>& getName,
                                   const std::function<juce::String (const juce::String&)>& read)
{
    juce::String errors;

    for (auto& [key, stem] : files.get())
    {
        const auto fileName { getName (key) };
        const auto defaultDocument { jam::ConfigDocument::parse (BinaryData::getString (fileName), fileName) };
        const auto readDocument { jam::ConfigDocument::parse (read (fileName), fileName) };
        const auto failures { jam::ConfigDocument::addTables (target, defaultDocument, readDocument,
                                                              jam::ConfigValidator {}.getRules()) };

        if (failures.isNotEmpty())
            errors << failures << juce::String::charToString (Chars::newline);
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
          const auto errors { addFileTables (themes,
              *map::FileThemes::getInstance(),
              ConfigDirectory::Themes::getName,
              [] (const juce::String& fileName) { return BinaryData::getString (fileName); }) };

          jassert (errors.isEmpty());

          if (errors.isNotEmpty())
              ENDModel::getInstance()->setMessage (errors);

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
        jam::File::getOrCreateDirectory (dir.getParentDirectory(), dir.getFileName());

        for (auto& [key, stem] : map::FileThemes::getInstance()->get())
        {
            const auto fileName { ConfigDirectory::Themes::getName (key) };

            if (BinaryData::Raw (fileName).exists())
                jam::ConfigDocument::getOrCreateDefaultFile (dir.getChildFile (fileName), BinaryData::getString (fileName));
        }

        auto flexDir { jam::File::getOrCreateDirectory (dir, Id::flex) };

        for (auto& [key, stem] : map::FileFlex::getInstance()->get())
        {
            const auto fileName { ConfigDirectory::Flex::getName (key) };
            const auto file { flexDir.getChildFile (fileName) };

            if (BinaryData::Raw (fileName).exists() and not file.existsAsFile())
                file.replaceWithText (BinaryData::getString (fileName));
        }
    }
}

juce::String ConfigTheme::loadFromPath (const juce::var& path)
{
    const juce::File dir { ConfigDirectory::Themes::getPath (path.toString()) };
    juce::String errors;

    if (dir.isDirectory())
    {
        juce::ValueTree disk { Id::toType (Id::themes) };

        errors << addFileTables (disk,
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
    else
    {
        errors << dir.getFullPathName() + Id::diagnosticSeparator + text::English::failMissingDirectory + path.toString()
               << juce::String::charToString (Chars::newline);
    }

    state.sendPropertyChangeMessage (Id::toType (Id::theme));

    return errors;
}

//==============================================================================
// ConfigModel
//==============================================================================

ConfigModel::ConfigModel()
    : jam::Model ([]
      {
          juce::ValueTree config { Id::toType (Id::config) };
          const auto errors { addFileTables (config,
              *map::FileConfig::getInstance(),
              ConfigDirectory::Config::getName,
              [] (const juce::String& fileName) { return BinaryData::getString (fileName); }) };

          jassert (errors.isEmpty());

          if (errors.isNotEmpty())
              ENDModel::getInstance()->setMessage (errors);

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

juce::var ConfigModel::getRowValue (const juce::Identifier& type, const juce::Identifier& name) const noexcept
{
    const auto table { getChildWithName (type) };

    const auto row { table.getChildWithName (Id::toType (name)) };

    jassert (row.isValid());

    const auto appearanceText { jam::Model::getValue<juce::var> (getChildWithName (Id::toType (Id::display)),
                                                                  Id::toType (Id::appearance)).toString() };

    return row.hasProperty (Id::dark) and jam::StyleManager::isDark (appearanceText)
               ? row.getProperty (Id::dark)
               : jam::Model::getValue<juce::var> (table, Id::toType (name));
}

void ConfigModel::saveToPath()
{
    jam::File::getOrCreateDirectory (
        ConfigDirectory::Config::path.getParentDirectory(), ConfigDirectory::Config::path.getFileName());

    for (auto& [key, stem] : map::FileConfig::getInstance()->get())
    {
        const auto fileName { ConfigDirectory::Config::getName (key) };

        if (BinaryData::Raw (fileName).exists())
            jam::ConfigDocument::getOrCreateDefaultFile (ConfigDirectory::Config::path.getChildFile (fileName),
                                                         BinaryData::getString (fileName));
    }

    theme.saveToPath (getRowValue (Id::toType (Id::display), Id::theme));
}

void ConfigModel::loadFromPath()
{
    juce::ValueTree disk { Id::toType (Id::config) };
    juce::String errors { addFileTables (disk,
        *map::FileConfig::getInstance(),
        ConfigDirectory::Config::getName,
        [] (const juce::String& fileName) { return ConfigDirectory::Config::getPath (fileName).loadFileAsString(); }) };

    // Load dependent resources BEFORE overlay — setValuesFrom fires parameter
    // notifications and consumers must read fresh source at that point.
    const auto getMergedValue { [this, &disk] (const juce::Identifier& tableType, const juce::Identifier& rowType)
    {
        const auto diskTable { jam::Model::getChildWithName (disk, tableType) };
        const auto sourceTable { diskTable.getChildWithName (rowType).isValid() ? diskTable
                                                                               : jam::Model::getChildWithName (state, tableType) };

        return jam::Model::getValue<juce::var> (sourceTable, rowType);
    } };

    errors << theme.loadFromPath (getMergedValue (Id::toType (Id::display), Id::toType (Id::theme)));
    errors << background.loadFromPath (getMergedValue (Id::toType (Id::graphics), Id::toType (Id::background)));
    errors << postProcessing.loadFromPath (getMergedValue (Id::toType (Id::graphics), Id::toType (Id::postProcessing)));

    setValuesFrom (disk);

    const juce::String message { errors.isEmpty()
                                     ? getRowValue (Id::toType (Id::display), Id::successMessage).toString()
                                     : errors };
    appModel.setMessage (message);
}

juce::Identifier ConfigModel::getEventKey (const jam::Function::Map<juce::Identifier, void>& events,
                                           const juce::ValueTree& tree,
                                           const juce::Identifier& property)
{
    const juce::Identifier typeKey { events.contains (property) ? property : tree.getType() };

    if (events.contains (typeKey)) return typeKey;
    return tree.getParent().getType();
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
