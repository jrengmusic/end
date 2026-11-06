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

    state.sendPropertyChangeMessage (jam::Format::toType (Id::graphics));

    return {};
}

static juce::String addFileTables (juce::ValueTree target,
                                   const jam::Bimap<int>& files,
                                   const std::function<juce::String (int)>& getName,
                                   const std::function<juce::String (const juce::String&)>& read)
{
    juce::String errors;

    for (auto& [key, stem] : files.get())
    {
        const auto fileName { getName (key) };
        const auto& defaultDocument { jam::ConfigDocument::getOrCreate (juce::Identifier { fileName }) };
        const auto readDocument { jam::ConfigDocument::parse (read (fileName), fileName) };
        const auto failures { jam::ConfigDocument::addTables (target, defaultDocument, readDocument,
                                                              jam::ConfigValidator {}.getRules()) };

        if (failures.isNotEmpty())
            errors << failures << juce::String::charToString (Chars::newline);
    }

    return errors;
}

static juce::String addDefaultTables (juce::ValueTree target,
                                      const jam::Bimap<int>& files,
                                      const std::function<juce::String (int)>& getName)
{
    juce::String errors;

    for (auto& [key, stem] : files.get())
    {
        const auto fileName { getName (key) };
        const auto& document { jam::ConfigDocument::getOrCreate (juce::Identifier { fileName }) };
        const auto failures { jam::ConfigDocument::addTables (target, document, document,
                                                              jam::ConfigValidator {}.getRules()) };

        if (failures.isNotEmpty())
            errors << failures << juce::String::charToString (Chars::newline);
    }

    return errors;
}

static juce::ValueTree getDefaultThemes()
{
    juce::ValueTree themes { jam::Format::toType (Id::themes) };
    const auto errors { addDefaultTables (themes, *map::FileThemes::getInstance(), ConfigDirectory::Themes::getName) };

    jassert (errors.isEmpty());

    if (errors.isNotEmpty())
        ENDModel::getInstance()->setMessage (errors);

    themes.appendChild (jam::Model::fromFiles (jam::Format::toType (Id::flex),
                                               map::FileFlex::getInstance()->get(),
                                               [] (int key)
                                               {
                                                   return BinaryData::getString (ConfigDirectory::Flex::getName (key));
                                               }),
                        nullptr);

    return themes;
}

static juce::ValueTree getDefaultConfig()
{
    juce::ValueTree config { jam::Format::toType (Id::config) };
    const auto errors { addDefaultTables (config, *map::FileConfig::getInstance(), ConfigDirectory::Config::getName) };

    jassert (errors.isEmpty());

    if (errors.isNotEmpty())
        ENDModel::getInstance()->setMessage (errors);

    return config;
}

//==============================================================================
// ConfigTheme
//==============================================================================

ConfigTheme::ConfigTheme()
    : ConfigDirectory (getDefaultThemes())
{
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
    auto themes { getDefaultThemes() };
    juce::String errors;

    if (dir.isDirectory())
    {
        juce::ValueTree disk { jam::Format::toType (Id::themes) };

        errors << addFileTables (disk,
            *map::FileThemes::getInstance(),
            ConfigDirectory::Themes::getName,
            [dir] (const juce::String& fileName) { return dir.getChildFile (fileName).loadFileAsString(); });

        const juce::File flexDir { dir.getChildFile (Id::flex) };

        disk.appendChild (jam::Model::fromFiles (
            jam::Format::toType (Id::flex),
            map::FileFlex::getInstance()->get(),
            [flexDir, &errors] (int key) -> juce::String
            {
                const auto file { flexDir.getChildFile (ConfigDirectory::Flex::getName (key)) };
                const auto text { file.loadFileAsString() };
                const auto validation { jam::XmlValidator {}.isValid (jam::XmlDocument::parse (text)) };

                if (validation.wasOk())
                    return text;

                errors << file.getFullPathName() + Id::diagnosticSeparator + validation.getErrorMessage()
                       << juce::String::charToString (Chars::newline);

                return BinaryData::getString (ConfigDirectory::Flex::getName (key));
            }),
            nullptr);

        setValuesFrom (themes, disk);
    }
    else
    {
        errors << dir.getFullPathName() + Id::diagnosticSeparator + text::English::failMissingDirectory + path.toString()
               << juce::String::charToString (Chars::newline);
    }

    setValuesFrom (themes);
    state.sendPropertyChangeMessage (jam::Format::toType (Id::theme));

    return errors;
}

//==============================================================================
// ConfigModel
//==============================================================================

ConfigModel::ConfigModel()
    : jam::Model (getDefaultConfig())
{
    state.appendChild (theme.state, nullptr);

    auto graphics { jam::Model::getChildWithName (state, jam::Format::toType (Id::graphics)) };
    graphics.appendChild (background.state, nullptr);
    graphics.appendChild (postProcessing.state, nullptr);

    loadFromPath();
    startWatcher();
}

juce::var ConfigModel::getRowValue (const juce::Identifier& type, const juce::Identifier& name) const noexcept
{
    const auto table { getChildWithName (type) };

    const auto row { table.getChildWithName (jam::Format::toType (name)) };

    jassert (row.isValid());

    const auto appearanceText { jam::Model::getValue<juce::var> (getChildWithName (jam::Format::toType (Id::display)),
                                                                  jam::Format::toType (Id::appearance)).toString() };

    return row.hasProperty (Id::dark) and jam::StyleManager::isDark (appearanceText)
               ? row.getProperty (Id::dark)
               : jam::Model::getValue<juce::var> (table, jam::Format::toType (name));
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
}

juce::String ConfigModel::loadResources (const juce::ValueTree& config)
{
    const auto getConfigValue { [&config] (const juce::Identifier& tableType, const juce::Identifier& rowType)
    {
        return jam::Model::getValue<juce::var> (jam::Model::getChildWithName (config, tableType), rowType);
    } };

    const auto themeName { getConfigValue (jam::Format::toType (Id::display), jam::Format::toType (Id::theme)) };

    theme.saveToPath (themeName);

    juce::String errors { theme.loadFromPath (themeName) };
    errors << background.loadFromPath (getConfigValue (jam::Format::toType (Id::graphics), jam::Format::toType (Id::background)));
    errors << postProcessing.loadFromPath (getConfigValue (jam::Format::toType (Id::graphics), jam::Format::toType (Id::postProcessing)));

    return errors;
}

void ConfigModel::loadFromPath()
{
    saveToPath();

    juce::ValueTree disk { jam::Format::toType (Id::config) };
    juce::String errors { addFileTables (disk,
        *map::FileConfig::getInstance(),
        ConfigDirectory::Config::getName,
        [] (const juce::String& fileName) { return ConfigDirectory::Config::getPath (fileName).loadFileAsString(); }) };

    auto config { getDefaultConfig() };

    setValuesFrom (config, disk);

    // Load dependent resources BEFORE overlay — setValuesFrom fires parameter
    // notifications and consumers must read fresh source at that point.
    errors << loadResources (config);

    setValuesFrom (config);

    const juce::String message { errors.isEmpty()
                                     ? getRowValue (jam::Format::toType (Id::display), Id::successMessage).toString()
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

void ConfigModel::fileChanged (const juce::File& file, jam::File::Watcher::Event)
{
    if (file.hasFileExtension (Extensions::md) or file.hasFileExtension (Extensions::svg))
        loadFromPath();
}
