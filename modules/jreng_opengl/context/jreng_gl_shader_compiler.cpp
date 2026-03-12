namespace jreng
{

std::unique_ptr<juce::OpenGLShaderProgram> GLShaderCompiler::compile (
    juce::OpenGLContext& context,
    const juce::String& vertexSource,
    const juce::String& fragmentSource) noexcept
{
    auto shader { std::make_unique<juce::OpenGLShaderProgram> (context) };

    if (shader->addVertexShader (vertexSource)
        and shader->addFragmentShader (fragmentSource)
        and shader->link())
    {
        return shader;
    }

    jassertfalse;
    return nullptr;
}

} // namespace jreng
