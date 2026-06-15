/**
 * @file shader/Compiler.h
 * @brief Static shader program builder — compiles and links OpenGL shader programs.
 */
#pragma once
#include <JuceHeader.h>
#include "end/Map.h"

namespace shader
{
/*____________________________________________________________________________*/

/** @brief Static shader program builder — compiles and links OpenGL shader programs.
 *
 *  Pure static utility following the jam::view::Manager pattern. No instance state.
 *
 *  build() takes fragment source strings, loads quad.vert from BinaryData via
 *  config::File::Shaders, compiles, links, returns the program. Caller owns the result.
 *
 *  Thread contract: ALL methods GL THREAD only.
 */
struct Compiler
{
    /** @brief Compiles and links a shader program from fragment sources.
     *
     *  Loads quad.vert from BinaryData via config::File::Shaders::getName().
     *  Adds each fragment source via addFragmentShader in a loop. Links.
     *
     *  @param fragments  Fragment shader sources (e.g. shader.frag wrapper + user source).
     *  @param context    The active OpenGL context.
     *  @param error      Receives error message on failure. Empty on success.
     *  @return           Compiled and linked program, or nullptr on failure.
     */
    static std::unique_ptr<juce::OpenGLShaderProgram> build (
        const juce::StringArray& fragments,
        juce::OpenGLContext& context,
        juce::String& error);

    /** @brief Discovers active uniforms from a linked program and registers
     *         dispatch lambdas in the caller's Function::Map.
     *
     *  Queries glGetActiveUniform for all active uniforms. For each, looks up
     *  the GL type in a static setter table (braced-initializer HashMap) and
     *  registers a lambda capturing the location in the caller's map via add().
     *
     *  @param program   Linked shader program to query.
     *  @param uniforms  Caller's Function::Map to populate with uniform setters.
     */
    static void registerUniforms (
        const juce::OpenGLShaderProgram& program,
        jam::Function::Map<juce::Identifier, void>& uniforms);
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace shader
