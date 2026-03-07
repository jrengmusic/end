/**
 * @file GLShaderCompiler.cpp
 * @brief Implementation of `GLShaderCompiler::compile()`.
 *
 * Implements the three-stage GLSL compilation pipeline:
 *
 * 1. **Vertex shader** — `juce::OpenGLShaderProgram::addVertexShader()`.
 * 2. **Fragment shader** — `juce::OpenGLShaderProgram::addFragmentShader()`.
 * 3. **Link** — `juce::OpenGLShaderProgram::link()`.
 *
 * Each stage is guarded by a positive check.  On failure at any stage, the
 * GLSL error log is written to `DBG()` and `nullptr` is returned.  On success,
 * ownership of the linked program is transferred to the caller.
 *
 * ### Error messages
 *
 * | Failure point        | DBG output                                  |
 * |----------------------|---------------------------------------------|
 * | Vertex compilation   | `"Vertex shader failed: <GLSL error log>"`  |
 * | Fragment compilation | `"Fragment shader failed: <GLSL error log>"`|
 * | Program link         | `"Shader link failed: <GLSL error log>"`    |
 *
 * @note This file must be compiled on the **GL THREAD** call path only.
 *       `compile()` requires an active OpenGL context.
 *
 * @see GLShaderCompiler.h
 * @see Terminal::Render::OpenGL::compileShaders()
 */
#include "GLShaderCompiler.h"

/**
 * @brief Compile and link a GLSL shader program from vertex and fragment sources.
 *
 * Creates a `juce::OpenGLShaderProgram` bound to @p context, then attempts
 * vertex compilation, fragment compilation, and linking in sequence.  Each
 * stage is attempted only if the previous one succeeded.
 *
 * @par Failure handling
 * On any failure, `juce::OpenGLShaderProgram::getLastError()` is retrieved
 * and written to `DBG()`.  The function returns `nullptr`; the partially
 * constructed shader object is destroyed automatically.
 *
 * @param context         Active OpenGL context current on the calling thread.
 * @param vertexSource    GLSL source for the vertex shader stage.
 * @param fragmentSource  GLSL source for the fragment shader stage.
 * @return                Owning `unique_ptr` to the linked program on success,
 *                        or `nullptr` on any compilation or link error.
 *
 * @note **GL THREAD** only.
 * @see GLShaderCompiler.h
 */
std::unique_ptr<juce::OpenGLShaderProgram> GLShaderCompiler::compile (
    juce::OpenGLContext& context,
    const juce::String& vertexSource,
    const juce::String& fragmentSource) noexcept
{
    auto shader { std::make_unique<juce::OpenGLShaderProgram> (context) };
    std::unique_ptr<juce::OpenGLShaderProgram> result;

    if (shader->addVertexShader (vertexSource))
    {
        if (shader->addFragmentShader (fragmentSource))
        {
            if (shader->link())
            {
                result = std::move (shader);
            }
            else
            {
                DBG ("Shader link failed: " + shader->getLastError());
            }
        }
        else
        {
            DBG ("Fragment shader failed: " + shader->getLastError());
        }
    }
    else
    {
        DBG ("Vertex shader failed: " + shader->getLastError());
    }

    return result;
}
