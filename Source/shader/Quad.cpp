#include "shader/Quad.h"

namespace shader
{
/*____________________________________________________________________________*/

void Quad::create()
{
    using namespace juce::gl;

    static constexpr float vertices[] {
        -1.0f, -1.0f,
         1.0f, -1.0f,
        -1.0f,  1.0f,
         1.0f,  1.0f,
    };

    glGenVertexArrays (1, &vao);
    glBindVertexArray (vao);

    glGenBuffers (1, &vbo);
    glBindBuffer (GL_ARRAY_BUFFER, vbo);
    glBufferData (GL_ARRAY_BUFFER, sizeof (vertices), vertices, GL_STATIC_DRAW);

    glVertexAttribPointer (positionAttribute, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    glEnableVertexAttribArray (positionAttribute);

    glBindVertexArray (0);
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

    if (vao != 0)
    {
        glDeleteVertexArrays (1, &vao);
        vao = 0;
    }
}

void Quad::draw()
{
    using namespace juce::gl;

    jassert (vao != 0);

    glBindVertexArray (vao);
    glDrawArrays (GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray (0);
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace shader
