#pragma once

namespace jreng
{

class GLRenderer : private juce::OpenGLRenderer
{
public:
    GLRenderer();
    ~GLRenderer();

    void attachTo (juce::Component& target);
    void detach();
    void setComponentPaintingEnabled (bool enabled) noexcept;

    using ComponentIterator = std::function<void (std::function<void (GLComponent&)>)>;
    void setComponentIterator (ComponentIterator iterator) noexcept;

    void triggerRepaint();
    void setClippingMask (const juce::Image& mask) noexcept;

private:
    void newOpenGLContextCreated() override;
    void renderOpenGL() override;
    void openGLContextClosing() override;

    juce::OpenGLContext openGLContext;
    jreng::Glyph::GLContext glyphContext;
    ComponentIterator componentIterator;
    float renderingScale { 1.0f };

    std::unique_ptr<juce::OpenGLShaderProgram> flatColourShader;
    GLuint vao { 0 };
    GLuint vbo { 0 };
    GLint projectionUniform { -1 };

    juce::Image clippingMask;
    juce::OpenGLTexture maskTexture;
    GLint viewportSizeUniform { -1 };
    GLint maskSamplerUniform { -1 };
    GLint hasMaskUniform { -1 };
    bool maskDirty { false };

    void compileFlatColourShader();
    void destroyGLResources();
    void uploadMaskTexture();
    void drawVertices (const std::vector<GLVertex>& vertices,
                       float offsetX, float offsetY,
                       GLenum mode = juce::gl::GL_TRIANGLES);
    static void enableSurfaceTransparency();
    void renderComponent (GLComponent* comp, const juce::Component* target, float totalScale, float vpWidth, float vpHeight);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GLRenderer)
};

} // namespace jreng
