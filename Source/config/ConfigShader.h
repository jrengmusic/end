#pragma once
#include "ConfigDirectory.h"

/**
    @brief Shader source model — @c ConfigDirectory subclass that holds a
           single shader tree (BACKGROUND or POST_PROCESSING) as its own @c state.

    The constructor adopts an empty @p treeType-rooted tree through
    @c ConfigDirectory's ValueTree ctor. The tree stays empty until @c loadFromPath
    reads the active shader project directory. An empty project is a valid
    initial state, not an error.

    After construction, @c ConfigModel appends each instance's @c state directly
    under the GRAPHICS child. The two instances are siblings under GRAPHICS.

    Load policy: @c saveToPath is a no-op (shader source is never seeded from
    BinaryData). @c loadFromPath reads GLSL from disk into @c state.

    @see ConfigDirectory
    @see ConfigModel
*/
class ConfigShader : public ConfigDirectory
{
public:
    /** @brief Constructs with an empty shader tree of the given type.
     *  @param treeType  Tree type identifier (jam::Format::toType (Id::background) or jam::Format::toType (Id::postProcessing)).
     */
    explicit ConfigShader (juce::Identifier treeType);

    /** @brief Defaulted. The owning ConfigModel destroys this object. */
    ~ConfigShader() override = default;

    /** @brief Reads the GLSL source of the active shader project into @c state.
     *
     *  Finds the shader directory through @c ConfigDirectory::Shaders::getPath.
     *  Reads the first file with the slang manifest extension in that directory,
     *  and parses it with @c jam::VulkanShaderPreset::parse(). The format is
     *  @c jam::VulkanShaderFormat::slang when the parsed preset has passes. An
     *  absent manifest, or one without passes, gives
     *  @c jam::VulkanShaderFormat::shadertoy. @c jam::VulkanShaderFormat::load()
     *  reads the directory in the resolved format. This method does not parse
     *  the sources itself.
     *
     *  Overlays the loaded tree on @c state with @c setValuesFrom. Stamps
     *  @c Id::shaderFormat with the resolved format, and @c Id::path with the
     *  full path of the directory, on @c state. Sends a property change for
     *  @c jam::Format::toType (Id::graphics).
     *
     *  @param path  Active shader project name from ConfigModel.
     *  @return      Always empty. This method reports no error.
     */
    juce::String loadFromPath (const juce::var& path) override;

private:
    //==========================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ConfigShader)
};
