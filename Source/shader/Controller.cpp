#include "Controller.h"

namespace shader
{
/*____________________________________________________________________________*/

Controller::Controller()
{
    registerEvents();
    appModel.addListener (this);
}

Controller::~Controller()
{
    appModel.removeListener (this);
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
}

void Controller::detach() { context.detach(); }

bool Controller::isAttached() const noexcept { return context.isAttached(); }

//==============================================================================
void Controller::newOpenGLContextCreated() { initialise(); }

void Controller::renderOpenGL()
{
    using namespace ::juce::gl;

    juce::OpenGLHelpers::clear (juce::Colours::transparentBlack);

    auto scale { context.getRenderingScale() };
    auto* comp { context.getTargetComponent() };

    if (comp != nullptr)
    {
        int w { juce::roundToInt (comp->getWidth() * scale) };
        int h { juce::roundToInt (comp->getHeight() * scale) };
        glViewport (0, 0, w, h);
    }

    if (quad != nullptr)
        quad->draw();

    //==============================================================================
    ++frameCounter;
}

void Controller::openGLContextClosing() { shutdown(); }

//==============================================================================
void Controller::initialise()
{
#if JUCE_MAC or JUCE_WINDOWS
    jam::BackgroundBlur::enableWindowTransparency();
#endif

    quad = std::make_unique<Quad>();
}

void Controller::shutdown() {}

//==============================================================================
void Controller::parameterChanged (const juce::Identifier& id, const juce::var&)
{
    if (events.contains (id))
        events.get (id);
}

//==============================================================================

void Controller::registerEvents()
{
    auto loadShaders = [this]
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
                            juce::String shaderSourceCode { jam::Format::prependNewLine (
                                var.toString(), common) };
                            shaderSourceCode =
                                jam::Format::replaceholder (wrapper, placeholder, shaderSourceCode);

                            auto p { std::make_unique<juce::OpenGLShaderProgram> (context) };

                            if (p->addVertexShader (screenQuad)
                                and p->addFragmentShader (shaderSourceCode) and p->link())
                            {
                                auto program { std::make_unique<Program>() };
                                program->p = std::move (p);
                                programs.insert_or_assign (id, std::move (program));
                            }
                            else
                            {
                                auto error { p->getLastError() };
                                appModel.setMessage (error);
                            }
                        }
                    });
            },
            false);
    };

    events.add (IDtype::shader, loadShaders);
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace shader
