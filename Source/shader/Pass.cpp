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
    const juce::String fragmentSource {
        BinaryData::getString ("shadertoy_uniforms.frag")
        + "\n" + source + "\n"
        + BinaryData::getString ("shadertoy_main.frag") };

    const juce::String translatedFragment {
        juce::OpenGLHelpers::translateFragmentShaderToV3 (fragmentSource) };

    auto prog = std::make_unique<juce::OpenGLShaderProgram> (context);

    const bool vertexOk   { prog->addVertexShader (vertexShader) };
    const bool fragmentOk { vertexOk and prog->addFragmentShader (translatedFragment) };
    const bool linked     { fragmentOk and prog->link() };

    if (not linked)
    {
        const juce::String error { prog->getLastError() };
        prog.reset();
        return error;
    }

    program = std::move (prog);

    // Cache uniform locations — avoids string lookup every frame
    const GLuint id { program->getProgramID() };
    locResolution = glGetUniformLocation (id, "iResolution");
    locTime       = glGetUniformLocation (id, "iTime");
    locTimeDelta  = glGetUniformLocation (id, "iTimeDelta");
    locFrame      = glGetUniformLocation (id, "iFrame");

    return {};
}

//==============================================================================
void Pass::render (Quad& quad, int width, int height,
                   float time, float timeDelta, int frame)
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

    quad.draw();
}

//==============================================================================
void Pass::release()
{
    program.reset();
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace shader
