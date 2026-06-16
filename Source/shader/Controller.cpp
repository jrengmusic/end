#include "shader/Controller.h"

namespace shader
{
/*____________________________________________________________________________*/

// GL_VIEWPORT returns 4 GLints: x, y, width, height — indices 2,3 are in-bounds by spec.
static constexpr int viewportWidth  { 2 };
static constexpr int viewportHeight { 3 };

//==============================================================================
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
    shaderConfig.state.addListener (this);

    openGLContext.setOpenGLVersionRequired (juce::OpenGLContext::openGL3_2);
    openGLContext.setComponentPaintingEnabled (true);
    openGLContext.setContinuousRepainting (true);
    openGLContext.setRenderer (this);
    openGLContext.attachTo (component);

    valueTreePropertyChanged (shaderConfig.state, IDtype::shaders);
}

void Controller::detach()
{
    shaderConfig.state.removeListener (this);
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
    for (auto& [id, pass] : passes)
        if (pass.fbo != nullptr)
            pass.fbo->release();

    passes.clear();
    quad.destroy();
}

void Controller::render()
{
    using namespace juce::gl;

    GLint viewport[4];
    glGetIntegerv (GL_VIEWPORT, viewport);

    const int   width     { viewport[viewportWidth] };
    const int   height    { viewport[viewportHeight] };
    const float fWidth    { static_cast<float> (width) };
    const float fHeight   { static_cast<float> (height) };
    const float time      { static_cast<float> (frameCounter) / 60.0f };
    const float timeDelta { 1.0f / 60.0f };

    // Bind buffer FBO textures to iChannel texture units
    {
        int unit { 0 };

        for (auto& [key, stem] : config::File::Shaders::get())
        {
            if (key != config::File::Shaders::common and key != config::File::Shaders::image)
            {
                if (passes.contains (juce::Identifier { stem }))
                {
                    auto& bufferPass { passes.at (juce::Identifier { stem }) };

                    if (bufferPass.fbo != nullptr and bufferPass.fbo->isValid())
                    {
                        glActiveTexture (GL_TEXTURE0 + static_cast<GLenum> (unit));
                        glBindTexture (GL_TEXTURE_2D, bufferPass.fbo->getTextureID());
                    }
                }

                ++unit;
            }
        }
    }

    // Render each pass in bimap order
    for (auto& [key, stem] : config::File::Shaders::get())
    {
        if (key != config::File::Shaders::common)
        {
            juce::Identifier passId { stem };

            if (passes.contains (passId))
            {
                auto& pass { passes.at (passId) };

                if (pass.fbo != nullptr)
                {
                    if (pass.fbo->getWidth() != width or pass.fbo->getHeight() != height)
                        pass.fbo->initialise (openGLContext, width, height);

                    pass.fbo->makeCurrentRenderingTarget();
                }

                glViewport (0, 0, width, height);
                juce::OpenGLHelpers::clear (juce::Colours::transparentBlack);

                pass.program->use();
                pass.setUniforms (fWidth, fHeight, time, timeDelta, frameCounter);
                quad.draw();

                if (pass.fbo != nullptr)
                    pass.fbo->releaseAsRenderingTarget();
            }
        }
    }
}

//==============================================================================
void Controller::valueTreePropertyChanged (juce::ValueTree& tree,
                                           const juce::Identifier& property)
{
    if (property == IDtype::shaders)
    {
        // Read Common source from tree (message thread)
        juce::String commonSource;

        for (auto child : tree)
        {
            if (child.getType() == ID::common)
                commonSource = child.getProperty (jam::ID::value).toString();
        }

        // Assemble fragment sources for each non-Common child
        jam::HashMap<juce::Identifier, juce::String> sources;

        for (auto child : tree)
        {
            if (child.getType() != ID::common)
            {
                auto passSource { child.getProperty (jam::ID::value).toString() };
                auto fragment { jam::Format::replaceholder (wrapper, "source", commonSource + passSource) };
                sources.insert_or_assign (child.getType(), fragment);
            }
        }

        openGLContext.executeOnGLThread (
            [this, sources] (juce::OpenGLContext&)
            {
                loadShaders (sources);
            }, false);
    }
}

void Controller::loadShaders (const jam::HashMap<juce::Identifier, juce::String>& sources)
{
    for (auto& [id, pass] : passes)
        if (pass.fbo != nullptr)
            pass.fbo->release();

    passes.clear();

    for (auto& [passId, fragment] : sources)
    {
        juce::String error;
        auto program { Compiler::build (vertexSource, fragment, openGLContext, error) };

        if (program != nullptr)
        {
            Pass pass;
            pass.setUniforms = Compiler::buildUniformSetter (*program);
            pass.program     = std::move (program);

            // Buffer passes (not Image) get FBOs
            if (passId != ID::image)
            {
                pass.fbo = std::make_unique<juce::OpenGLFrameBuffer>();

                GLint viewport[4];
                juce::gl::glGetIntegerv (juce::gl::GL_VIEWPORT, viewport);
                pass.fbo->initialise (openGLContext, viewport[viewportWidth], viewport[viewportHeight]);
            }

            passes.insert_or_assign (passId, std::move (pass));
        }
    }
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace shader
