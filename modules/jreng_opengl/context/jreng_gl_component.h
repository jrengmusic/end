#pragma once

namespace jreng
{

class GLComponent : public juce::Component
{
public:
    GLComponent() = default;
    ~GLComponent() override = default;

    virtual void glContextCreated() noexcept {}
    virtual void glContextClosing() noexcept {}
    virtual void renderGL() noexcept {}

    virtual void renderGL (GLGraphics& g) noexcept {}

    void setScale (float s) noexcept { scale = s; }
    float getScale() const noexcept { return scale; }

private:
    float scale { 1.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GLComponent)
};

} // namespace jreng
