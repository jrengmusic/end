/**
 * @file GLShaderCompiler.h
 * @brief Utility for compiling and linking OpenGL shader programs.
 *
 * `GLShaderCompiler` is a stateless utility namespace that wraps the
 * `juce::OpenGLShaderProgram` compilation pipeline into a single, safe
 * factory function.  It handles the three-stage process — vertex shader
 * compilation, fragment shader compilation, and program linking — and logs
 * diagnostic messages via `DBG()` on any failure.
 *
 * ### Usage
 *
 * @code
 * // GL THREAD — called from Render::OpenGL::compileShaders()
 * auto* ctx = juce::OpenGLContext::getCurrentContext();
 * if (ctx != nullptr)
 * {
 *     auto prog = GLShaderCompiler::compile (*ctx, vertSrc, fragSrc);
 *     if (prog != nullptr)
 *         prog->use();
 * }
 * @endcode
 *
 * ### Error handling
 *
 * On any compilation or link failure, `compile()` returns `nullptr` and
 * writes the GLSL error log to `DBG()`.  The caller should treat a `nullptr`
 * return as a fatal render-setup error and skip rendering until the context
 * is recreated.
 *
 * @note All functions in this namespace must be called on the **GL THREAD**.
 *
 * @see Terminal::Render::OpenGL::compileShaders()
 * @see GLVertexLayout
 */
#pragma once
#include <JuceHeader.h>

/**
 * @namespace GLShaderCompiler
 * @brief Stateless utility for compiling OpenGL shader programs.
 *
 * Contains a single factory function `compile()` that takes GLSL source
 * strings and returns a linked `juce::OpenGLShaderProgram` or `nullptr` on
 * failure.
 *
 * @note **GL THREAD** only — all functions require an active OpenGL context.
 *
 * @see compile()
 */
// Utility for compiling OpenGL shader programs
// GL THREAD ONLY
namespace GLShaderCompiler
{
    /**
     * @brief Compile and link a GLSL shader program from vertex and fragment sources.
     *
     * Performs the full three-stage pipeline:
     * 1. Compiles @p vertexSource as a vertex shader via
     *    `juce::OpenGLShaderProgram::addVertexShader()`.
     * 2. Compiles @p fragmentSource as a fragment shader via
     *    `juce::OpenGLShaderProgram::addFragmentShader()`.
     * 3. Links the program via `juce::OpenGLShaderProgram::link()`.
     *
     * On success, returns the linked program.  On any failure, logs the GLSL
     * error string via `DBG()` and returns `nullptr`.
     *
     * @param context         Active OpenGL context.  Must be the context current
     *                        on the calling thread.
     * @param vertexSource    GLSL source code for the vertex shader.
     * @param fragmentSource  GLSL source code for the fragment shader.
     * @return                Owning pointer to the linked shader program, or
     *                        `nullptr` if compilation or linking failed.
     *
     * @note **GL THREAD** only.
     * @see Terminal::Render::OpenGL::compileShaders()
     */
    // Compile a shader program from vertex and fragment sources
    // Returns nullptr if compilation fails
    // On failure, error message is logged via DBG()
    std::unique_ptr<juce::OpenGLShaderProgram> compile (
        juce::OpenGLContext& context,
        const juce::String& vertexSource,
        const juce::String& fragmentSource) noexcept;
}
