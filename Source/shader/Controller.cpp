#include "shader/Controller.h"

namespace shader
{
/*____________________________________________________________________________*/

Controller::~Controller()
{
    shutdownOpenGL();
}

//==============================================================================
void Controller::shutdownOpenGL()
{
    openGLContext.detach();
}

void Controller::attach (juce::Component& component)
{
    openGLContext.setComponentPaintingEnabled (true);
    openGLContext.setContinuousRepainting (false);
    openGLContext.setRenderer (this);
    openGLContext.attachTo (component);
}

void Controller::detach()
{
    openGLContext.detach();
}

bool Controller::isAttached() const noexcept
{
    return openGLContext.isAttached();
}

//==============================================================================
void Controller::newOpenGLContextCreated()
{
    initialise();
}

void Controller::renderOpenGL()
{
    ++frameCounter;
    render();
}

void Controller::openGLContextClosing()
{
    shutdown();
}

//==============================================================================
void Controller::initialise()
{
#if JUCE_MAC or JUCE_WINDOWS
    jam::BackgroundBlur::enableWindowTransparency();
#endif

    quad.create();
}

void Controller::shutdown()
{
    programs.clear();
    uniforms.clear();
    quad.destroy();
}

void Controller::render()
{
    using namespace juce::gl;

    juce::OpenGLHelpers::clear (juce::Colours::transparentBlack);
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace shader
