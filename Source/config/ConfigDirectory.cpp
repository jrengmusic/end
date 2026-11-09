#include "config/ConfigDirectory.h"

ConfigDirectory::ConfigDirectory (juce::ValueTree initialState)
    : jam::Model (std::move (initialState))
{
}

void ConfigDirectory::saveToPath (const juce::var&) {}

const juce::File ConfigDirectory::Config::path {
    juce::File::getSpecialLocation (juce::File::userHomeDirectory).getChildFile (configDirectoryName)
};

juce::String ConfigDirectory::Config::getName (int key) noexcept
{
    return jam::Format::toFileName (map::FileConfig::getInstance()->get (key), Extensions::md);
}

juce::File ConfigDirectory::Config::getPath (juce::StringRef child) noexcept
{
    return path.getChildFile (child);
}

juce::String ConfigDirectory::Themes::getName (int key) noexcept
{
    return jam::Format::toFileName (map::FileThemes::getInstance()->get (key), Extensions::md);
}

juce::File ConfigDirectory::Themes::getPath (const juce::String& themeName) noexcept
{
    return Config::getPath (Id::themes).getChildFile (themeName);
}

juce::String ConfigDirectory::Flex::getName (int key) noexcept
{
    return jam::Format::toFileName (map::FileFlex::getInstance()->get (key), Extensions::svg);
}

juce::File ConfigDirectory::Shaders::getPath (const juce::String& shadersName) noexcept
{
    return Config::getPath (Id::shaders).getChildFile (shadersName);
}

static void addTables (juce::ValueTree target, juce::ValueTree tree)
{
    while (tree.getNumChildren() > 0)
    {
        const auto table { tree.getChild (0) };

        tree.removeChild (0, nullptr);
        target.appendChild (table, nullptr);
    }
}

juce::String ConfigDirectory::addFileTables (juce::ValueTree target,
                                             const jam::Bimap<int>& files,
                                             const std::function<juce::String (int)>& getName,
                                             const std::function<juce::String (const juce::String&)>& read,
                                             const std::function<const jam::ConfigDocument& (const juce::String&)>& getDefaultDocument,
                                             const std::function<juce::ValueTree (const jam::ConfigDocument&)>& getTree)
{
    juce::String errors {};

    for (auto& [key, stem] : files.get())
    {
        const auto fileName { getName (key) };
        const auto& document { getDefaultDocument (fileName) };
        const auto readDocument { jam::ConfigDocument::parse (read (fileName), fileName) };
        jam::Document::Validator::Rules defaultRules {};
        jam::ConfigValidator::addRules (defaultRules, document);
        jam::ConfigValidator::addTableRules (defaultRules, document);

        const auto validation { jam::Document::Validator::isValid (jam::ConfigValidator {}.getRules(), readDocument) };
        const auto defaultValidation { validation.wasOk() ? jam::Document::Validator::isValid (defaultRules, readDocument) : validation };

        if (defaultValidation.wasOk())
            addTables (target, getTree (readDocument));
        else
            errors << defaultValidation.getErrorMessage() << juce::String::charToString (Chars::newline);
    }

    return errors;
}

void ConfigDirectory::addDefaultTables (juce::ValueTree target,
                                        const jam::Bimap<int>& files,
                                        const std::function<juce::String (int)>& getName,
                                        const std::function<const jam::ConfigDocument& (const juce::String&)>& getDefaultDocument,
                                        const std::function<juce::ValueTree (const jam::ConfigDocument&)>& getTree)
{
    for (auto& [key, stem] : files.get())
    {
        addTables (target, getTree (getDefaultDocument (getName (key))));
    }
}
