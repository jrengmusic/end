#include "shader/Controller.h"

namespace shader
{
/*____________________________________________________________________________*/

Controller::~Controller()
{
    detach();
}

//==============================================================================
void Controller::attach (juce::Component& component)
{
    context.setComponentPaintingEnabled (true);
    context.setContinuousRepainting (false);
    context.setRenderer (this);
    context.attachTo (component);
}

void Controller::detach()
{
    context.detach();
}

bool Controller::isAttached() const noexcept
{
    return context.isAttached();
}

void Controller::resize (int width, int height)
{
    pendingWidth.store (width);
    pendingHeight.store (height);
    context.triggerRepaint();
}

//==============================================================================
void Controller::newOpenGLContextCreated()
{
#if JUCE_MAC || JUCE_WINDOWS
    jam::BackgroundBlur::enableWindowTransparency();
#endif

    quad.create (context);

    auto now = static_cast<float> (juce::Time::getMillisecondCounterHiRes() * 0.001);
    startTime     = now;
    lastFrameTime = now;
    frameCount    = 0;
}

void Controller::openGLContextClosing()
{
    background.release();

    for (auto& b : buffers)
        b.release();

    quad.destroy();
}

void Controller::renderOpenGL()
{
    using namespace juce::gl;

    int pw = pendingWidth.exchange (0);
    int ph = pendingHeight.exchange (0);

    if (pw > 0 and ph > 0)
    {
        viewportWidth  = pw;
        viewportHeight = ph;

        for (auto& b : buffers)
        {
            if (b.isLoaded())
                b.resize (context, viewportWidth, viewportHeight);
        }
    }

    if (viewportWidth <= 0 or viewportHeight <= 0)
        return;

    // Timing
    auto  now       = static_cast<float> (juce::Time::getMillisecondCounterHiRes() * 0.001);
    float time      = now - startTime;
    float timeDelta = now - lastFrameTime;
    lastFrameTime   = now;

    // Clear to transparent — glass shows through
    juce::OpenGLHelpers::clear (juce::Colours::transparentBlack);
    glViewport (0, 0, viewportWidth, viewportHeight);

    // Multipass buffers
    for (size_t i = 0; i < 4; ++i)
    {
        if (buffers[i].isLoaded())
            buffers[i].render (quad, viewportWidth, viewportHeight, time, timeDelta, frameCount, mouse);
    }

    // Background shader — renders behind JUCE components
    if (background.isLoaded())
        background.render (quad, viewportWidth, viewportHeight, time, timeDelta, frameCount, mouse);

    ++frameCount;
}

/**______________________________END OF NAMESPACE______________________________*/
} // namespace shader
