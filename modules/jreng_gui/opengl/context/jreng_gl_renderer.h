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

    /**
     * @brief Sets the native shared GL context handle on the underlying juce::OpenGLContext.
     *
     * JUCE pre-attach invariant: must be called before attachTo.
     *
     * @param handle  Raw native GL context handle (HGLRC on Windows, NSOpenGLContext on macOS).
     * @note MESSAGE THREAD.
     */
    void setNativeSharedContext (void* handle) noexcept;

    /**
     * @brief Returns the underlying juce::OpenGLContext's raw native handle.
     *
     * Used by parent Window to expose its HGLRC for shared-context inheritance
     * by modal/child Windows.
     *
     * @return Raw native handle, or nullptr if context is not yet attached.
     * @note MESSAGE THREAD.
     */
    void* getNativeContext() const noexcept;

    /**
     * @brief Returns whether the underlying juce::OpenGLContext is attached.
     *
     * @return true if attached, false otherwise.
     * @note MESSAGE THREAD.
     */
    bool isAttached() const noexcept;

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
