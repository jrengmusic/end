#include "ConfigTheme.h"

static juce::String addFileFlex (juce::ValueTree target, const juce::File& flexDir)
{
    juce::String errors {};

    target.appendChild (jam::Model::fromFiles (
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

    return errors;
}

static const juce::String& getOrCreateDefaultText (const juce::String& fileName)
{
    static jam::HashMap<juce::String, juce::String> defaultTexts;

    if (defaultTexts.contains (fileName))
        return defaultTexts.at (fileName);

    jam::Strings styleSheets;
    styleSheets.add (files::themeStyleSheet);

    defaultTexts.addOrReplace (fileName,
                               BinaryData::getString (fileName)
                                   + juce::String::charToString (Chars::newline)
                                   + jam::MarkdownWriter {}.getText (jam::ConfigDocument::toDocument (jam::Css::getValueTree (jam::Format::toType (Id::settings), styleSheets))));

    return defaultTexts.at (fileName);
}

static juce::ValueTree getDefaultThemes()
{
    juce::ValueTree themes { jam::Format::toType (Id::themes) };

    for (auto& [key, stem] : map::FileThemes::getInstance()->get())
    {
        const auto fileName { ConfigDirectory::Themes::getName (key) };

        jam::ConfigDocument::getOrCreate (juce::Identifier { fileName }, getOrCreateDefaultText (fileName));
    }

    ConfigDirectory::addDefaultTables (themes, *map::FileThemes::getInstance(), ConfigDirectory::Themes::getName);

    themes.appendChild (jam::Model::fromFiles (jam::Format::toType (Id::flex),
                                               map::FileFlex::getInstance()->get(),
                                               [] (int key)
                                               {
                                                   return BinaryData::getString (ConfigDirectory::Flex::getName (key));
                                               }),
                        nullptr);

    return themes;
}

ConfigTheme::ConfigTheme()
    : ConfigDirectory (getDefaultThemes())
{
}

void ConfigTheme::saveToPath (const juce::var& path)
{
    const juce::File dir { ConfigDirectory::Themes::getPath (path.toString()) };

    jam::File::getOrCreateDirectory (dir.getParentDirectory(), dir.getFileName());

    for (auto& [key, stem] : map::FileThemes::getInstance()->get())
    {
        const auto fileName { ConfigDirectory::Themes::getName (key) };

        jam::ConfigDocument::getOrCreateDefaultFile (dir.getChildFile (fileName), getOrCreateDefaultText (fileName));
    }

    auto flexDir { jam::File::getOrCreateDirectory (dir, Id::flex) };

    for (auto& [key, stem] : map::FileFlex::getInstance()->get())
    {
        const auto fileName { ConfigDirectory::Flex::getName (key) };
        const auto file { flexDir.getChildFile (fileName) };

        if (not file.existsAsFile())
            file.replaceWithText (BinaryData::getString (fileName));
    }
}

juce::String ConfigTheme::loadFromPath (const juce::var& path)
{
    const juce::File dir { ConfigDirectory::Themes::getPath (path.toString()) };
    auto themes { getDefaultThemes() };
    juce::String errors {};

    if (dir.isDirectory())
    {
        juce::ValueTree disk { jam::Format::toType (Id::themes) };

        errors << ConfigDirectory::addFileTables (disk,
            *map::FileThemes::getInstance(),
            ConfigDirectory::Themes::getName,
            [dir] (const juce::String& fileName) { return dir.getChildFile (fileName).loadFileAsString(); });

        errors << addFileFlex (disk, dir.getChildFile (Id::flex));

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
