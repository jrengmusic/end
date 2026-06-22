#include "Config.h"

namespace config
{
/*____________________________________________________________________________*/

//==============================================================================
// Shader
//==============================================================================

Shader::Shader()
    : Directory (jam::Model::fromFiles (
          IDtype::shader, file::Shaders::get(), [] (int) { return juce::String(); }))
{
}

void Shader::loadFromPath() {}

//==============================================================================
// Theme
//==============================================================================

Theme::Theme()
    : Directory (jam::Model::fromLua (
          IDtype::themes, file::Themes::get(),
          [] (int key) { return BinaryData::getString (file::Themes::getName (key)); }))
{
    auto flex { jam::Model::fromFiles (
        IDtype::flex, file::Flex::get(),
        [] (int key) { return BinaryData::getString (file::Flex::getName (key)); }) };

    state.appendChild (flex, nullptr);
}

void Theme::saveToPath() {}

void Theme::loadFromPath() {}

//==============================================================================
// Model
//==============================================================================

Model::Model()
    : jam::Model (jam::Model::fromLua (
          IDtype::config, file::Config::get(),
          [] (int key) { return BinaryData::getString (file::Config::getName (key)); }))
{
    // theme and shader members are now constructed — attach their subtrees.
    state.appendChild (theme.state, nullptr);

    auto graphics { jam::Model::getChildWithName (state, IDtype::graphics) };
    graphics.appendChild (shader.state, nullptr);

    saveToPath();
    loadFromPath();
    startWatcher();
}

void Model::saveToPath() {}

void Model::loadFromPath() {}

void Model::startWatcher() {}

void Model::fileChanged (const juce::File& file, jam::File::Watcher::Event event)
{
    juce::ignoreUnused (file, event);
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace config
