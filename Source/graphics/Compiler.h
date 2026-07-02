#pragma once
#include <JuceHeader.h>

/** @file Compiler.h
 *  @brief GLSL -> SPIR-V compile unit for END's Shadertoy-compatible shader
 *         passes.
 */

namespace graphics
{
/*____________________________________________________________________________*/
/** @brief Compiles a @c config::Shader state tree into a compiled
 *         @c jam::vulkan::Shader.
 *
 *  Static-only -- no instance state to carry between calls (the same
 *  @c compile() serves both the background component and the post-process
 *  handler). Wraps every present pass -- @c Common and @c Image special-cased,
 *  every other property on @p shaderState is a named buffer pass, compiled in
 *  lexicographic name order (@c config::Shader::loadFromPath, which enumerates
 *  every regular file in the shader project directory) -- with the engine-owned
 *  Shadertoy prelude (uniform/binding declarations matching
 *  @c jam::vulkan::ShaderUniforms and the set-0 GLSL contract documented in
 *  @c jam_VulkanShaderUniforms.h), compiles each assembled pass via
 *  @c shaderc::Compiler, and assembles the ordered @c jam::vulkan::Shader pass
 *  chain.
 *
 *  Opacity and resolution scale are NOT compile()'s concern -- neither is a
 *  @c jam::vulkan::Shader field (jam_VulkanShader.h's Shader doc comment);
 *  both are supplied by the caller at render/build time instead
 *  (jam::vulkan::render(), jam::vulkan::ShaderComponent::setShader()/setParams(),
 *  jam::vulkan::Registry::setPostProcess()/setPostProcessParams()). Only
 *  ImageResample is a compile()-time input -- baked directly into the generated
 *  sampler macros, never stored on the resulting Shader.
 *
 *  shaderc is used only inside @c Compiler.cpp -- no shaderc type appears in
 *  this header, so no shaderc symbol reaches any consumer (or JAM, which
 *  never links shaderc).
 */
struct Compiler
{
    /** @brief Compiles @p shaderState into a @c jam::vulkan::Shader.
     *
     *  @c shaderState is one @c config::Shader instance's @c state tree (of
     *  type @c IDtype::background or @c IDtype::postProcessing) -- a flat
     *  property set holding GLSL source per present pass, keyed by pass name
     *  (@c ID::common, @c ID::image, and every other property name = a named
     *  buffer pass -- @c config::Shader::loadFromPath's disk-enumeration
     *  contract). @c shaderState.getType() is this compile's sole mode signal
     *  (SSOT, no extra parameter) -- it decides the Image pass's generated
     *  opacity-mix formula, and whether the generated prelude exposes @c iScene
     *  at all (see @c assemblePass()'s doc comment). Buffer passes are wrapped
     *  and compiled in lexicographic name order; the @c ID::image pass is
     *  mandatory -- an empty @c ID::image source, or any present pass failing
     *  to compile (diagnostic logged via @c jam::debug::Log, shaderc error
     *  text + pass name), yields @c nullptr and no @c jam::vulkan::Shader is
     *  constructed. Callers keep their last-good Shader on @c nullptr.
     *
     *  @param shaderState  The shader instance's state tree.
     *  @param filter       Image resample mode for intermediate-pass upscaling --
     *                      selects, at generation time, which set-0 sampler
     *                      binding (@c linearSampler / @c nearestSampler) the
     *                      generated prelude's channel macros sample through.
     *  @return             The compiled Shader, or @c nullptr.
     */
    static std::unique_ptr<jam::vulkan::Shader> compile (const juce::ValueTree& shaderState,
                                                          jam::map::ImageResample::Type filter);

private:
    /** @brief Assembles one pass's full fragment source: the Shadertoy
     *  prelude (uniform/binding declarations, push-constant block --
     *  textually identical for every pass and both modes, so it always
     *  matches @c jam::vulkan::ShaderUniforms' C++ layout even when
     *  @c iScene is unused -- named sampler macros per buffer pass plus
     *  @c iChannel0-3 Shadertoy paste-compat aliases, and the @c iScene
     *  compatibility macro when @p isBackground is false), then
     *  @p commonSource (if present), then @p passSource, then a generated
     *  @c main(). Only @p isImagePass gets the mode-specific opacity mix
     *  documented on @c jam::vulkan::ShaderUniforms::opacity -- buffer
     *  passes write their @c mainImage() output raw regardless of mode.
     *  @param passSource       This pass's user GLSL source (its mainImage()).
     *  @param commonSource     The project's Common source, prepended before
     *                          @p passSource when present; empty otherwise.
     *  @param bufferPassNames  Every buffer pass name present on this Shader,
     *                          already in lexicographic (= ordinal) order --
     *                          channelMacros()'s SSOT for both the named
     *                          sampler macro per pass and the iChannel0-3
     *                          aliases.
     *  @param filter           Resample mode selecting which set-0 sampler
     *                          binding the generated channel macros sample
     *                          through (linear / nearest).
     *  @param isImagePass      True for the mandatory final Image pass -- the
     *                          only pass whose generated @c main() applies the
     *                          mode-specific opacity mix.
     *  @param isBackground     True for @c IDtype::background's tree shape
     *                          (no resolved scene to mix against -- the
     *                          component-transparency formula), false for
     *                          @c IDtype::postProcessing's (the effect-
     *                          intensity mix against the resolved scene).
     */
    static juce::String assemblePass (const juce::String& passSource, const juce::String& commonSource,
                                       const juce::StringArray& bufferPassNames, jam::map::ImageResample::Type filter,
                                       bool isImagePass, bool isBackground);

    /** @brief Returns one \#define line per entry in @p bufferPassNames,
     *  rebinding each pass's own name to a @c sampler2D expression indexing
     *  its ordinal's push-constant channels[] slot through @p filter's set-0
     *  sampler binding -- plus, for however many entries exist up to 4, an
     *  additional \#define iChannel0-3 alias to that SAME expression
     *  (Shadertoy paste-compat: pasted code referencing @c iChannel0-3
     *  directly keeps working regardless of what the buffer pass file was
     *  actually named).
     */
    static juce::String channelMacros (const juce::StringArray& bufferPassNames, jam::map::ImageResample::Type filter);

    /** @brief Returns the \#define iScene line rebinding the push-constant
     *  bindless index to a @c sampler2D expression through @p filter's set-0
     *  sampler binding -- same self-reference technique as @c channelMacros().
     *  Only ever appended by @c assemblePass() on the post-processing path
     *  (@c IDtype::postProcessing) -- the background path never appends it,
     *  so a background shader referencing @c iScene fails to compile with a
     *  clear diagnostic instead of silently sampling an invalid bindless index
     *  (see @c jam::vulkan::ShaderUniforms::iScene's doc comment).
     */
    static juce::String sceneMacro (jam::map::ImageResample::Type filter);

    /** @brief Compiles @p source (already assembled by @c assemblePass) to
     *  SPIR-V via shaderc, targeting Vulkan 1.0 with performance
     *  optimization. @p passName tags shaderc diagnostics and identifies
     *  the pass in the logged error line.
     *  @return  The compiled SPIR-V, or an empty @c MemoryBlock on failure
     *           (diagnostic logged via @c jam::debug::Log).
     */
    static juce::MemoryBlock compileSpirv (const juce::String& source, const juce::String& passName);
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace graphics
