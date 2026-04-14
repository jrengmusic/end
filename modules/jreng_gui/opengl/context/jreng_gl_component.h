#pragma once

namespace jreng
{

/**
 * @class GLComponent
 * @brief juce::Component with optional GL-accelerated render hooks.
 *
 * The inherited @c paint(juce::Graphics&) is the baseline software contract
 * every subclass honors.  @c paintGL() and @c paintGL(GLGraphics&) are
 * optional optimizations that fire only when a @c jreng::GLRenderer is active
 * on the owning @c jreng::Window.
 *
 * In CPU mode, only @c paint() runs.  In GPU mode, both @c paint() —
 * rasterized into the GL framebuffer via @c setComponentPaintingEnabled(true)
 * — and @c paintGL() — composited into the same framebuffer — run together.
 *
 * @see jreng::Window
 * @see jreng::GLRenderer
 */
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
