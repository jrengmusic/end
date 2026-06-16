/**
 * @file shader/Compiler.h
 * @brief Static shader program builder.
 */
#pragma once
#include <JuceHeader.h>

namespace shader
{
/*____________________________________________________________________________*/

/** @brief Static shader program builder — compiles, links, and binds uniform setters.
 *
 *  Pure static utility. No instance state.
 *
 *  build() compiles and links a shader program from vertex + fragment sources.
 *  buildUniformSetter() discovers active uniforms from a linked program and
 *  returns a single std::function that sets all of them in one call.
 *
 *  Thread contract: ALL methods GL THREAD only.
 */
struct Compiler
{
    /** @brief Uniform setter signature — width, height, time, timeDelta, frame. */
    using UniformSetter = std::function<void (float, float, float, float, int)>;

    /** @brief Compiles and links a shader program.
     *  @param vertexSource   Vertex shader source.
     *  @param fragmentSource Fragment shader source (single compilation unit).
     *  @param context        Active OpenGL context.
     *  @param error          Receives error message on failure.
     *  @return               Linked program, or nullptr on failure.
     */
    static std::unique_ptr<juce::OpenGLShaderProgram> build (
        const juce::String& vertexSource,
        const juce::String& fragmentSource,
        juce::OpenGLContext& context,
        juce::String& error);

    /** @brief Discovers active uniforms and returns a baked setter function.
     *
     *  Queries glGetActiveUniform for all active uniforms. For each known
     *  Shadertoy uniform, captures its location and the matching glUniform*
     *  call. Returns a single function that sets all discovered uniforms
     *  from the five frame parameters (width, height, time, timeDelta, frame).
     *
     *  Unknown uniforms (user-defined) are silently skipped — they have no
     *  value source in the frame parameters.
     *
     *  @param program  Linked shader program to query.
     *  @return         Baked setter function. Callable every frame.
     */
    static UniformSetter buildUniformSetter (const juce::OpenGLShaderProgram& program);
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace shader
