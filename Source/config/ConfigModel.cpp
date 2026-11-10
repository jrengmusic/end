#include "ConfigModel.h"

static juce::ValueTree getConfigTree (const jam::ConfigDocument& document)
{
    return document.getValueTree (jam::Format::toType (Id::config));
}

static const jam::ConfigDocument& getDefaultDocument (const juce::String& fileName)
{
    return jam::ConfigDocument::getOrCreate (juce::Identifier { fileName });
}

static juce::ValueTree getDefaultConfig()
{
    juce::ValueTree config { jam::Format::toType (Id::config) };
    ConfigDirectory::addDefaultTables (config, *map::FileConfig::getInstance(), ConfigDirectory::Config::getName,
                                      getDefaultDocument, getConfigTree);

    return config;
}

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
    const auto row { getChildWithName (type).getChildWithName (jam::Format::toType (name)) };
    const auto appearanceText { jam::Model::getValue<juce::var> (getChildWithName (jam::Format::toType (Id::display)),
                                                                  jam::Format::toType (Id::appearance)).toString() };

    return row.hasProperty (Id::dark) and jam::StyleManager::isDark (appearanceText)
               ? row.getProperty (Id::dark)
               : row.getProperty (Id::value);
}

void ConfigModel::saveToPath()
{
    jam::File::getOrCreateDirectory (
        ConfigDirectory::Config::path.getParentDirectory(), ConfigDirectory::Config::path.getFileName());

    for (auto& [key, stem] : map::FileConfig::getInstance()->get())
    {
        const auto fileName { ConfigDirectory::Config::getName (key) };

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
    juce::String errors { ConfigDirectory::addFileTables (disk,
        *map::FileConfig::getInstance(),
        ConfigDirectory::Config::getName,
        [] (const juce::String& fileName) { return ConfigDirectory::Config::getPath (fileName).loadFileAsString(); },
        getDefaultDocument,
        getConfigTree) };

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

juce::Identifier ConfigModel::getEventKey (const Events& events,
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
