#include "shader/Compiler.h"

namespace shader
{
/*____________________________________________________________________________*/

//==============================================================================
std::unique_ptr<juce::OpenGLShaderProgram> Compiler::build (const juce::String& vertexSource,
                                                            const juce::String& fragmentSource,
                                                            juce::OpenGLContext& context,
                                                            juce::String& error)
{
    auto prog { std::make_unique<juce::OpenGLShaderProgram> (context) };

    prog->addVertexShader (juce::OpenGLHelpers::translateVertexShaderToV3 (vertexSource));
    prog->addFragmentShader (juce::OpenGLHelpers::translateFragmentShaderToV3 (fragmentSource));

    juce::gl::glBindAttribLocation (prog->getProgramID(), 0, "position");

    if (not prog->link())
    {
        error = prog->getLastError();
        return nullptr;
    }

    return prog;
}

//==============================================================================
Compiler::UniformSetter Compiler::buildUniformSetter (const juce::OpenGLShaderProgram& program)
{
    using namespace juce::gl;

    const GLuint programId { program.getProgramID() };
    GLint uniformCount { 0 };
    glGetProgramiv (programId, GL_ACTIVE_UNIFORMS, &uniformCount);

    // Discover locations for known Shadertoy uniforms
    GLint locResolution { glGetUniformLocation (programId, "iResolution") };
    GLint locTime       { glGetUniformLocation (programId, "iTime") };
    GLint locTimeDelta  { glGetUniformLocation (programId, "iTimeDelta") };
    GLint locFrame      { glGetUniformLocation (programId, "iFrame") };

    // iChannel sampler locations
    std::array<GLint, 4> locChannels {
        glGetUniformLocation (programId, "iChannel0"),
        glGetUniformLocation (programId, "iChannel1"),
        glGetUniformLocation (programId, "iChannel2"),
        glGetUniformLocation (programId, "iChannel3"),
    };

    return [locResolution, locTime, locTimeDelta, locFrame, locChannels]
           (float width, float height, float time, float timeDelta, int frame)
    {
        // glGetUniformLocation returns -1 for inactive uniforms.
        // Positive check: if this uniform is active, set it.
        if (locResolution >= 0) glUniform3f (locResolution, width, height, 1.0f);
        if (locTime >= 0)       glUniform1f (locTime, time);
        if (locTimeDelta >= 0)  glUniform1f (locTimeDelta, timeDelta);
        if (locFrame >= 0)      glUniform1i (locFrame, frame);

        for (int ch { 0 }; ch < 4; ++ch)
            if (locChannels.at (static_cast<size_t> (ch)) >= 0)
                glUniform1i (locChannels.at (static_cast<size_t> (ch)), ch);
    };
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace shader
