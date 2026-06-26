#include "Compositor.h"

namespace graphics
{
/*____________________________________________________________________________*/

void Compositor::prepare (juce::OpenGLContext& ctx)
{
    context = &ctx;

    outputProgram = createProgram (outputShader);
    jassert (outputProgram != nullptr);

    postProcess.uniform.resolutionScale = 1.0f;
}

void Compositor::process (Quad& quad)
{
    background.uniform.advance();
    postProcess.uniform.advance();

    background.renderBuffers (quad);
    background.renderImage (quad, &backgroundPass[0]);

    if (postProcess.isCompiled())
        compositeScene (quad);

    juce::OpenGLHelpers::clear (juce::Colours::transparentBlack);

    if (postProcess.isCompiled())
        renderPostProcess (quad);
    else
        renderOutput (quad);
}

void Compositor::reset()
{
    background.shutdown();
    postProcess.shutdown();

    backgroundPass.release();
    sceneCapture.release();

    outputProgram.reset();
}

//==============================================================================
void Compositor::loadShaders (const juce::Identifier& treeType)
{
    jassert (context != nullptr);

    auto& compilation = (treeType == IDtype::background) ? background : postProcess;
    auto shaderTree { config.getChildWithName (treeType) };

    compilation.load (shaderTree,
                      wrapper,
                      placeholder,
                      [this] (juce::StringRef source)
                      {
                          return createProgram (source);
                      });
}

void Compositor::resize (int screenWidth, int screenHeight)
{
    jassert (context != nullptr);

    background.resize (*context, screenWidth, screenHeight);

    auto [bufferWidth, bufferHeight] = background.uniform.getSize();
    backgroundPass.resize (*context, bufferWidth, bufferHeight);

    postProcess.resize (*context, screenWidth, screenHeight);

    if (postProcess.isCompiled())
        sceneCapture.resize (*context, screenWidth, screenHeight);
}

//==============================================================================
void Compositor::setOpacity (float value) { background.uniform.opacity = value; }
void Compositor::setTextureFilter (GLenum value) { background.uniform.textureFilter = value; }

void Compositor::setResolutionScale (float value) { background.uniform.resolutionScale = value; }

void Compositor::setFrameRate (int fps)
{
    background.uniform.setFrameRate (fps);
    postProcess.uniform.setFrameRate (fps);
}

//==============================================================================
std::unique_ptr<juce::OpenGLShaderProgram> Compositor::createProgram (juce::StringRef shaderSource)
{
    jassert (context != nullptr);

    auto p { std::make_unique<juce::OpenGLShaderProgram> (*context) };

    if (p->addVertexShader (screenQuad) and p->addFragmentShader (shaderSource) and p->link())
    {
        return p;
    }

    appModel.setMessage (p->getLastError());
    return nullptr;
}

void Compositor::renderOutput (Quad& quad)
{
    using namespace ::juce::gl;

    jassert (outputProgram != nullptr);

    auto [screenWidth, screenHeight] = background.uniform.getScreenSize();
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

    quad.draw();

    glBindTexture (GL_TEXTURE_2D, 0);
    glDisable (GL_BLEND);
}

void Compositor::compositeScene (Quad& quad)
{
    using namespace ::juce::gl;

    auto [screenWidth, screenHeight] = background.uniform.getScreenSize();

    // Capture FB 0 (previous frame's JUCE component painting) into sceneCapture first.
    sceneCapture[0].makeCurrentAndClear();
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

    // Render background on top of captured JUCE components with blend.
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

    quad.draw();

    glBindTexture (GL_TEXTURE_2D, 0);
    glDisable (GL_BLEND);
    sceneCapture[0].releaseAsRenderingTarget();
}

void Compositor::renderPostProcess (Quad& quad)
{
    using namespace ::juce::gl;

    auto [screenWidth, screenHeight] = background.uniform.getScreenSize();

    glBindFramebuffer (GL_FRAMEBUFFER, 0);
    glViewport (0, 0, screenWidth, screenHeight);

    postProcess.sceneTexture = sceneCapture[0].getTextureID();
    postProcess.renderBuffers (quad);
    postProcess.renderImage (quad, nullptr);
    postProcess.sceneTexture = 0;
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace graphics
