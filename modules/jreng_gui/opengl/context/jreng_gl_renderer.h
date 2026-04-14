#pragma once

namespace jreng
{

class GLRenderer : protected juce::OpenGLRenderer
{
public:
    GLRenderer();
    virtual ~GLRenderer();

    void setSharedRenderer (GLRenderer& source);
    void attachTo (juce::Component& target);
    void detach();
    void setComponentPaintingEnabled (bool enabled) noexcept;

    using ComponentIterator = std::function<void (std::function<void (GLComponent&)>)>;
    void setComponentIterator (ComponentIterator iterator) noexcept;

    void triggerRepaint();
    void setClippingMask (const juce::Image& mask) noexcept;

protected:
    /** @brief Initialise subclass GL resources. Called after base GL init, before component notification. @note GL THREAD. */
    virtual void contextReady() {}

    /** @brief Release subclass GL resources. Called after component notification, before base GL teardown. @note GL THREAD. */
    virtual void contextClosing() {}

    /** @brief Render text commands collected in @p g. Called after path/shape rendering. @note GL THREAD. */
    virtual void renderText (GLGraphics& g, const juce::Component* target,
                             GLComponent* comp, float totalScale, float vpHeight) {}

    virtual void renderComponent (GLComponent* comp, const juce::Component* target, float totalScale, float vpWidth, float vpHeight);

    void notifyComponentsCreated();
    void notifyComponentsClosing();

private:
    void newOpenGLContextCreated() override;
    void renderOpenGL() override;
    void openGLContextClosing() override;

    juce::OpenGLContext openGLContext;
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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GLRenderer)
};

} // namespace jreng
