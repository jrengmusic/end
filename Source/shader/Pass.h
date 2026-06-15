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
 *  Owns an OpenGLShaderProgram, caches Shadertoy uniform locations, and
 *  binds up to 4 iChannel textures per render call.
 *
 *  compile() wraps the user's fragment shader with the Shadertoy uniform
 *  block and mainImage→main bridge, then translates via
 *  OpenGLHelpers::translateFragmentShaderToV3 for GL version portability.
 *
 *  Thread contract: ALL methods GL THREAD only.
 */
struct Pass
{
    /** @brief Compiles a Shadertoy fragment shader.
     *
     *  Wraps the user source with the uniform block + mainImage bridge,
     *  translates to GL v3, links with the quad's vertex shader.
     *
     *  @param source        Raw user fragment shader (contains mainImage).
     *  @param vertexShader  Translated vertex shader from Quad::getVertexShader().
     *  @param context       The active OpenGL context.
     *  @return              Empty string on success, error message on failure.
     */
    juce::String compile (const juce::String& source,
                          const juce::String& vertexShader,
                          juce::OpenGLContext& context);

    /** @brief Renders the pass — sets all uniforms, binds iChannel textures, draws quad.
     *
     *  @param quad       Shared fullscreen quad geometry.
     *  @param width      Viewport width in pixels (for iResolution).
     *  @param height     Viewport height in pixels.
     *  @param time       Seconds since context created (iTime).
     *  @param timeDelta  Seconds since last frame (iTimeDelta).
     *  @param frame      Frame counter (iFrame).
     *  @param mouse      Mouse state: xy = current pos, zw = click pos (iMouse).
     */
    void render (Quad& quad, int width, int height,
                 float time, float timeDelta, int frame,
                 const float mouse[4]);

    /** @brief Binds a texture to an iChannel slot (0–3).
     *  @param channel    Slot index (0–3).
     *  @param textureID  GL texture handle to bind, or 0 to unbind.
     */
    void setChannel (int channel, GLuint textureID);

    /** @brief Returns true if a program is compiled and linked. */
    bool isLoaded() const noexcept { return program != nullptr; }

    /** @brief Releases the compiled program. */
    void release();

private:
    std::unique_ptr<juce::OpenGLShaderProgram> program;

    // Shadertoy uniform locations — cached after compile, -1 = not found
    GLint locResolution        { -1 };
    GLint locTime              { -1 };
    GLint locTimeDelta         { -1 };
    GLint locFrame             { -1 };
    GLint locMouse             { -1 };
    GLint locDate              { -1 };
    GLint locChannelResolution { -1 };
    GLint locChannel[4]        { -1, -1, -1, -1 };

    // Currently bound channel texture IDs
    GLuint channelTextures[4] { 0, 0, 0, 0 };
};

/**______________________________END OF NAMESPACE______________________________*/
} // namespace shader
