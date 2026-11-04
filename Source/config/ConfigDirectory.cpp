#include "config/ConfigDirectory.h"

ConfigDirectory::ConfigDirectory (juce::ValueTree initialState)
    : jam::Model (std::move (initialState))
{
}

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
