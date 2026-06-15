#include "shader/Pass.h"

namespace shader
{
/*____________________________________________________________________________*/

using namespace juce::gl;

//==============================================================================
juce::String Pass::compile (const juce::String& source,
                             const juce::String& vertexShader,
                             juce::OpenGLContext& context)
{
    // Wrap user source with the Shadertoy uniform block and mainImage bridge
    const juce::String fragmentSource =
        "uniform vec3      iResolution;\n"
        "uniform float     iTime;\n"
        "uniform float     iTimeDelta;\n"
        "uniform int       iFrame;\n"
        "uniform vec4      iMouse;\n"
        "uniform vec4      iDate;\n"
        "uniform vec3      iChannelResolution[4];\n"
        "uniform sampler2D iChannel0;\n"
        "uniform sampler2D iChannel1;\n"
        "uniform sampler2D iChannel2;\n"
        "uniform sampler2D iChannel3;\n"
        "\n"
        + source +
        "\n"
        "void main()\n"
        "{\n"
        "    vec4 col;\n"
        "    mainImage(col, gl_FragCoord.xy);\n"
        "    gl_FragColor = col;\n"
        "}\n";

    const juce::String translatedFragment =
        juce::OpenGLHelpers::translateFragmentShaderToV3 (fragmentSource);

    auto prog = std::make_unique<juce::OpenGLShaderProgram> (context);

    const bool vertexOk   = prog->addVertexShader (vertexShader);
    const bool fragmentOk = vertexOk and prog->addFragmentShader (translatedFragment);
    const bool linked     = fragmentOk and prog->link();

    if (not linked)
    {
        const juce::String error = prog->getLastError();
        prog.reset();
        return error;
    }

    program = std::move (prog);

    // Cache uniform locations — avoids string lookup every frame
    const GLuint id = program->getProgramID();
    locResolution        = glGetUniformLocation (id, "iResolution");
    locTime              = glGetUniformLocation (id, "iTime");
    locTimeDelta         = glGetUniformLocation (id, "iTimeDelta");
    locFrame             = glGetUniformLocation (id, "iFrame");
    locMouse             = glGetUniformLocation (id, "iMouse");
    locDate              = glGetUniformLocation (id, "iDate");
    locChannelResolution = glGetUniformLocation (id, "iChannelResolution");
    locChannel[0]        = glGetUniformLocation (id, "iChannel0");
    locChannel[1]        = glGetUniformLocation (id, "iChannel1");
    locChannel[2]        = glGetUniformLocation (id, "iChannel2");
    locChannel[3]        = glGetUniformLocation (id, "iChannel3");

    return {};
}

//==============================================================================
void Pass::render (Quad& quad, int width, int height,
                   float time, float timeDelta, int frame,
                   const float mouse[4])
{
    program->use();

    if (locResolution >= 0)
        glUniform3f (locResolution, static_cast<GLfloat> (width),
                                    static_cast<GLfloat> (height),
                                    1.0f);

    if (locTime >= 0)
        glUniform1f (locTime, static_cast<GLfloat> (time));

    if (locTimeDelta >= 0)
        glUniform1f (locTimeDelta, static_cast<GLfloat> (timeDelta));

    if (locFrame >= 0)
        glUniform1i (locFrame, static_cast<GLint> (frame));

    if (locMouse >= 0)
        glUniform4f (locMouse,
                     static_cast<GLfloat> (mouse[0]),
                     static_cast<GLfloat> (mouse[1]),
                     static_cast<GLfloat> (mouse[2]),
                     static_cast<GLfloat> (mouse[3]));

    // iDate not set — requires system time, deferred by design

    // Bind iChannel textures and set sampler uniforms
    for (int i = 0; i < 4; ++i)
    {
        if (channelTextures[i] != 0)
        {
            glActiveTexture (GL_TEXTURE0 + static_cast<GLenum> (i));
            glBindTexture (GL_TEXTURE_2D, channelTextures[i]);

            if (locChannel[i] >= 0)
                glUniform1i (locChannel[i], static_cast<GLint> (i));
        }
    }

    quad.draw();

    // Unbind textures — leave the context clean for the next pass
    for (int i = 3; i >= 0; --i)
    {
        if (channelTextures[i] != 0)
        {
            glActiveTexture (GL_TEXTURE0 + static_cast<GLenum> (i));
            glBindTexture (GL_TEXTURE_2D, 0);
        }
    }

    glActiveTexture (GL_TEXTURE0);
}

//==============================================================================
void Pass::setChannel (int channel, GLuint textureID)
{
    channelTextures[channel] = textureID;
}

//==============================================================================
void Pass::release()
{
    program.reset();
}

/**______________________________END OF NAMESPACE______________________________*/
} // namespace shader
