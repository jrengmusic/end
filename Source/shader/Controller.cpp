#include "Controller.h"

namespace shader
{
/*____________________________________________________________________________*/

Controller::Controller()
{
    registerEvents();
    config.addListener (this);
    appModel.addListener (this);
}

Controller::~Controller()
{
    stopTimer();
    appModel.removeListener (this);
    config.removeListener (this);
    shutdownOpenGL();
}

//==============================================================================
void Controller::shutdownOpenGL() { context.detach(); }

void Controller::attach (juce::Component& component)
{
    context.setOpenGLVersionRequired (juce::OpenGLContext::openGL4_1);
    context.setContinuousRepainting (false);
    context.setMultisamplingEnabled (true);
    context.setRenderer (this);
    context.attachTo (component);
    refreshParameters();
}

void Controller::detach() { context.detach(); }

bool Controller::isAttached() const noexcept { return context.isAttached(); }

//==============================================================================
void Controller::newOpenGLContextCreated() { initialise(); }

void Controller::renderOpenGL()
{
    background.uniform.advance();
    postProcess.uniform.advance();

    if (postProcess.isCompiled())
    {
        using namespace ::juce::gl;

        auto [screenWidth, screenHeight] = background.uniform.getScreenSize();

        // Render background fresh into composite writeBuffer — no feedback from previous post-pro.
        background.renderBuffers (*quad);
        background.renderImage (*quad, &backgroundPass[0]);

        composite.writeBuffer().makeCurrentAndClear();
        glViewport (0, 0, screenWidth, screenHeight);
        glEnable (GL_BLEND);
        glBlendFunc (GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

        outputProgram->use();
        outputProgram->setUniform (IDref::iOpacity, background.uniform.opacity);

        glActiveTexture (GL_TEXTURE0);
        glBindTexture (GL_TEXTURE_2D, backgroundPass[0].getTextureID());
        glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, background.uniform.textureFilter);
        glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, background.uniform.textureFilter);
        outputProgram->setUniform (IDref::outputTexture, 0);

        quad->draw();

        glBindTexture (GL_TEXTURE_2D, 0);
        glDisable (GL_BLEND);

        // Blit JUCE component painting from FB 0 (previous frame) onto composite writeBuffer.
        glBindFramebuffer (GL_READ_FRAMEBUFFER, 0);
        glBlitFramebuffer (0,
                           0,
                           screenWidth,
                           screenHeight,
                           0,
                           0,
                           screenWidth,
                           screenHeight,
                           GL_COLOR_BUFFER_BIT,
                           GL_NEAREST);
        composite.writeBuffer().releaseAsRenderingTarget();
        composite.swap();

        // Post-process the composite (bg + components) → FB 0.
        glBindFramebuffer (GL_FRAMEBUFFER, 0);
        juce::OpenGLHelpers::clear (juce::Colours::transparentBlack);
        glViewport (0, 0, screenWidth, screenHeight);

        postProcess.sceneTexture = composite.readBuffer().getTextureID();
        postProcess.renderBuffers (*quad);
        postProcess.renderImage (*quad, nullptr);
        postProcess.sceneTexture = 0;
    }
    else
    {
        // No post-processing — current behavior.
        background.renderBuffers (*quad);
        background.renderImage (*quad, &backgroundPass[0]);
        renderOutput();
    }
}

void Controller::renderOutput()
{
    using namespace ::juce::gl;

    jassert (outputProgram != nullptr);

    auto [screenWidth, screenHeight] = background.uniform.getScreenSize();
    juce::OpenGLHelpers::clear (juce::Colours::transparentBlack);
    glViewport (0, 0, screenWidth, screenHeight);
    glEnable (GL_BLEND);
    glBlendFunc (GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    outputProgram->use();
    outputProgram->setUniform (IDref::iOpacity, background.uniform.opacity);

    glActiveTexture (GL_TEXTURE0);
    glBindTexture (GL_TEXTURE_2D, backgroundPass[0].getTextureID());
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, background.uniform.textureFilter);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, background.uniform.textureFilter);
    outputProgram->setUniform (IDref::outputTexture, 0);

    quad->draw();

    glBindTexture (GL_TEXTURE_2D, 0);
    glDisable (GL_BLEND);
}

void Controller::openGLContextClosing() { shutdown(); }

void Controller::timerCallback() { context.triggerRepaint(); }

//==============================================================================
void Controller::initialise()
{
#if JUCE_MAC or JUCE_WINDOWS
    jam::BackgroundBlur::enableWindowTransparency();
#endif

    quad = std::make_unique<Quad>();

    outputProgram = createProgram (outputShader);
    jassert (outputProgram != nullptr);

    postProcess.uniform.resolutionScale = 1.0f;
}

void Controller::shutdown()
{
    background.shutdown();
    postProcess.shutdown();
    for (auto& fbo : backgroundPass)
        fbo.release();
    for (auto& fbo : composite)
        fbo.release();
    outputProgram.reset();
    quad.reset();
}

//==============================================================================
void Controller::parameterChanged (const juce::Identifier& id, const juce::var& newValue)
{
    if (events.contains (id))
        events.get (id, newValue);
}

//==============================================================================

std::unique_ptr<juce::OpenGLShaderProgram> Controller::createProgram (juce::StringRef shaderSource)
{
    auto p { std::make_unique<juce::OpenGLShaderProgram> (context) };

    if (p->addVertexShader (screenQuad) and p->addFragmentShader (shaderSource) and p->link())
    {
        return p;
    }

    appModel.setMessage (p->getLastError());
    return nullptr;
}

void Controller::resize (int screenWidth, int screenHeight)
{
    background.resize (context, screenWidth, screenHeight);

    auto [bufferWidth, bufferHeight] = background.uniform.getSize();
    backgroundPass.resize (context, bufferWidth, bufferHeight);

    postProcess.resize (context, screenWidth, screenHeight);

    if (postProcess.isCompiled())
        composite.resize (context, screenWidth, screenHeight);
}

void Controller::loadShaders (Compilation& compilation, const juce::Identifier& treeType)
{
    context.executeOnGLThread (
        [this, &compilation, treeType] (juce::OpenGLContext&)
        {
            auto shaderTree { config.getChildWithName (treeType) };

            compilation.load (shaderTree,
                              wrapper,
                              placeholder,
                              [this] (juce::StringRef source)
                              {
                                  return createProgram (source);
                              });

            auto* comp { context.getTargetComponent() };
            jassert (comp != nullptr);

            auto scale { context.getRenderingScale() };
            resize (juce::roundToInt (comp->getWidth() * scale),
                    juce::roundToInt (comp->getHeight() * scale));
        },
        false);
}

void Controller::refreshParameters()
{
    const auto& graphics { config.getChildWithName (IDtype::graphics) };

    jam::Model::forEachProperty (graphics,
                                 [this] (const juce::Identifier& id, const juce::var& newValue)
                                 {
                                     parameterChanged (id, newValue);
                                 });
}

void Controller::registerEvents()
{
    events.add<const juce::var&> (ID::background,
                                  [this] (const juce::var&)
                                  {
                                      loadShaders (background, IDtype::background);
                                  });

    events.add<const juce::var&> (ID::postProcessing,
                                  [this] (const juce::var&)
                                  {
                                      loadShaders (postProcess, IDtype::postProcessing);
                                  });

    events.add<const juce::var&> (ID::size,
                                  [this] (const juce::var& newValue)
                                  {
                                      end::Size size { static_cast<int> (newValue) };
                                      auto [w, h] = size;
                                      resizer.set (IDtype::view, w, h);
                                  });

    resizer.addTrigger (
        juce::Identifier { jam::ID::stop },
        [this] (int width, int height)
        {
            context.executeOnGLThread (
                [this, width, height] (juce::OpenGLContext&)
                {
                    const auto scale { context.getRenderingScale() };
                    resize (juce::roundToInt (width * scale), juce::roundToInt (height * scale));
                },
                false);
        });

    events.add<const juce::var&> (ID::frameRate,
                                  [this] (const juce::var& newValue)
                                  {
                                      int fps { static_cast<int> (newValue) };
                                      startTimerHz (fps);
                                      background.uniform.setFrameRate (fps);
                                      postProcess.uniform.setFrameRate (fps);
                                  });

    events.add<const juce::var&> (ID::resolutionScale,
                                  [this] (const juce::var& newValue)
                                  {
                                      background.uniform.resolutionScale =
                                          static_cast<float> (newValue);
                                      auto screen { background.uniform.getScreenSize() };

                                      if (screen.x > 0 and screen.y > 0)
                                      {
                                          context.executeOnGLThread (
                                              [this, screen] (juce::OpenGLContext&)
                                              {
                                                  resize (screen.x, screen.y);
                                              },
                                              false);
                                      }
                                  });

    events.add<const juce::var&> (ID::filter,
                                  [this] (const juce::var& newValue)
                                  {
                                      background.uniform.textureFilter =
                                          end::Filter::get (newValue.toString());
                                  });

    events.add<const juce::var&> (ID::backgroundOpacity,
                                  [this] (const juce::var& newValue)
                                  {
                                      background.uniform.opacity = static_cast<float> (newValue);
                                  });
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace shader
