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

    void setComponents (jreng::Owner<GLComponent>& source) noexcept;

    void triggerRepaint();
    void setClippingMask (const juce::Image& mask) noexcept;

private:
    void newOpenGLContextCreated() override;
    void renderOpenGL() override;
    void openGLContextClosing() override;

    juce::OpenGLContext openGLContext;
    jreng::Owner<GLComponent>* components { nullptr };
    float renderingScale { 1.0f };

    GLuint shaderProgram { 0 };
    GLuint vao { 0 };
    GLuint vbo { 0 };
    GLint projectionUniform { -1 };

    juce::Image clippingMask;
    juce::OpenGLTexture maskTexture;
    GLint viewportSizeUniform { -1 };
    GLint maskSamplerUniform { -1 };
    GLint hasMaskUniform { -1 };
    bool maskDirty { false };

    void createShaderProgram();
    void destroyGLResources();
    void uploadMaskTexture();
    void drawVertices (const std::vector<GLVertex>& vertices,
                       float offsetX, float offsetY,
                       GLenum mode = juce::gl::GL_TRIANGLES);
    static void enableSurfaceTransparency();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GLRenderer)
};

} // namespace jreng
