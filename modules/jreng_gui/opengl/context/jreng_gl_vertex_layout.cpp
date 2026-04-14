namespace jreng
{

void GLVertexLayout::setupAttributes (const Attribute* attributes, int count) noexcept
{
    for (int i { 0 }; i < count; ++i)
    {
        const Attribute& attr { attributes[i] };
        juce::gl::glEnableVertexAttribArray (attr.location);
        juce::gl::glVertexAttribPointer (attr.location,
                                         attr.size,
                                         attr.type,
                                         juce::gl::GL_FALSE,
                                         attr.stride,
                                         reinterpret_cast<const void*> (attr.offset));
        juce::gl::glVertexAttribDivisor (attr.location, attr.divisor);
    }
}

} // namespace jreng
