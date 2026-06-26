/**
 * @file graphics/Program.h
 * @brief Shader pass data and per-frame uniform dispatch.
 */
#pragma once
#include <JuceHeader.h>
#include "Identifier.h"

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
    float resolutionScale { 0.5f };
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
/** @brief GL framebuffer container — single or ping-pong double-buffered.
 *
 *  Inherits std::vector<juce::OpenGLFrameBuffer> sized at construction.
 *  Single-buffer (1): direct render target.
 *  Double-buffer (2): ping-pong cycling via readBuffer()/writeBuffer()/swap().
 */
struct FrameBuffer : std::vector<juce::OpenGLFrameBuffer>
{
    FrameBuffer (size_t numBuffers = 1)
        : std::vector<juce::OpenGLFrameBuffer> (numBuffers)
    {
    }
    ~FrameBuffer() = default;

    /** @brief Index of the current read FBO (0 or 1). */
    int readIndex { 0 };

    /** @brief Returns the current read FBO — the source for sampling. */
    juce::OpenGLFrameBuffer& readBuffer() { return at (static_cast<size_t> (readIndex)); }

    /** @brief Returns the current write FBO — the render target. */
    juce::OpenGLFrameBuffer& writeBuffer() { return at (static_cast<size_t> (readIndex ^ 1)); }

    /** @brief Swaps read and write FBOs by toggling readIndex. */
    void swap() noexcept { readIndex ^= 1; }

    /** @brief Initialises and clears all FBOs at the given dimensions.
     *  @param context  Active GL context.
     *  @param w        Width in pixels.
     *  @param h        Height in pixels.
     */
    void resize (juce::OpenGLContext& context, int w, int h)
    {
        for (auto& fbo : *this)
        {
            fbo.initialise (context, w, h);
            fbo.makeCurrentAndClear();
            fbo.releaseAsRenderingTarget();
        }
    }

    void release()
    {
        for (auto& fbo : *this)
            fbo.release();
    }

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FrameBuffer)
};

//==============================================================================
/** @brief A compiled shader pass — GL program and optional ping-pong FBO pair.
 *
 *  Buffer is emplaced at creation time for non-Image passes.
 *  Image renders to default framebuffer (no buffer).
 *  Ping-pong state is delegated to FrameBuffer — readBuffer() is the current
 *  read source, writeBuffer() is the current render target. Call swap()
 *  after each pass to advance the pair.
 */
struct RenderPass
{
    std::unique_ptr<juce::OpenGLShaderProgram> program;
    std::optional<FrameBuffer> buffer;

    RenderPass() = default;
    ~RenderPass() = default;

    /** @brief Returns the current read FBO. Requires buffer to be emplaced. */
    juce::OpenGLFrameBuffer& readBuffer() { return buffer->readBuffer(); }

    /** @brief Returns the current write FBO. Requires buffer to be emplaced. */
    juce::OpenGLFrameBuffer& writeBuffer() { return buffer->writeBuffer(); }

    /** @brief Swaps read and write FBOs. Requires buffer to be emplaced. */
    void swap() noexcept { buffer->swap(); }

    /** @brief Resizes buffer FBOs. No-op if buffer is not emplaced.
     *         FBO content after initialise() is undefined per GL spec —
     *         explicit clear ensures first-frame reads get black, not garbage.
     *  @param context  Active GL context.
     *  @param w        Width in pixels.
     *  @param h        Height in pixels.
     */
    void resize (juce::OpenGLContext& context, int w, int h)
    {
        if (buffer.has_value())
            buffer->resize (context, w, h);
    }

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RenderPass)
};

//==============================================================================
/** @brief Fullscreen triangle strip — VAO + VBO for shader rendering. RAII. */
struct Quad
{
    Quad()
    {
        using namespace ::juce::gl;

        glGenVertexArrays (1, &vao);
        glBindVertexArray (vao);

        glGenBuffers (1, &vbo);
        glBindBuffer (GL_ARRAY_BUFFER, vbo);

        static constexpr float vertices[] { -1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f };
        glBufferData (GL_ARRAY_BUFFER, sizeof (vertices), vertices, GL_STATIC_DRAW);

        glVertexAttribPointer (0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
        glEnableVertexAttribArray (0);

        glBindVertexArray (0);
    }

    ~Quad()
    {
        using namespace ::juce::gl;

        glDeleteBuffers (1, &vbo);
        glDeleteVertexArrays (1, &vao);
    }

    void draw() const noexcept
    {
        using namespace ::juce::gl;

        glBindVertexArray (vao);
        glDrawArrays (GL_TRIANGLE_STRIP, 0, 4);
        glBindVertexArray (0);
    }

    GLuint vao { 0 };
    GLuint vbo { 0 };

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Quad)
};

//==============================================================================
/** @brief Compiled multi-pass Shadertoy pipeline — passes, uniform state, load/render/resize.
 *
 *  Encapsulates a complete Shadertoy-compatible shader set: BufferA-D intermediate
 *  passes and an Image output pass. Each pass is compiled from GLSL source,
 *  stored in the passes map as a RenderPass, and rendered in HashMap insertion order.
 *  Compositor owns two Compilation instances: background and post-processing.
 *
 *  @par Thread contract
 *  All methods must be called on the **GL THREAD** (via OpenGLRenderer callbacks
 *  or executeOnGLThread).
 */
struct Compilation
{
    Compilation() = default;
    ~Compilation() = default;

    jam::HashMap<juce::Identifier, std::unique_ptr<RenderPass>> passes;
    Uniform uniform;

    /** @brief Compiles shader passes from a config ValueTree.
     *
     *  Clears existing passes, iterates properties on @p shaderTree,
     *  compiles each non-Common pass via @p createProgram, and stores
     *  the resulting Pass in the map. Buffer passes (non-Image) get
     *  FBO pairs emplaced for ping-pong rendering.
     *
     *  @param shaderTree     Config VT with pass properties (key=pass name, value=GLSL source).
     *  @param wrapper        Wrapper fragment shader template with placeholder.
     *  @param placeholder    Placeholder string replaced by user source in wrapper.
     *  @param createProgram  Factory callable: (StringRef source) -> unique_ptr<OpenGLShaderProgram>.
     */
    template<typename F>
    void load (const juce::ValueTree& shaderTree,
               const juce::String& wrapper,
               const juce::String& placeholder,
               F&& createProgram)
    {
        passes.clear();

        juce::String common;
        if (shaderTree.hasProperty (ID::common))
            common = shaderTree.getProperty (ID::common).toString();

        jam::Model::forEachProperty (
            shaderTree,
            [this, &common, &wrapper, &placeholder, &createProgram] (
                const juce::Identifier& id, const juce::var& var)
            {
                if (id != ID::common and file::Shaders::getInstance()->contains (id.toString()))
                {
                    if (auto code { var.toString() }; code.isNotEmpty())
                    {
                        if (common.isNotEmpty())
                            code = jam::Format::prependNewLine (code, common);

                        code = jam::Format::replaceholder (wrapper, placeholder, code);

                        if (auto p { createProgram (code) })
                        {
                            auto pass { std::make_unique<RenderPass>() };
                            pass->program = std::move (p);

                            if (id != ID::image)
                                pass->buffer.emplace (2);

                            passes.addOrReplace (id, std::move (pass));
                        }
                    }
                }
            });
    }

    /** @brief Renders all buffer passes (BufferA-D) at the configured resolution.
     *
     *  Iterates passes with buffer in HashMap insertion order. Each pass
     *  renders into its writeBuffer FBO, then swaps the ping-pong index.
     *  Image pass (no buffer) is skipped.
     *
     *  @param quad  Fullscreen quad for drawing.
     */
    void renderBuffers (Quad& quad)
    {
        using namespace ::juce::gl;

        for (auto& [id, pass] : passes)
        {
            if (pass->buffer.has_value())
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

    /** @brief Renders the Image pass into a target FBO or the default framebuffer.
     *
     *  If @p target is non-null, binds it as render target, clears, and sets viewport.
     *  If @p target is null, renders to the currently bound framebuffer (default).
     *  No-op if no Image pass is loaded.
     *
     *  @param quad    Fullscreen quad for drawing.
     *  @param target  Target FBO, or nullptr for default framebuffer.
     */
    void renderImage (Quad& quad, juce::OpenGLFrameBuffer* target)
    {
        using namespace ::juce::gl;

        if (passes.contains (ID::image))
        {
            if (target != nullptr)
            {
                auto [w, h] = uniform.getSize();
                target->makeCurrentAndClear();
                glViewport (0, 0, w, h);
            }

            auto& image { passes.at (ID::image) };
            image->program->use();
            setChannels (*image->program);
            uniform.set (*image->program);
            quad.draw();

            if (target != nullptr)
                target->releaseAsRenderingTarget();
        }
    }

    /** @brief Binds buffer pass read textures to iChannel sampler uniforms.
     *
     *  Iterates file::BufferChannel bimap, binding each existing buffer pass's
     *  read FBO texture to the corresponding GL texture unit (0-3) and setting
     *  the iChannel uniform.
     *
     *  @param program  The active GL program (must be use()'d first).
     */
    void setChannels (juce::OpenGLShaderProgram& program)
    {
        using namespace ::juce::gl;

        for (auto& [id, channelName] : file::BufferChannel::get())
        {
            juce::Identifier passId { file::Shaders::get().at (id) };

            if (passes.contains (passId) and passes.at (passId)->buffer.has_value())
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
            const int unit { static_cast<int> (file::BufferChannel::get().size()) };
            glActiveTexture (GL_TEXTURE0 + unit);
            glBindTexture (GL_TEXTURE_2D, sceneTexture);
            glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, uniform.textureFilter);
            glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, uniform.textureFilter);
            program.setUniform (IDref::iScene, unit);
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
    void resize (juce::OpenGLContext& context, int w, int h)
    {
        uniform.resize (w, h);
        auto [bw, bh] = uniform.getSize();

        for (auto& [id, pass] : passes)
            pass->resize (context, bw, bh);
    }

    /** @brief Destroys all passes. */
    void shutdown() { passes.clear(); }

    /** @brief Returns true if the shader pipeline has been compiled (at least one pass loaded). */
    bool isCompiled() const noexcept { return passes.size() > 0; }

    /** @brief GL texture ID for the composited scene (post-processing input). 0 = inactive (background). */
    GLuint sceneTexture { 0 };

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Compilation)
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace graphics
