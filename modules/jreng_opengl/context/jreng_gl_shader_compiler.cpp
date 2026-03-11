namespace jreng
{

std::unique_ptr<juce::OpenGLShaderProgram> GLShaderCompiler::compile (
    juce::OpenGLContext& context,
    const juce::String& vertexSource,
    const juce::String& fragmentSource) noexcept
{
    auto shader { std::make_unique<juce::OpenGLShaderProgram> (context) };

    jassert (shader->addVertexShader (vertexSource));
    jassert (shader->addFragmentShader (fragmentSource));
    jassert (shader->link());

    return shader;
}

} // namespace jreng
