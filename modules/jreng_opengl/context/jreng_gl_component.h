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
    virtual void paintGL() noexcept {}

    virtual void paintGL (GLGraphics& g) noexcept {}

    void setScale (float s) noexcept { scale = s; }
    float getScale() const noexcept { return scale; }

    void setFullViewportHeight (int h) noexcept { fullViewportHeight = h; }
    int getFullViewportHeight() const noexcept { return fullViewportHeight; }

private:
    float scale { 1.0f };
    int fullViewportHeight { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GLComponent)
};

} // namespace jreng
