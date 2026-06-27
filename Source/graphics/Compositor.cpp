#include "Compositor.h"

namespace graphics
{
/*____________________________________________________________________________*/

Compositor::Compositor (juce::OpenGLRenderer& rendererRef)
    : renderer (rendererRef)
{
}

void Compositor::attach (juce::Component& view, juce::Component& cacheTarget)
{
    context.setOpenGLVersionRequired (juce::OpenGLContext::openGL4_1);
    context.setMultisamplingEnabled (true);
    context.setRenderer (&renderer);
    cacheTarget.setCachedComponentImage (new CachedImage (*this, cacheTarget));
    context.attachTo (view);
}

void Compositor::detach() { context.detach(); }

bool Compositor::isAttached() const noexcept { return context.isAttached(); }

bool Compositor::isPostProcessing() const noexcept
{
    return shaders.contains (ID::postProcessing)
       and shaders.at (ID::postProcessing)->isCompiled();
}

void Compositor::renderPost (juce::Component& owner, juce::Graphics& g)
{
    using namespace ::juce::gl;

    if (auto* component = context.getTargetComponent())
    {
        auto [screenWidth, screenHeight] = getSize (*component);
        auto cachedFBO { juce::OpenGLFrameBuffer::getCurrentFrameBufferTarget() };

        // 1. Composite background + components into componentCapture FBO
        {
            componentCapture.makeCurrentAndClear();

            if (shaders.at (ID::background)->isCompiled())
            {
                glEnable (GL_BLEND);
                glBlendFunc (GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

                renderTexture (ID::background, screenWidth, screenHeight);

                glDisable (GL_BLEND);
            }

            // Components on top — JUCE paints with correct premultiplied blending
            {
                std::unique_ptr<juce::LowLevelGraphicsContext> captureContext (
                    juce::createOpenGLGraphicsContext (context, componentCapture));
                captureContext->addTransform (juce::AffineTransform::scale (
                    static_cast<float> (context.getRenderingScale())));

                juce::Graphics captureGraphics (*captureContext);
                owner.paintEntireComponent (captureGraphics, false);
            }

            componentCapture.releaseAsRenderingTarget();
        }

        // 2. Post-process: feed composited scene to postProcess Compilation
        shaders.at (ID::postProcessing)->sceneTexture = componentCapture.getTextureID();
        shaders.at (ID::postProcessing)->render (*quad);

        // 3. Output post-processed result to JUCE's cachedImageFrameBuffer
        glBindFramebuffer (GL_FRAMEBUFFER, cachedFBO);

        renderTexture (ID::postProcessing, screenWidth, screenHeight);

        return;
    }

    owner.paintEntireComponent (g, false);
}

void Compositor::triggerRepaint()
{
    context.triggerRepaint();

    if (isPostProcessing())
    {
        if (auto* comp = context.getTargetComponent())
            comp->repaint();
    }
}

//==============================================================================
void Compositor::prepare()
{
    quad = std::make_unique<jam::OpenGL::Quad> (jam::OpenGL::Quad::getScreen());

    outputProgram = jam::OpenGL::createProgram (context, screenQuad, outputShader,
        [this] (const juce::String& msg)
        {
            if (reportError)
                reportError (msg);
        });
    jassert (outputProgram != nullptr);

    for (auto& id : compilationIDs)
        shaders.addOrReplace (id, std::make_unique<Compilation>());

    shaders.at (ID::postProcessing)->uniform.opacity = 1.0f;
    resizeBuffers();
}

void Compositor::process()
{
    if (auto* component = context.getTargetComponent())
    {
        auto [screenWidth, screenHeight] = getSize (*component);

        if (shaders.at (ID::background)->isCompiled())
            shaders.at (ID::background)->render (*quad);

        juce::OpenGLHelpers::clear (juce::Colours::transparentBlack);

        if (shaders.at (ID::background)->isCompiled() and not isPostProcessing())
        {
            juce::gl::glEnable (juce::gl::GL_BLEND);
            juce::gl::glBlendFunc (juce::gl::GL_ONE, juce::gl::GL_ONE_MINUS_SRC_ALPHA);

            renderTexture (ID::background, screenWidth, screenHeight);

            juce::gl::glDisable (juce::gl::GL_BLEND);
        }

        // Components: JUCE paints via CachedImage after renderOpenGL returns.
    }
}

void Compositor::reset()
{
    shaders.clear();
    componentCapture.release();
    outputProgram.reset();
    quad.reset();
}

//==============================================================================
void Compositor::compileShaders (const juce::Identifier& id,
                                 const jam::HashMap<juce::Identifier, juce::String>& sources)
{
    auto* target { shaders.at (id).get() };

    context.executeOnGLThread (
        [this, target, sources] (juce::OpenGLContext&)
        {
            target->load (sources,
                          [this] (juce::StringRef source)
                          {
                              return jam::OpenGL::createProgram (context, screenQuad, source,
                                  [this] (const juce::String& msg)
                                  {
                                      if (reportError)
                                          reportError (msg);
                                  });
                          });

            resizeBuffers();
        },
        false);
}

void Compositor::resize()
{
    context.executeOnGLThread (
        [this] (juce::OpenGLContext&) { resizeBuffers(); },
        false);
}

end::Size Compositor::getSize (const juce::Component& component) const
{
    auto scale { static_cast<float> (context.getRenderingScale()) };
    end::Size size { component.getWidth() * scale,
                     component.getHeight() * scale };
    auto [w, h] = size;
    jassert (w > 0 and h > 0);
    return size;
}

void Compositor::resizeBuffers()
{
    if (auto* comp = context.getTargetComponent())
    {
        auto [w, h] = getSize (*comp);

        for (auto& [id, compilation] : shaders)
            compilation->resize (context, w, h);

        componentCapture.initialise (context, w, h);
    }
}

//==============================================================================
void Compositor::setOpacity (float value) { shaders.at (ID::background)->uniform.opacity = value; }
void Compositor::setPostOpacity (float value)
{
    shaders.at (ID::postProcessing)->postOpacity = value;
}
void Compositor::setTextureFilter (GLenum value) { shaders.at (ID::background)->uniform.textureFilter = value; }

void Compositor::setResolutionScale (float value)
{
    shaders.at (ID::background)->uniform.resolutionScale = value;
    resize();
}

void Compositor::setFrameRate (int fps)
{
    for (auto& [id, compilation] : shaders)
        compilation->uniform.setFrameRate (fps);
}

void Compositor::renderTexture (const juce::Identifier& id, int width, int height)
{
    using namespace ::juce::gl;

    glViewport (0, 0, width, height);
    outputProgram->use();
    outputProgram->setUniform (IDref::iOpacity, shaders.at (id)->uniform.opacity);

    glActiveTexture (GL_TEXTURE0);
    glBindTexture (GL_TEXTURE_2D, shaders.at (id)->getOutputTextureID());
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, shaders.at (id)->uniform.textureFilter);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, shaders.at (id)->uniform.textureFilter);
    outputProgram->setUniform (IDref::outputTexture, 0);
    quad->draw();

    glBindTexture (GL_TEXTURE_2D, 0);
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace graphics
