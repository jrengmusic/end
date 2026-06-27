#include "Compositor.h"

namespace graphics
{
/*____________________________________________________________________________*/

void Compositor::attach (juce::OpenGLRenderer& renderer, juce::Component& component)
{
    context.setOpenGLVersionRequired (juce::OpenGLContext::openGL4_1);
    context.setMultisamplingEnabled (true);
    context.setRenderer (&renderer);
    context.attachTo (component);
}

void Compositor::detach() { context.detach(); }

bool Compositor::isAttached() const noexcept { return context.isAttached(); }

void Compositor::triggerRepaint() { context.triggerRepaint(); }

//==============================================================================
void Compositor::prepare()
{
    quad = std::make_unique<Quad>();

    outputProgram = createProgram (outputShader);
    jassert (outputProgram != nullptr);

    if (auto* comp = context.getTargetComponent())
    {
        auto scale = context.getRenderingScale();
        resizeBuffers (juce::roundToInt (comp->getWidth() * scale),
                       juce::roundToInt (comp->getHeight() * scale));
    }
}

void Compositor::process()
{
    auto* component { context.getTargetComponent() };
    jassert (component != nullptr);

    auto scale { static_cast<float> (context.getRenderingScale()) };
    int screenWidth { juce::roundToInt (component->getWidth() * scale) };
    int screenHeight { juce::roundToInt (component->getHeight() * scale) };

    if (screenWidth <= 0 or screenHeight <= 0)
        return;

    if (background.isCompiled())
    {
        background.uniform.advance();
        background.renderBuffers (*quad);
        background.renderImage (*quad);
    }

    juce::OpenGLHelpers::clear (juce::Colours::transparentBlack);

    if (background.isCompiled())
        renderOutput (*quad, screenWidth, screenHeight);

    // Components: JUCE paints via CachedImage after renderOpenGL returns.
}

void Compositor::reset()
{
    background.shutdown();
    outputProgram.reset();
    quad.reset();
}

//==============================================================================
void Compositor::loadShaders (const juce::ValueTree& shaderTree)
{
    context.executeOnGLThread (
        [this, shaderTree] (juce::OpenGLContext&)
        {
            background.load (shaderTree,
                             wrapper,
                             placeholder,
                             [this] (juce::StringRef source)
                             {
                                 return createProgram (source);
                             });

            if (auto* comp = context.getTargetComponent())
            {
                auto scale = context.getRenderingScale();
                resizeBuffers (juce::roundToInt (comp->getWidth() * scale),
                               juce::roundToInt (comp->getHeight() * scale));
            }
        },
        false);
}

void Compositor::resize (int width, int height)
{
    context.executeOnGLThread (
        [this, width, height] (juce::OpenGLContext&)
        {
            auto scale = context.getRenderingScale();
            resizeBuffers (juce::roundToInt (width * scale),
                           juce::roundToInt (height * scale));
        },
        false);
}

void Compositor::resizeBuffers (int screenWidth, int screenHeight)
{
    background.resize (context, screenWidth, screenHeight);
}

//==============================================================================
void Compositor::setOpacity (float value) { background.uniform.opacity = value; }
void Compositor::setTextureFilter (GLenum value) { background.uniform.textureFilter = value; }

void Compositor::setResolutionScale (float value)
{
    background.uniform.resolutionScale = value;

    context.executeOnGLThread (
        [this] (juce::OpenGLContext&)
        {
            if (auto* comp = context.getTargetComponent())
            {
                auto scale = context.getRenderingScale();
                resizeBuffers (juce::roundToInt (comp->getWidth() * scale),
                               juce::roundToInt (comp->getHeight() * scale));
            }
        },
        false);
}

void Compositor::setFrameRate (int fps)
{
    background.uniform.setFrameRate (fps);
}

//==============================================================================
std::unique_ptr<juce::OpenGLShaderProgram> Compositor::createProgram (juce::StringRef shaderSource)
{
    auto p { std::make_unique<juce::OpenGLShaderProgram> (context) };

    if (p->addVertexShader (screenQuad) and p->addFragmentShader (shaderSource) and p->link())
    {
        return p;
    }

    if (reportError)
        reportError (p->getLastError());

    return nullptr;
}

void Compositor::renderOutput (Quad& quad, int screenWidth, int screenHeight)
{
    using namespace ::juce::gl;

    jassert (outputProgram != nullptr);

    glViewport (0, 0, screenWidth, screenHeight);
    glEnable (GL_BLEND);
    glBlendFunc (GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    outputProgram->use();
    outputProgram->setUniform (IDref::iOpacity, background.uniform.opacity);

    glActiveTexture (GL_TEXTURE0);
    glBindTexture (GL_TEXTURE_2D, background.getOutputTextureID());
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, background.uniform.textureFilter);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, background.uniform.textureFilter);
    outputProgram->setUniform (IDref::outputTexture, 0);
    quad.draw();

    glBindTexture (GL_TEXTURE_2D, 0);
    glDisable (GL_BLEND);
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace graphics
