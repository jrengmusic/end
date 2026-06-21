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
    int lastFrameTime { 0 };

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

    /** @brief Updates viewport dimensions from pixel width and height.
     *  @param w  Viewport width in pixels.
     *  @param h  Viewport height in pixels.
     */
    void resize (int w, int h) { values.at (ID::iResolution) = end::Size (w, h).toInt(); }

    /** @brief Advances per-frame time and frame counter. Called once per frame. */
    void advance()
    {
        auto now { static_cast<int> (juce::Time::getMillisecondCounterHiRes()) };
        values.at (ID::iTimeDelta) = now - lastFrameTime;
        values.at (ID::iTime) = values.at (ID::iTime) + values.at (ID::iTimeDelta);
        lastFrameTime = now;
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
/** @brief A compiled shader pass — GL program and optional ping-pong FBO pair.
 *
 *  Buffer is emplaced at creation time for non-Image passes.
 *  Image renders to default framebuffer (no buffer).
 */
struct Pass
{
    std::unique_ptr<juce::OpenGLShaderProgram> program;
    std::optional<std::array<juce::OpenGLFrameBuffer, 2>> buffer;

    Pass() = default;
    ~Pass() = default;

    /** @brief Initialises existing FBO pair at given pixel dims. No-op if no buffer (Image pass).
     *  @param context  Active GL context.
     *  @param w        Width in pixels.
     *  @param h        Height in pixels.
     */
    void resize (juce::OpenGLContext& context, int w, int h)
    {
        if (buffer.has_value())
        {
            (*buffer).at (0).initialise (context, w, h);
            (*buffer).at (1).initialise (context, w, h);
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
