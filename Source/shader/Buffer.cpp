#include "shader/Buffer.h"

namespace shader
{
/*____________________________________________________________________________*/

using namespace juce::gl;

bool Buffer::resize (juce::OpenGLContext& context, int width, int height)
{
    fbos[0].release();
    fbos[1].release();

    bool const ok0 = fbos[0].initialise (context, width, height);
    bool const ok1 = fbos[1].initialise (context, width, height);

    fbos[0].makeCurrentAndClear();
    fbos[0].releaseAsRenderingTarget();

    fbos[1].makeCurrentAndClear();
    fbos[1].releaseAsRenderingTarget();

    return ok0 and ok1;
}

void Buffer::render (Quad& quad, int width, int height,
                     float time, float timeDelta, int frame,
                     const float mouse[4])
{
    fbos[writeIndex].makeCurrentRenderingTarget();
    juce::OpenGLHelpers::clear (juce::Colours::transparentBlack);
    glViewport (0, 0, width, height);
    pass.render (quad, width, height, time, timeDelta, frame, mouse);
    fbos[writeIndex].releaseAsRenderingTarget();
    writeIndex = 1 - writeIndex;
}

GLuint Buffer::getReadTexture() const noexcept
{
    return fbos[1 - writeIndex].getTextureID();
}

void Buffer::release()
{
    fbos[0].release();
    fbos[1].release();
    pass.release();
    writeIndex = 0;
}

/**______________________________END OF NAMESPACE______________________________*/
} // namespace shader
