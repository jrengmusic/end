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
    appModel.removeListener (this);
    config.removeListener (this);
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

    juce::OpenGLHelpers::clear (juce::Colours::transparentBlack);

    uniform.advance();
    auto [w, h] = uniform.getSize();
    glViewport (0, 0, w, h);

    if (programs.contains (ID::image))
    {
        auto& image { programs.at (ID::image) };
        image->program->use();
        uniform.set (*image->program);
        quad->draw();
    }
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
void Controller::parameterChanged (const juce::Identifier& id, const juce::var&)
{
    if (events.contains (id))
        events.get (id);
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

void Controller::loadShaders()
{
    context.executeOnGLThread (
        [this] (juce::OpenGLContext&)
        {
            auto shaders { config.getChildWithName (IDtype::shader) };
            juce::String common { shaders.getProperty (ID::common).toString() };

            jam::Model::forEachProperty (
                shaders,
                [common, this] (const juce::Identifier& id, const juce::var& var)
                {
                    if (files.contains (id.toString()))
                    {
                        if (var.toString().isEmpty())
                            return;

                        juce::String shaderSourceCode { jam::Format::prependNewLine (
                            var.toString(), common) };
                        shaderSourceCode =
                            jam::Format::replaceholder (wrapper, placeholder, shaderSourceCode);

                        if (auto p { createProgram (shaderSourceCode) })
                        {
                            auto pass { std::make_unique<Pass>() };
                            pass->program = std::move (p);

                            if (id.toString() != IDref::image)
                                pass->buffer.emplace();

                            programs.insert_or_assign (id, std::move (pass));
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

void Controller::registerEvents()
{
    for (auto& [key, value] : config::Shaders::get())
        events.add (juce::Identifier { value },
                    [this]
                    {
                        loadShaders();
                    });

    events.add (ID::size,
                [this]
                {
                    end::Size size { appModel.getValue (IDtype::view, ID::size) };
                    auto [w, h] = size;
                    resizer.set (IDtype::view, w, h);
                });

    resizer.addTrigger (juce::Identifier { jam::ID::stop },
                        [this] (int w, int h)
                        {
                            context.executeOnGLThread (
                                [this, w, h] (juce::OpenGLContext&)
                                {
                                    auto scale { context.getRenderingScale() };
                                    int scaledW { juce::roundToInt (w * scale) };
                                    int scaledH { juce::roundToInt (h * scale) };
                                    resize (scaledW, scaledH);
                                },
                                false);
                        });
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace shader
