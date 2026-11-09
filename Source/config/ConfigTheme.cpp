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

static juce::ValueTree getStyleTree (const jam::ConfigDocument& document)
{
    return jam::CssDocument::parse (jam::CssWriter {}.getText (document)).getValueTree (jam::Format::toType (Id::themes),
                                                                                        jam::CssDocument::getSheetType (files::themeStyleSheet));
}

static juce::String getDefaultText (const juce::String& fileName)
{
    return BinaryData::getString (fileName)
           + juce::String::charToString (Chars::newline)
           + jam::MarkdownWriter {}.getText (jam::ConfigDocument::toDocument (jam::CssDocument::getOrCreate (juce::Identifier { files::themeStyleSheet })));
}

static const jam::ConfigDocument& getDefaultDocument (const juce::String& fileName)
{
    return jam::ConfigDocument::getOrCreate (juce::Identifier { fileName }, getDefaultText (fileName));
}

static juce::ValueTree getDefaultThemes()
{
    juce::ValueTree themes { jam::Format::toType (Id::themes) };

    ConfigDirectory::addDefaultTables (themes, *map::FileThemes::getInstance(), ConfigDirectory::Themes::getName, getDefaultDocument, getStyleTree);

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

        jam::ConfigDocument::getOrCreateDefaultFile (dir.getChildFile (fileName), getDefaultText (fileName));
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
            [dir] (const juce::String& fileName) { return dir.getChildFile (fileName).loadFileAsString(); },
            getDefaultDocument,
            getStyleTree);

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
