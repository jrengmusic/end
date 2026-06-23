/**
 * @file shader/Program.h
 * @brief Shader pass data and per-frame uniform dispatch.
 */
#pragma once
#include <JuceHeader.h>
#include "Identifier.h"

namespace shader
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

    /** @brief Dispatches all uniforms but overrides iResolution with the full
     *         screen resolution. Used by the post-processing pipeline, whose
     *         passes normalise gl_FragCoord against the screen-size iResolution
     *         (post renders at full physical screen res, not the scaled buffer res).
     *  @param p  The active GL program (must be use()'d first).
     */
    void setScene (juce::OpenGLShaderProgram& p)
    {
        set (p);
        setters.get (ID::iResolution, p, screenResolution);
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
/** @brief A compiled shader pass — GL program and optional ping-pong FBO pair.
 *
 *  Buffer is emplaced at creation time for non-Image passes.
 *  Image renders to default framebuffer (no buffer).
 *  Ping-pong state is tracked via readIndex — readBuffer() is the current
 *  read source, writeBuffer() is the current render target. Call swap()
 *  after each pass to advance the pair.
 */
struct Pass
{
    std::unique_ptr<juce::OpenGLShaderProgram> program;
    std::optional<std::array<juce::OpenGLFrameBuffer, 2>> buffer;

    /** @brief Index of the current read FBO in the ping-pong pair (0 or 1). */
    int readIndex { 0 };

    Pass() = default;
    ~Pass() = default;

    /** @brief Returns the current read FBO — the source for this pass's sampler. */
    juce::OpenGLFrameBuffer& readBuffer() { return (*buffer).at (readIndex); }

    /** @brief Returns the current write FBO — the render target for this pass. */
    juce::OpenGLFrameBuffer& writeBuffer() { return (*buffer).at (readIndex ^ 1); }

    /** @brief Swaps read and write FBOs by toggling readIndex. */
    void swap() noexcept { readIndex ^= 1; }

    /** @brief Initialises and clears the FBO pair at given pixel dims.
     *         No-op if no buffer (Image pass). FBO content after initialise()
     *         is undefined per GL spec — explicit clear ensures first-frame
     *         reads get black, not garbage.
     *  @param context  Active GL context.
     *  @param w        Width in pixels.
     *  @param h        Height in pixels.
     */
    void resize (juce::OpenGLContext& context, int w, int h)
    {
        if (buffer.has_value())
        {
            for (auto& fbo : *buffer)
            {
                fbo.initialise (context, w, h);
                fbo.makeCurrentAndClear();
                fbo.releaseAsRenderingTarget();
            }
        }
    }

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Pass)
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

/**______________________________END OF NAMESPACE______________________________*/
}// namespace shader
