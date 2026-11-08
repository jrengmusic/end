#include "ConfigShader.h"

ConfigShader::ConfigShader (juce::Identifier treeType)
    : ConfigDirectory (juce::ValueTree (treeType))
{
}

juce::String ConfigShader::loadFromPath (const juce::var& path)
{
    const juce::File dir { ConfigDirectory::Shaders::getPath (path.toString()) };
    const auto presetFiles (dir.findChildFiles (
        juce::File::findFiles,
        false,
        jam::VulkanShaderFormat::getExtension().at (jam::VulkanShaderFormat::slang)));
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
