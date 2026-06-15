/**
 * @file shader/Pass.h
 * @brief One compiled Shadertoy-compatible shader pass.
 */
#pragma once
#include <JuceHeader.h>
#include "shader/Quad.h"

namespace shader
{
/*____________________________________________________________________________*/

/** @brief One compiled Shadertoy-compatible shader pass.
 *
 *  Owns an OpenGLShaderProgram, caches the Shadertoy minimum uniform
 *  locations (iResolution, iTime, iTimeDelta, iFrame), and draws a fullscreen
 *  quad per render call.
 *
 *  compile() prepends shadertoy_uniforms.frag and appends shadertoy_main.frag
 *  from BinaryData, then translates via
 *  OpenGLHelpers::translateFragmentShaderToV3 for GL version portability.
 *
 *  Thread contract: ALL methods GL THREAD only.
 */
struct Pass
{
    /** @brief Compiles a Shadertoy fragment shader.
     *
     *  Wraps the user source with the uniform block (shadertoy_uniforms.frag)
     *  and mainImage bridge (shadertoy_main.frag) from BinaryData, translates
     *  to GL v3, and links with the quad's vertex shader.
     *
     *  @param source        Raw user fragment shader (contains mainImage).
     *  @param vertexShader  Translated vertex shader from Quad::getVertexShader().
     *  @param context       The active OpenGL context.
     *  @return              Empty string on success, error message on failure.
     */
    juce::String compile (const juce::String& source,
                          const juce::String& vertexShader,
                          juce::OpenGLContext& context);

    /** @brief Renders the pass — sets Shadertoy minimum uniforms and draws the quad.
     *
     *  @param quad       Shared fullscreen quad geometry.
     *  @param width      Viewport width in pixels (for iResolution).
     *  @param height     Viewport height in pixels.
     *  @param time       Seconds since context created (iTime).
     *  @param timeDelta  Seconds since last frame (iTimeDelta).
     *  @param frame      Frame counter (iFrame).
     */
    void render (Quad& quad, int width, int height,
                 float time, float timeDelta, int frame);

    /** @brief Returns true if a program is compiled and linked. */
    bool isLoaded() const noexcept { return program != nullptr; }

    /** @brief Releases the compiled program. */
    void release();

private:
    std::unique_ptr<juce::OpenGLShaderProgram> program;

    // Shadertoy minimum uniform locations — cached after compile, -1 = not found
    GLint locResolution { -1 };
    GLint locTime       { -1 };
    GLint locTimeDelta  { -1 };
    GLint locFrame      { -1 };
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace shader
