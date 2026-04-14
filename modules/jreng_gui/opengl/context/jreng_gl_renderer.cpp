namespace jreng
{

GLRenderer::GLRenderer()
{
    const auto* display { juce::Desktop::getInstance().getDisplays().getPrimaryDisplay() };
    renderingScale = display != nullptr ? static_cast<float> (display->scale) : 1.0f;

    juce::OpenGLPixelFormat pixelFormat;
    pixelFormat.stencilBufferBits = 8;
    openGLContext.setPixelFormat (pixelFormat);
    openGLContext.setOpenGLVersionRequired (juce::OpenGLContext::openGL3_2);
    openGLContext.setRenderer (this);
    openGLContext.setComponentPaintingEnabled (false);
    openGLContext.setContinuousRepainting (false);
}

GLRenderer::~GLRenderer()
{
    openGLContext.detach();
}

void GLRenderer::setSharedRenderer (GLRenderer& source)
{
    jassert (not openGLContext.isAttached());
    openGLContext.setNativeSharedContext (source.openGLContext.getRawContext());
}

void GLRenderer::attachTo (juce::Component& target)
{
    if (not openGLContext.isAttached())
        openGLContext.attachTo (target);
}

void GLRenderer::detach()
{
    openGLContext.detach();
}

void GLRenderer::setComponentPaintingEnabled (bool enabled) noexcept
{
    openGLContext.setComponentPaintingEnabled (enabled);
}

void GLRenderer::setRenderables (ComponentIterator iterator) noexcept
{
    componentIterator = std::move (iterator);
}

void GLRenderer::setRenderable (GLRenderer& renderer, jreng::GLComponent& content)
{
    renderer.setRenderables (
        [&content] (std::function<void (jreng::GLComponent&)> renderComponent)
        {
            if (content.isVisible())
                renderComponent (content);
        });
}

void GLRenderer::triggerRepaint()
{
    openGLContext.triggerRepaint();
}

void GLRenderer::setNativeSharedContext (void* handle) noexcept
{
    jassert (not openGLContext.isAttached());
    openGLContext.setNativeSharedContext (handle);
}

void* GLRenderer::getNativeContext() const noexcept
{
    return openGLContext.getRawContext();
}

bool GLRenderer::isAttached() const noexcept
{
    return openGLContext.isAttached();
}

void GLRenderer::setClippingMask (const juce::Image& mask) noexcept
{
    // UI THREAD
    clippingMask = mask;
    maskDirty = true;
}

void GLRenderer::newOpenGLContextCreated()
{
    // GL THREAD
    enableSurfaceTransparency();
    compileFlatColourShader();

    if (flatColourShader != nullptr)
    {
        juce::gl::glGenVertexArrays (1, &vao);
        juce::gl::glGenBuffers (1, &vbo);
    }

    contextReady();
    notifyComponentsCreated();
}

void GLRenderer::notifyComponentsCreated()
{
    if (componentIterator)
    {
        componentIterator ([] (GLComponent& comp)
        {
            comp.glContextCreated();
        });
    }
}

void GLRenderer::renderOpenGL()
{
    // GL THREAD
    juce::gl::glClearColor (0.0f, 0.0f, 0.0f, 0.0f);
    juce::gl::glClear (juce::gl::GL_COLOR_BUFFER_BIT);

    if (flatColourShader != nullptr)
    {
        if (const auto* target { openGLContext.getTargetComponent() })
        {
            juce::gl::glEnable (juce::gl::GL_BLEND);
            juce::gl::glBlendFunc (juce::gl::GL_SRC_ALPHA, juce::gl::GL_ONE_MINUS_SRC_ALPHA);

            flatColourShader->use();

            GLint viewport[4];
            juce::gl::glGetIntegerv (juce::gl::GL_VIEWPORT, viewport);
            const float vpWidth { static_cast<float> (viewport[2]) };
            const float vpHeight { static_cast<float> (viewport[3]) };

            float projection[16] {
                2.0f / vpWidth,  0.0f,             0.0f, 0.0f,
                0.0f,           -2.0f / vpHeight,  0.0f, 0.0f,
                0.0f,            0.0f,            -1.0f, 0.0f,
               -1.0f,            1.0f,             0.0f, 1.0f
            };

            juce::gl::glUniformMatrix4fv (projectionUniform, 1, juce::gl::GL_FALSE, projection);

            if (maskDirty)
                uploadMaskTexture();

            if (maskTexture.getTextureID() != 0 and hasMaskUniform >= 0)
            {
                juce::gl::glActiveTexture (juce::gl::GL_TEXTURE0);
                maskTexture.bind();

                if (maskSamplerUniform >= 0)
                    juce::gl::glUniform1i (maskSamplerUniform, 0);

                if (viewportSizeUniform >= 0)
                    juce::gl::glUniform2f (viewportSizeUniform, vpWidth, vpHeight);

                juce::gl::glUniform1i (hasMaskUniform, 1);
            }
            else if (hasMaskUniform >= 0)
            {
                juce::gl::glUniform1i (hasMaskUniform, 0);
            }

            const float uiScale { juce::Component::getApproximateScaleFactorForComponent (target) };
            const float totalScale { uiScale * renderingScale };

            if (componentIterator)
            {
                componentIterator ([&] (GLComponent& glComp)
                {
                    if (glComp.isVisible())
                        renderComponent (&glComp, target, totalScale, vpWidth, vpHeight);
                });
            }

            if (maskTexture.getTextureID() != 0)
                maskTexture.unbind();

            juce::gl::glBindVertexArray (0);
            juce::gl::glUseProgram (0);
            juce::gl::glDisable (juce::gl::GL_BLEND);
        }
    }
}

void GLRenderer::drawVertices (const std::vector<GLVertex>& vertices,
                              float offsetX, float offsetY,
                              GLenum mode)
{
    if (not vertices.empty())
    {
        std::vector<GLVertex> offsetVerts { vertices };

        for (auto& v : offsetVerts)
        {
            v.x = v.x + offsetX;
            v.y = v.y + offsetY;
        }

        juce::gl::glBindBuffer (juce::gl::GL_ARRAY_BUFFER, vbo);
        juce::gl::glBufferData (juce::gl::GL_ARRAY_BUFFER,
                                static_cast<GLsizeiptr> (offsetVerts.size() * sizeof (GLVertex)),
                                offsetVerts.data(),
                                juce::gl::GL_DYNAMIC_DRAW);

        juce::gl::glVertexAttribPointer (0, 2, juce::gl::GL_FLOAT, juce::gl::GL_FALSE,
                                         sizeof (GLVertex),
                                         reinterpret_cast<void*> (0));
        juce::gl::glEnableVertexAttribArray (0);

        juce::gl::glVertexAttribPointer (1, 4, juce::gl::GL_FLOAT, juce::gl::GL_FALSE,
                                         sizeof (GLVertex),
                                         reinterpret_cast<void*> (2 * sizeof (float)));
        juce::gl::glEnableVertexAttribArray (1);

        juce::gl::glDrawArrays (mode, 0,
                                static_cast<GLsizei> (offsetVerts.size()));
    }
}

void GLRenderer::openGLContextClosing()
{
    // GL THREAD
    notifyComponentsClosing();
    contextClosing();
    destroyGLResources();
}

void GLRenderer::notifyComponentsClosing()
{
    if (componentIterator)
    {
        componentIterator ([] (GLComponent& comp)
        {
            comp.glContextClosing();
        });
    }
}

void GLRenderer::compileFlatColourShader()
{
    const auto vertexSource { BinaryData::getString ("flat_colour.vert") };
    const auto fragmentSource { BinaryData::getString ("flat_colour.frag") };

    auto* glContext { juce::OpenGLContext::getCurrentContext() };
    if (glContext != nullptr)
    {
        flatColourShader = GLShaderCompiler::compile (*glContext, vertexSource, fragmentSource);

        if (flatColourShader != nullptr)
        {
            const auto programID { flatColourShader->getProgramID() };
            projectionUniform = juce::gl::glGetUniformLocation (programID, "uProjection");
            viewportSizeUniform = juce::gl::glGetUniformLocation (programID, "uViewportSize");
            maskSamplerUniform = juce::gl::glGetUniformLocation (programID, "uMaskTexture");
            hasMaskUniform = juce::gl::glGetUniformLocation (programID, "uHasMask");
        }
    }
}

void GLRenderer::uploadMaskTexture()
{
    // GL THREAD
    maskTexture.release();

    if (clippingMask.isValid())
        maskTexture.loadImage (clippingMask);

    maskDirty = false;
}

void GLRenderer::destroyGLResources()
{
    maskTexture.release();

    if (vbo != 0)
    {
        juce::gl::glDeleteBuffers (1, &vbo);
        vbo = 0;
    }

    if (vao != 0)
    {
        juce::gl::glDeleteVertexArrays (1, &vao);
        vao = 0;
    }

    flatColourShader.reset();
}


#if ! JUCE_MAC
void GLRenderer::enableSurfaceTransparency()
{
    // TODO: Windows/Linux GL surface transparency
}
#endif

void GLRenderer::renderComponent (GLComponent* comp, const juce::Component* target, float totalScale, float vpWidth, float vpHeight)
{
    comp->setFullViewportHeight (static_cast<int> (vpHeight));
    comp->paintGL();

    const auto localBounds { comp->getLocalBounds() };
    const float compWidth { static_cast<float> (localBounds.getWidth()) };
    const float compHeight { static_cast<float> (localBounds.getHeight()) };

    GLGraphics g { compWidth, compHeight, totalScale };
    comp->paintGL (g);

    if (g.hasContent())
    {
        const auto origin { target->getLocalPoint (comp, juce::Point<float> (0.0f, 0.0f)) };
        const float destX { origin.x * totalScale };
        const float destY { origin.y * totalScale };
        const float physW { compWidth * totalScale };
        const float physH { compHeight * totalScale };

        juce::gl::glEnable (juce::gl::GL_SCISSOR_TEST);
        juce::gl::glScissor (static_cast<GLint> (destX),
                             static_cast<GLint> (vpHeight - destY - physH),
                             static_cast<GLsizei> (physW),
                             static_cast<GLsizei> (physH));

        juce::gl::glBindVertexArray (vao);

        for (const auto& cmd : g.getCommands())
        {
            if (cmd.type == GLGraphics::CommandType::pushClip)
            {
                juce::gl::glEnable (juce::gl::GL_STENCIL_TEST);
                juce::gl::glStencilFunc (juce::gl::GL_ALWAYS, 1, 0xFF);
                juce::gl::glStencilOp (juce::gl::GL_KEEP, juce::gl::GL_KEEP, juce::gl::GL_REPLACE);
                juce::gl::glStencilMask (0xFF);
                juce::gl::glClear (juce::gl::GL_STENCIL_BUFFER_BIT);
                juce::gl::glColorMask (juce::gl::GL_FALSE, juce::gl::GL_FALSE, juce::gl::GL_FALSE, juce::gl::GL_FALSE);

                drawVertices (cmd.vertices, destX, destY);

                juce::gl::glColorMask (juce::gl::GL_TRUE, juce::gl::GL_TRUE, juce::gl::GL_TRUE, juce::gl::GL_TRUE);
                juce::gl::glStencilFunc (juce::gl::GL_EQUAL, 1, 0xFF);
                juce::gl::glStencilMask (0x00);
            }
            else if (cmd.type == GLGraphics::CommandType::popClip)
            {
                juce::gl::glDisable (juce::gl::GL_STENCIL_TEST);
            }
            else if (cmd.type == GLGraphics::CommandType::draw)
            {
                drawVertices (cmd.vertices, destX, destY);
            }
            else if (cmd.type == GLGraphics::CommandType::drawLineStrip)
            {
                drawVertices (cmd.vertices, destX, destY, juce::gl::GL_LINE_STRIP);
            }
        }

        juce::gl::glDisable (juce::gl::GL_SCISSOR_TEST);
        juce::gl::glDisable (juce::gl::GL_STENCIL_TEST);
    }

    renderText (g, target, comp, totalScale, vpHeight);
}

} // namespace jreng
