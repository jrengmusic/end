#include "shader/Compiler.h"

namespace shader
{
/*____________________________________________________________________________*/

using namespace juce::gl;
using Setter = std::function<void (GLint, const juce::var&)>;

// GLenum → glUniform* setter. Braced initializer — no switch, no brackets.
static const jam::HashMap<GLenum, Setter> setters {
    { GL_FLOAT,      [] (GLint loc, const juce::var& v) { glUniform1f (loc, static_cast<GLfloat> (v)); } },
    { GL_FLOAT_VEC2, [] (GLint loc, const juce::var& v)
        {
            if (auto* a { v.getArray() })
                glUniform2f (loc,
                             static_cast<GLfloat> (a->getReference (0)),
                             static_cast<GLfloat> (a->getReference (1)));
        } },
    { GL_FLOAT_VEC3, [] (GLint loc, const juce::var& v)
        {
            if (auto* a { v.getArray() })
                glUniform3f (loc,
                             static_cast<GLfloat> (a->getReference (0)),
                             static_cast<GLfloat> (a->getReference (1)),
                             static_cast<GLfloat> (a->getReference (2)));
        } },
    { GL_FLOAT_VEC4, [] (GLint loc, const juce::var& v)
        {
            if (auto* a { v.getArray() })
                glUniform4f (loc,
                             static_cast<GLfloat> (a->getReference (0)),
                             static_cast<GLfloat> (a->getReference (1)),
                             static_cast<GLfloat> (a->getReference (2)),
                             static_cast<GLfloat> (a->getReference (3)));
        } },
    { GL_INT,        [] (GLint loc, const juce::var& v) { glUniform1i (loc, static_cast<GLint> (v)); } },
    { GL_BOOL,       [] (GLint loc, const juce::var& v) { glUniform1i (loc, static_cast<GLint> (v)); } },
    { GL_SAMPLER_2D, [] (GLint loc, const juce::var& v) { glUniform1i (loc, static_cast<GLint> (v)); } },
};

//==============================================================================
std::unique_ptr<juce::OpenGLShaderProgram> Compiler::build (
    const juce::StringArray& fragments,
    juce::OpenGLContext& context,
    juce::String& error)
{
    auto prog { std::make_unique<juce::OpenGLShaderProgram> (context) };

    const auto vertexSource { juce::OpenGLHelpers::translateVertexShaderToV3 (
        BinaryData::getString (config::File::Shaders::getName (config::File::Shaders::quad))) };

    prog->addVertexShader (vertexSource);

    for (const auto& frag : fragments)
        prog->addFragmentShader (frag);

    if (not prog->link())
    {
        error = prog->getLastError();
        return nullptr;
    }

    return prog;
}

//==============================================================================
void Compiler::registerUniforms (
    const juce::OpenGLShaderProgram& program,
    jam::Function::Map<juce::Identifier, void>& uniforms)
{
    const GLuint programId { program.getProgramID() };
    GLint uniformCount { 0 };
    glGetProgramiv (programId, GL_ACTIVE_UNIFORMS, &uniformCount);

    for (GLint i { 0 }; i < uniformCount; ++i)
    {
        GLchar name[256];
        GLsizei nameLength { 0 };
        GLint   size       { 0 };
        GLenum  type       { 0 };

        glGetActiveUniform (programId, static_cast<GLuint> (i),
                            sizeof (name), &nameLength, &size, &type, name);

        const GLint location { glGetUniformLocation (programId, name) };

        if (location >= 0)
        {
            const auto it { setters.find (type) };

            if (it != setters.end())
            {
                const auto& setter { it->second };

                uniforms.add<const juce::var&> (
                    juce::Identifier { juce::String (name, static_cast<size_t> (nameLength)) },
                    [location, &setter] (const juce::var& v)
                    {
                        setter (location, v);
                    });
            }
        }
    }
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace shader
