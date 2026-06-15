#pragma once
#include <JuceHeader.h>

namespace shader
{
/*____________________________________________________________________________*/

/** @brief Fullscreen quad geometry — shared by all shader passes.
 *
 *  Owns a VBO with 4 vertices in a triangle strip covering [-1,1] NDC.
 *  The passthrough vertex shader is loaded from BinaryData (passthrough.vert).
 *
 *  GL resource lifecycle: create() allocates VBO on the GL thread
 *  (call from newOpenGLContextCreated). destroy() releases it
 *  (call from openGLContextClosing).
 *
 *  Thread contract: ALL methods GL THREAD only.
 */
struct Quad
{
    /** @brief Allocates the VBO and translates the vertex shader from BinaryData.
     *  @param context  The active OpenGL context.
     */
    void create (juce::OpenGLContext& context);

    /** @brief Releases the VBO. Safe to call if not created. */
    void destroy();

    /** @brief Binds the VBO, enables the position attribute, draws, disables, unbinds. */
    void draw();

    /** @brief Returns the compiled vertex shader source (GLSL v3 translated).
     *  Valid after create(). Used by shader::Pass to link programs.
     */
    const juce::String& getVertexShader() const noexcept { return vertexShader; }

    /** @brief Returns true if the VBO has been created. */
    bool isCreated() const noexcept { return vbo != 0; }

private:
    /** @brief VBO handle. Zero when not created. */
    GLuint vbo { 0 };

    /** @brief Vertex shader source translated to GLSL v3 by JUCE. Valid after create(). */
    juce::String vertexShader;

    /** @brief Position attribute location in the passthrough vertex shader. */
    static constexpr GLuint positionAttribute { 0 };
};

/**______________________________END OF NAMESPACE______________________________*/
} // namespace shader
