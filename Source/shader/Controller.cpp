#include "Controller.h"

namespace shader
{
/*____________________________________________________________________________*/

//==============================================================================
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
    // context.setComponentPaintingEnabled (true);
    // context.setContinuousRepainting (true);
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
    juce::OpenGLHelpers::clear (juce::Colours::transparentBlack);

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
                    p->addFragmentShader (shaderSourceCode);

                    auto program { std::make_unique<Program>() };
                    program->p = std::move (p);

                    programs.insert_or_assign (id, std::move (program));
                }
            });
    };

    events.add (IDtype::shader, loadShaders);
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace shader
