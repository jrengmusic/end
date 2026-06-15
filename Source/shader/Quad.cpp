#include "shader/Quad.h"

namespace shader
{
/*____________________________________________________________________________*/

void Quad::create (juce::OpenGLContext& context)
{
    using namespace juce::gl;

    // Load vertex shader from BinaryData and translate to GLSL v3
    vertexShader = juce::OpenGLHelpers::translateVertexShaderToV3 (
        BinaryData::getString ("passthrough.vert"));

    // Triangle strip: two triangles covering [-1,1] NDC
    static constexpr float vertices[] =
    {
        -1.0f, -1.0f,
         1.0f, -1.0f,
        -1.0f,  1.0f,
         1.0f,  1.0f,
    };

    glGenBuffers (1, &vbo);
    glBindBuffer (GL_ARRAY_BUFFER, vbo);
    glBufferData (GL_ARRAY_BUFFER, sizeof (vertices), vertices, GL_STATIC_DRAW);
    glBindBuffer (GL_ARRAY_BUFFER, 0);
}

void Quad::destroy()
{
    using namespace juce::gl;

    if (vbo != 0)
    {
        glDeleteBuffers (1, &vbo);
        vbo = 0;
    }
}

void Quad::draw()
{
    using namespace juce::gl;

    jassert (vbo != 0);

    glBindBuffer (GL_ARRAY_BUFFER, vbo);
    glVertexAttribPointer (positionAttribute, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    glEnableVertexAttribArray (positionAttribute);

    glDrawArrays (GL_TRIANGLE_STRIP, 0, 4);

    glDisableVertexAttribArray (positionAttribute);
    glBindBuffer (GL_ARRAY_BUFFER, 0);
}

/**______________________________END OF NAMESPACE______________________________*/
} // namespace shader
