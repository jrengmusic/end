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
    uniform.advance();
    auto [bufferWidth, bufferHeight] = uniform.getSize();

    renderBuffers (bufferWidth, bufferHeight);
    renderImage (bufferWidth, bufferHeight);
    renderOutput();
}

void Controller::renderBuffers (int bufferWidth, int bufferHeight)
{
    using namespace ::juce::gl;

    for (auto& [id, pass] : programs)
    {
        if (pass->buffer.has_value() and id != ID::output)
        {
            pass->writeBuffer().makeCurrentAndClear();
            glViewport (0, 0, bufferWidth, bufferHeight);

            pass->program->use();
            setChannels (*pass->program);
            uniform.set (*pass->program);
            quad->draw();

            pass->writeBuffer().releaseAsRenderingTarget();
            pass->swap();
        }
    }
}

void Controller::renderImage (int bufferWidth, int bufferHeight)
{
    using namespace ::juce::gl;

    if (programs.contains (ID::image) and programs.contains (ID::output))
    {
        auto& outputPass { programs.at (ID::output) };
        jassert (outputPass->buffer.has_value());

        (*outputPass->buffer).at (0).makeCurrentAndClear();
        glViewport (0, 0, bufferWidth, bufferHeight);

        auto& image { programs.at (ID::image) };
        image->program->use();
        setChannels (*image->program);
        uniform.set (*image->program);
        quad->draw();

        (*outputPass->buffer).at (0).releaseAsRenderingTarget();
    }
}

void Controller::renderOutput()
{
    using namespace ::juce::gl;

    jassert (programs.contains (ID::output));

    auto& output { programs.at (ID::output) };
    jassert (output->program != nullptr and output->buffer.has_value());

    auto [screenWidth, screenHeight] = uniform.getScreenSize();
    juce::OpenGLHelpers::clear (juce::Colours::transparentBlack);
    glViewport (0, 0, screenWidth, screenHeight);
    glEnable (GL_BLEND);
    glBlendFunc (GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    output->program->use();
    output->program->setUniform (IDref::iOpacity, uniform.opacity);

    glActiveTexture (GL_TEXTURE0);
    glBindTexture (GL_TEXTURE_2D, (*output->buffer).at (0).getTextureID());
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, uniform.textureFilter);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, uniform.textureFilter);
    output->program->setUniform (IDref::outputTexture, 0);

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

    auto output { std::make_unique<Pass>() };
    output->program = createProgram (outputShader);
    jassert (output->program != nullptr);
    output->buffer.emplace();
    programs.addOrReplace (ID::output, std::move (output));
}

void Controller::shutdown()
{
    programs.clear();
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
    uniform.resize (screenWidth, screenHeight);
    auto [bufferWidth, bufferHeight] = uniform.getSize();

    for (auto& [id, shader] : programs)
        shader->resize (context, bufferWidth, bufferHeight);
}

void Controller::setChannels (juce::OpenGLShaderProgram& program)
{
    using namespace ::juce::gl;

    for (auto& [id, channelName] : file::BufferChannel::get())
    {
        juce::Identifier passId { file::Shaders::get().at (id) };

        if (programs.contains (passId) and programs.at (passId)->buffer.has_value())
        {
            glActiveTexture (GL_TEXTURE0 + id);
            glBindTexture (GL_TEXTURE_2D, programs.at (passId)->readBuffer().getTextureID());
            glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, uniform.textureFilter);
            glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, uniform.textureFilter);
            program.setUniform (channelName.toRawUTF8(), id);
        }
    }
}

void Controller::loadShaders()
{
    context.executeOnGLThread (
        [this] (juce::OpenGLContext&)
        {
            programs.clear();

            // Output pass — infrastructure, not config-driven. Recreate after clear.
            {
                auto output { std::make_unique<Pass>() };
                output->program = createProgram (outputShader);
                jassert (output->program != nullptr);
                output->buffer.emplace();
                programs.addOrReplace (ID::output, std::move (output));
            }

            juce::String common { config.getValue (IDtype::shader, ID::common) };
            auto shader { config.getChildWithName (IDtype::shader) };

            jam::Model::forEachProperty (
                shader,
                [common, this] (const juce::Identifier& id, const juce::var& var)
                {
                    if (id != ID::common and files.contains (id.toString()))
                    {
                        if (auto code { var.toString() }; code.isNotEmpty())
                        {
                            if (common.isNotEmpty())
                                code = jam::Format::prependNewLine (code, common);

                            code = jam::Format::replaceholder (wrapper, placeholder, code);

                            if (auto p { createProgram (code) })
                            {
                                auto pass { std::make_unique<Pass>() };
                                pass->program = std::move (p);

                                if (id != ID::image)
                                    pass->buffer.emplace();

                                programs.addOrReplace (id, std::move (pass));
                            }
                        }
                    }
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
                                     if (events.contains (id))
                                         events.get (id, newValue);
                                 });
}

void Controller::registerEvents()
{
    events.add<const juce::var&> (ID::background,
                                  [this] (const juce::var&)
                                  {
                                      loadShaders();
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
                                      uniform.setFrameRate (fps);
                                  });

    events.add<const juce::var&> (ID::resolutionScale,
                                  [this] (const juce::var& newValue)
                                  {
                                      uniform.resolutionScale = static_cast<float> (newValue);
                                      auto screen { uniform.getScreenSize() };

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
                                      uniform.textureFilter =
                                          end::Filter::get (newValue.toString());
                                  });

    events.add<const juce::var&> (ID::backgroundOpacity,
                                  [this] (const juce::var& newValue)
                                  {
                                      uniform.opacity = static_cast<float> (newValue);
                                  });
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace shader
