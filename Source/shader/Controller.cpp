#include "Controller.h"

namespace shader
{
/*____________________________________________________________________________*/

Controller::Controller()
{
    registerEvents();
    // config.addListener (this);
    appModel.addListener (this);
}

Controller::~Controller()
{
    appModel.removeListener (this);
    // config.removeListener (this);
    shutdownOpenGL();
}

//==============================================================================
void Controller::shutdownOpenGL() { context.detach(); }

void Controller::attach (juce::Component& component)
{
    context.setOpenGLVersionRequired (juce::OpenGLContext::openGL4_1);
    context.setContinuousRepainting (true);
    context.setMultisamplingEnabled (true);
    context.setRenderer (this);
    context.attachTo (component);
    loadShaders();
}

void Controller::detach() { context.detach(); }

bool Controller::isAttached() const noexcept { return context.isAttached(); }

//==============================================================================
void Controller::newOpenGLContextCreated() { initialise(); }

void Controller::renderOpenGL()
{
    using namespace ::juce::gl;

    uniform.advance();
    auto [w, h] = uniform.getSize();

    for (auto& [id, pass] : programs)
    {
        if (pass->buffer.has_value())
        {
            pass->writeBuffer().makeCurrentAndClear();
            glViewport (0, 0, w, h);

            pass->program->use();
            setChannels (*pass->program);
            uniform.set (*pass->program);
            quad->draw();

            pass->writeBuffer().releaseAsRenderingTarget();
            pass->swap();
        }
    }

    juce::OpenGLHelpers::clear (juce::Colours::transparentBlack);
    glViewport (0, 0, w, h);

    if (programs.contains (ID::image))
    {
        auto& image { programs.at (ID::image) };
        image->program->use();
        setChannels (*image->program);
        uniform.set (*image->program);
        quad->draw();
    }

    unbindChannels();
}

void Controller::openGLContextClosing() { shutdown(); }

//==============================================================================
void Controller::initialise()
{
#if JUCE_MAC or JUCE_WINDOWS
    jam::BackgroundBlur::enableWindowTransparency();
#endif

    quad = std::make_unique<Quad>();
    uniform.lastFrameTime = static_cast<int> (juce::Time::getMillisecondCounterHiRes());
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

void Controller::resize (int w, int h)
{
    uniform.resize (w, h);

    for (auto& [id, shader] : programs)
        shader->resize (context, w, h);
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
            program.setUniform (channelName.toRawUTF8(), id);
        }
    }
}

void Controller::unbindChannels()
{
    using namespace ::juce::gl;

    for (int i { 3 }; i >= 0; --i)
    {
        glActiveTexture (GL_TEXTURE0 + i);
        glBindTexture (GL_TEXTURE_2D, 0);
    }
}

void Controller::loadShaders()
{
    // context.executeOnGLThread (
    //     [this] (juce::OpenGLContext&)
    //     {
    //         programs.clear();
    //
    //         juce::String common { config.getValue (IDtype::shader, ID::common) };
    //         auto shader { config.getChildWithName (IDtype::shader) };
    //
    //         jam::Model::forEachProperty (
    //             shader,
    //             [common, this] (const juce::Identifier& id, const juce::var& var)
    //             {
    //                 if (id != ID::common and files.contains (id.toString()))
    //                 {
    //                     if (auto code { var.toString() }; code.isNotEmpty())
    //                     {
    //                         if (common.isNotEmpty())
    //                             code = jam::Format::prependNewLine (code, common);
    //
    //                         code = jam::Format::replaceholder (wrapper, placeholder, code);
    //
    //                         if (auto p { createProgram (code) })
    //                         {
    //                             auto pass { std::make_unique<Pass>() };
    //                             pass->program = std::move (p);
    //
    //                             if (id != ID::image)
    //                                 pass->buffer.emplace();
    //
    //                             programs.addOrReplace (id, std::move (pass));
    //                         }
    //                     }
    //                 }
    //             });
    //
    //         auto* comp { context.getTargetComponent() };
    //         jassert (comp != nullptr);
    //
    //         auto scale { context.getRenderingScale() };
    //         resize (juce::roundToInt (comp->getWidth() * scale),
    //                 juce::roundToInt (comp->getHeight() * scale));
    //     },
    //     false);
}

void Controller::registerEvents()
{
    // events.add<const juce::var&> (ID::background,
    //                               [this] (const juce::var&)
    //                               {
    //                                   loadShaders();
    //                               });
    //
    // events.add<const juce::var&> (ID::size,
    //                               [this] (const juce::var& newValue)
    //                               {
    //                                   end::Size size { static_cast<int> (newValue) };
    //                                   auto [w, h] = size;
    //                                   resizer.set (IDtype::view, w, h);
    //                               });
    //
    // resizer.addTrigger (juce::Identifier { jam::ID::stop },
    //                     [this] (int w, int h)
    //                     {
    //                         context.executeOnGLThread (
    //                             [this, w, h] (juce::OpenGLContext&)
    //                             {
    //                                 const auto scale { context.getRenderingScale() };
    //                                 int scaledW { juce::roundToInt (w * scale) };
    //                                 int scaledH { juce::roundToInt (h * scale) };
    //                                 resize (scaledW, scaledH);
    //                             },
    //                             false);
    //                     });
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace shader
