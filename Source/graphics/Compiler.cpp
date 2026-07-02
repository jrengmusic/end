#include "Compiler.h"
#include "../Identifier.h"

#include <shaderc/shaderc.hpp>

namespace graphics
{
/*____________________________________________________________________________*/

juce::String Compiler::channelMacros (const juce::StringArray& bufferPassNames, jam::map::ImageResample::Type filter)
{
    static constexpr int shadertoyAliasCount { 4 };// iChannel0-3 paste-compat ceiling

    const juce::String sampler { filter == jam::map::ImageResample::Type::linear ? "linearSampler" : "nearestSampler" };
    juce::String macros;

    for (int ordinal = 0; ordinal < bufferPassNames.size(); ++ordinal)
    {
        const juce::String expression {
            "sampler2D(textures[nonuniformEXT(channels[" + juce::String (ordinal) + "])], " + sampler + ")"
        };

        macros << "#define " << bufferPassNames[ordinal] << " " << expression << "\n";

        if (ordinal < shadertoyAliasCount)
            macros << "#define iChannel" << ordinal << " " << expression << "\n";
    }

    return macros;
}

juce::String Compiler::sceneMacro (jam::map::ImageResample::Type filter)
{
    const juce::String sampler { filter == jam::map::ImageResample::Type::linear ? "linearSampler" : "nearestSampler" };

    return "#define iScene sampler2D(textures[nonuniformEXT(iScene)], " + sampler + ")\n";
}

juce::String Compiler::assemblePass (const juce::String& passSource, const juce::String& commonSource,
                                      const juce::StringArray& bufferPassNames, jam::map::ImageResample::Type filter,
                                      bool isImagePass, bool isBackground)
{
    // Prelude -- the push-constant block textually mirrors
    // jam::vulkan::ShaderUniforms (jam_VulkanShaderUniforms.h, member order
    // matches exactly, channels[] sized from maxChannelCount directly -- no
    // duplicated literal -- iScene included unconditionally so the block
    // matches the C++ layout even on the background path, where iScene is
    // stamped but never sampled); the set-0 binding declarations mirror that
    // same file's documented GLSL contract. GL_EXT_nonuniform_qualifier:
    // require and the sampler2D(textures[nonuniformEXT(index)], sampler)
    // indexing technique follow image.frag/tiled_image.frag precedent
    // (jam_vulkan/shaders). uv matches shader_pass.vert's shared varying.
    juce::String source;

    source << "#version 450\n"
              "#extension GL_EXT_nonuniform_qualifier : require\n"
              "\n"
              "layout(set = 0, binding = 0) uniform texture2D textures[];\n"
              "layout(set = 0, binding = 1) uniform sampler linearSampler;\n"
              "layout(set = 0, binding = 2) uniform sampler nearestSampler;\n"
              "\n"
              "layout(push_constant) uniform ShaderPC\n"
              "{\n"
              "    vec4  iMouse;\n"
              "    vec2  iResolution;\n"
              "    float iTime;\n"
              "    float iTimeDelta;\n"
              "    int   iFrame;\n"
              "    int   channels[" << jam::vulkan::ShaderUniforms::maxChannelCount << "];\n"
              "    int   iScene;\n"
              "    float opacity;\n"
              "};\n"
              "\n"
              "layout(location = 0) in vec2 uv;\n"
              "layout(location = 0) out vec4 fragColor;\n"
              "\n"
           << channelMacros (bufferPassNames, filter)
           << (isBackground ? juce::String() : sceneMacro (filter))
           << "\n"
           << commonSource
           << "\n"
           << passSource
           << "\n";

    // Generated main() -- the channel macros above rebind each push-constant
    // channels[] slot (declared by name in the block above, before these
    // macros exist) to a sampler2D expression; self-reference inside each
    // macro's own replacement list resolves to that raw array index, never
    // re-expanding the macro, so the push-constant member keeps the exact
    // name jam::vulkan::ShaderUniforms documents. Only the Image pass mixes
    // -- and the two modes mix DIFFERENT things, per
    // jam::vulkan::ShaderUniforms::opacity's documented per-mode contract:
    // post-process mixes the user shader's own output against the resolved
    // scene (sampled through iScene, stamped by
    // Graphics::recordPostProcessCompositeDrawCommands() with its own
    // resolved-scene bindless index -- never channels[0], which keeps its
    // ordinary buffer-pass-or-scene-fallback meaning); background has no
    // scene to mix against and instead scales the user shader's own alpha by
    // opacity. Buffer passes write mainImage()'s raw output in both modes.
    source << (isImagePass
        ? (isBackground
            ? "void main()\n"
              "{\n"
              "    vec4 userColor;\n"
              "    mainImage (userColor, uv * iResolution);\n"
              "    fragColor = vec4 (userColor.rgb, userColor.a * opacity);\n"
              "}\n"
            : "void main()\n"
              "{\n"
              "    vec4 userColor;\n"
              "    mainImage (userColor, uv * iResolution);\n"
              "    fragColor = mix (texture (iScene, uv), userColor, opacity);\n"
              "}\n")
        : "void main()\n"
          "{\n"
          "    mainImage (fragColor, uv * iResolution);\n"
          "}\n");

    return source;
}

juce::MemoryBlock Compiler::compileSpirv (const juce::String& source, const juce::String& passName)
{
    shaderc::Compiler compiler;
    shaderc::CompileOptions options;

    options.SetTargetEnvironment (shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_0);
    options.SetOptimizationLevel (shaderc_optimization_level_performance);

    const auto result { compiler.CompileGlslToSpv (source.toRawUTF8(), source.getNumBytesAsUTF8(),
                                                    shaderc_glsl_fragment_shader,
                                                    passName.toRawUTF8(), options) };

    juce::MemoryBlock spirv;

    if (result.GetCompilationStatus() == shaderc_compilation_status_success)
        spirv.append (result.cbegin(), static_cast<size_t> (result.cend() - result.cbegin()) * sizeof (uint32_t));
    else
        jam::debug::Log::write ("graphics::Compiler: " + passName + " -- " + result.GetErrorMessage());

    return spirv;
}

std::unique_ptr<jam::vulkan::Shader> Compiler::compile (const juce::ValueTree& shaderState, jam::map::ImageResample::Type filter)
{
    const bool isBackground { shaderState.getType() == IDtype::background };
    const juce::String commonSource { shaderState.getProperty (ID::common).toString() };
    const juce::String imageSource { shaderState.getProperty (ID::image).toString() };

    std::unique_ptr<jam::vulkan::Shader> shader;

    if (imageSource.isNotEmpty())
    {
        // Every property but common/image is a named buffer pass
        // (config::Shader::loadFromPath's disk-enumeration contract) --
        // sorted lexicographically (SSOT ordinal order, jam_VulkanShader.h's
        // Shader doc comment), independent of ValueTree property iteration order.
        juce::StringArray bufferPassNames;

        for (int i = 0; i < shaderState.getNumProperties(); ++i)
        {
            const auto propertyName { shaderState.getPropertyName (i) };

            if (propertyName != ID::common and propertyName != ID::image)
                bufferPassNames.add (propertyName.toString());
        }

        bufferPassNames.sort (false);

        jam::Owner<jam::vulkan::Shader::Pass> passes;
        bool everyPassCompiled { true };

        for (auto& bufferPassName : bufferPassNames)
        {
            const juce::String bufferSource {
                shaderState.getProperty (juce::Identifier (bufferPassName)).toString()
            };

            auto spirv { compileSpirv (
                assemblePass (bufferSource, commonSource, bufferPassNames, filter, false, isBackground),
                bufferPassName) };

            everyPassCompiled = everyPassCompiled and not spirv.isEmpty();
            passes.add (std::make_unique<jam::vulkan::Shader::Pass> (
                jam::vulkan::Shader::Pass { bufferPassName, std::move (spirv) }));
        }

        auto imageSpirv { compileSpirv (
            assemblePass (imageSource, commonSource, bufferPassNames, filter, true, isBackground),
            IDref::image) };

        everyPassCompiled = everyPassCompiled and not imageSpirv.isEmpty();
        passes.add (std::make_unique<jam::vulkan::Shader::Pass> (
            jam::vulkan::Shader::Pass { IDref::image, std::move (imageSpirv) }));

        if (everyPassCompiled)
            shader = std::make_unique<jam::vulkan::Shader> (std::move (passes));
    }

    return shader;
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace graphics
