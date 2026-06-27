/**
 * @file graphics/Program.h
 * @brief Shader pass data and per-frame uniform dispatch.
 */
#pragma once
#include <JuceHeader.h>
#include "Identifier.h"
#include "Bimap.h"
#include "end/Model.h"

namespace graphics
{
/*____________________________________________________________________________*/

/** @brief Per-frame shader uniform state and dispatch.
 *
 *  Values stored as int — viewport packed via end::Size, time as integer
 *  milliseconds, frame as count. Setters registered at init convert
 *  int values to GL uniform calls. Call site uses advance(), resize(),
 *  set(), getSize() — never touches individual keys.
 */
struct Uniform
{
    jam::HashMap<juce::Identifier, int> values {
        { ID::iResolution, 0 },
        { ID::iTime,       0 },
        { ID::iTimeDelta,  0 },
        { ID::iFrame,      0 }
    };

    jam::Function::Map<juce::Identifier, void> setters;

    int frameDelta { 1000 / 30 };
    int screenResolution { 0 };
    float resolutionScale { 1.0f };
    float opacity { 0.5f };
    GLenum textureFilter { juce::gl::GL_LINEAR };

    /** @brief Registers per-uniform conversion setters. Called once at init. */
    Uniform()
    {
        setters.add<juce::OpenGLShaderProgram&, int> (
            ID::iResolution,
            [] (juce::OpenGLShaderProgram& p, int value)
            {
                end::Size size { value };
                auto [w, h] = size;
                p.setUniform (
                    IDref::iResolution, static_cast<float> (w), static_cast<float> (h), 1.0f);
            });

        setters.add<juce::OpenGLShaderProgram&, int> (
            ID::iTime,
            [] (juce::OpenGLShaderProgram& p, int value)
            {
                p.setUniform (IDref::iTime, static_cast<float> (value) * 0.001f);
            });

        setters.add<juce::OpenGLShaderProgram&, int> (
            ID::iTimeDelta,
            [] (juce::OpenGLShaderProgram& p, int value)
            {
                p.setUniform (IDref::iTimeDelta, static_cast<float> (value) * 0.001f);
            });

        setters.add<juce::OpenGLShaderProgram&, int> (ID::iFrame,
                                                      [] (juce::OpenGLShaderProgram& p, int value)
                                                      {
                                                          p.setUniform (IDref::iFrame, value);
                                                      });
    }

    ~Uniform() = default;

    /** @brief Updates screen and buffer dimensions from pixel width and height.
     *  Buffer dimensions are screen dimensions scaled by resolutionScale.
     *  @param w  Screen viewport width in pixels.
     *  @param h  Screen viewport height in pixels.
     */
    void resize (int w, int h)
    {
        screenResolution = end::Size (w, h).toInt();
        int bw { juce::roundToInt (w * resolutionScale) };
        int bh { juce::roundToInt (h * resolutionScale) };
        values.at (ID::iResolution) = end::Size (bw, bh).toInt();
    }

    /** @brief Precomputes frame time delta from configured fps.
     *  @param fps  Target frame rate (1-120).
     */
    void setFrameRate (int fps) { frameDelta = 1000 / fps; }

    /** @brief Returns current screen dimensions by unpacking the stored screenResolution value. */
    juce::Point<int> getScreenSize() const
    {
        end::Size size { screenResolution };
        auto [w, h] = size;
        return { w, h };
    }

    /** @brief Advances per-frame time and frame counter. Called once per frame.
     *  Uses constant frameDelta — deterministic, no clock dependency.
     */
    void advance()
    {
        values.at (ID::iTimeDelta) = frameDelta;
        values.at (ID::iTime) += frameDelta;
        ++values.at (ID::iFrame);
    }

    /** @brief Dispatches all uniform values through registered setters onto the given program.
     *  @param p  The active GL program (must be use()'d first).
     */
    void set (juce::OpenGLShaderProgram& p)
    {
        for (auto& [name, value] : values)
            setters.get (name, p, value);
    }

    /** @brief Returns current viewport dimensions by unpacking the stored iResolution value. */
    juce::Point<int> getSize() const
    {
        end::Size size { values.at (ID::iResolution) };
        auto [w, h] = size;
        return { w, h };
    }

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Uniform)
};

//==============================================================================
/** @brief END Shadertoy pipeline — thin consumer of jam::OpenGL::Compilation.
 *
 *  Delegates compilation, pass management, and lifecycle to the base class.
 *  Supplies END-specific configuration via virtual getter overrides
 *  (getShaderMap, getChannelMap, getCommonKey, getWrapper, getPlaceholder).
 *  Implements render() with Uniform-driven per-frame state:
 *  advance() → renderBuffers() → renderImage().
 *
 *  Buffer passes (BufferA-D) ping-pong via swap(). Image pass renders into
 *  its own FBO — output accessible via getOutputTextureID().
 *
 *  @par Thread contract
 *  All methods must be called on the **GL THREAD** (via OpenGLRenderer callbacks
 *  or executeOnGLThread).
 */
struct Compilation : jam::OpenGL::Compilation
{
    Compilation() = default;

    Uniform uniform;

    //==============================================================================
    // Virtual getter overrides — supply END configuration to base class load()

    /** @brief Returns the Shadertoy pass name registry (pass key → pass name). */
    const jam::HashMap<int, juce::String>& getShaderMap() const override
    {
        return file::Shaders::get();
    }

    /** @brief Returns the channel binding map (buffer key → sampler uniform name). */
    const jam::HashMap<int, juce::String>& getChannelMap() const override
    {
        return file::BufferChannel::get();
    }

    /** @brief Returns the wrapper fragment shader template. */
    const juce::String& getWrapper() const override
    {
        static const juce::String source { BinaryData::getString ("wrapper.frag") };
        return source;
    }

    //==============================================================================
    /** @brief Renders all buffer passes (BufferA-D) at the configured resolution.
     *
     *  Iterates passes with buffer in HashMap insertion order. Each pass
     *  renders into its writeBuffer FBO, then swaps the ping-pong index.
     *  Image pass is skipped.
     *
     *  @param quad  Fullscreen quad for drawing.
     */
    void renderBuffers (jam::OpenGL::Quad& quad)
    {
        using namespace ::juce::gl;

        for (auto& [id, pass] : passes)
        {
            if (id != ID::image)
            {
                auto [w, h] = uniform.getSize();

                pass->writeBuffer().makeCurrentAndClear();
                glViewport (0, 0, w, h);

                pass->program->use();
                setChannels (*pass->program);
                uniform.set (*pass->program);
                quad.draw();

                pass->writeBuffer().releaseAsRenderingTarget();
                pass->swap();
            }
        }
    }

    /** @brief Renders the Image pass into its own FBO.
     *
     *  The result is accessible via getOutputTextureID().
     *  No-op if no Image pass is loaded.
     *
     *  @param quad    Fullscreen quad for drawing.
     */
    void renderImage (jam::OpenGL::Quad& quad)
    {
        using namespace ::juce::gl;

        if (passes.contains (ID::image))
        {
            auto& image { passes.at (ID::image) };
            auto [w, h] = uniform.getSize();

            image->readBuffer().makeCurrentAndClear();
            glViewport (0, 0, w, h);

            image->program->use();
            setChannels (*image->program);
            uniform.set (*image->program);
            if (sceneTexture != 0)
                image->program->setUniform (IDref::iPostOpacity, postOpacity);
            quad.draw();

            image->readBuffer().releaseAsRenderingTarget();
        }
    }

    /** @brief Advances uniforms and renders all passes (buffers + image). GL thread only.
     *  @param quad  Fullscreen quad for drawing.
     */
    void render (jam::OpenGL::Quad& quad) override
    {
        uniform.advance();
        renderBuffers (quad);
        renderImage (quad);
    }

    /** @brief Returns the GL texture ID of the Image pass output.
     *  Returns 0 if no Image pass is loaded or FBO is uninitialized.
     */
    GLuint getOutputTextureID() const
    {
        if (passes.contains (ID::image))
            return passes.at (ID::image)->readBuffer().getTextureID();
        return 0;
    }

    /** @brief Binds buffer pass read textures to iChannel sampler uniforms.
     *
     *  Iterates getChannelMap(), binding each existing buffer pass's read FBO
     *  texture to the corresponding GL texture unit (0-3) and setting the
     *  iChannel uniform. Buffer passes are identified by passId != ID::image.
     *
     *  @param program  The active GL program (must be use()'d first).
     */
    void setChannels (juce::OpenGLShaderProgram& program)
    {
        using namespace ::juce::gl;

        for (auto& [id, channelName] : getChannelMap())
        {
            juce::Identifier passId { getShaderMap().at (id) };

            if (passes.contains (passId) and passId != ID::image)
            {
                glActiveTexture (GL_TEXTURE0 + id);
                glBindTexture (GL_TEXTURE_2D, passes.at (passId)->readBuffer().getTextureID());
                glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, uniform.textureFilter);
                glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, uniform.textureFilter);
                program.setUniform (channelName.toRawUTF8(), id);
            }
        }

        if (sceneTexture != 0)
        {
            const int unit { static_cast<int> (getChannelMap().size()) };
            glActiveTexture (GL_TEXTURE0 + unit);
            glBindTexture (GL_TEXTURE_2D, sceneTexture);
            glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, uniform.textureFilter);
            glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, uniform.textureFilter);
            program.setUniform (IDref::iScene, unit);
            program.setUniform (IDref::iPostOpacity, -1.0f);
        }
        else
        {
            program.setUniform (IDref::iPostOpacity, -1.0f);
        }
    }

    /** @brief Resizes uniform viewport and all pass FBOs.
     *
     *  Calls @c uniform.resize(w,h) which stores screen dims and computes
     *  buffer dims via resolutionScale. Pass FBOs are sized at the buffer
     *  resolution returned by @c uniform.getSize() — not the raw screen dims.
     *
     *  @param context  Active GL context.
     *  @param w        Screen width in pixels.
     *  @param h        Screen height in pixels.
     */
    void resize (juce::OpenGLContext& context, int w, int h) override
    {
        uniform.resize (w, h);
        auto [bw, bh] = uniform.getSize();

        for (auto& [id, pass] : passes)
            pass->resize (context, bw, bh);
    }

    //==============================================================================
    /** @brief GL texture ID for the composited scene (post-processing input). 0 = inactive (background). */
    GLuint sceneTexture { 0 };

    /** @brief Post-processing effect intensity (0.0 = original, 1.0 = full effect). Set by Processor. */
    float postOpacity { 0.0f };

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Compilation)
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace graphics
