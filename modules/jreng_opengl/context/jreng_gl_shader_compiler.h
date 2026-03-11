#pragma once

namespace jreng
{

/**
 * @namespace GLShaderCompiler
 * @brief Stateless utility for compiling OpenGL shader programs.
 *
 * Provides a factory function `compile()` that takes GLSL source
 * strings and returns a linked `juce::OpenGLShaderProgram` or `nullptr` on
 * failure.
 */
namespace GLShaderCompiler
{
    std::unique_ptr<juce::OpenGLShaderProgram> compile (
        juce::OpenGLContext& context,
        const juce::String& vertexSource,
        const juce::String& fragmentSource) noexcept;
}

} // namespace jreng
