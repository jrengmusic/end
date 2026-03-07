/**
 * @file TerminalGLDraw.cpp
 * @brief Instance upload and instanced draw calls for glyph and background passes.
 *
 * Implements the two draw methods of `Terminal::Render::OpenGL`:
 *
 * - `drawInstances()` — uploads an array of `Render::Glyph` instances to the
 *   instance VBO and issues a single `glDrawArraysInstanced` call for either
 *   the mono or emoji shader pass.
 * - `drawBackgrounds()` — uploads an array of `Render::Background` instances
 *   to the instance VBO and issues a single `glDrawArraysInstanced` call for
 *   the background shader pass.
 *
 * ### Instanced rendering model
 *
 * Both passes share the same VAO and unit-quad VBO (attribute location 0,
 * set up in `createBuffers()`).  Per-instance data is streamed into the
 * instance VBO each frame via `GL_DYNAMIC_DRAW`.  `GLVertexLayout::setupAttributes()`
 * configures the per-instance attribute pointers and divisors immediately
 * before each draw call.  Divisors are reset to 0 after the draw to leave the
 * VAO in a clean state.
 *
 * ### Vertex attribute layout — glyph pass
 *
 * | Location | Components | Type      | Stride         | Offset                                    | Divisor |
 * |----------|------------|-----------|----------------|-------------------------------------------|---------|
 * | 0        | 2          | GL_FLOAT  | —              | —  (quad VBO, non-instanced)              | 0       |
 * | 1        | 2          | GL_FLOAT  | sizeof(Glyph)  | 0                                         | 1       |
 * | 2        | 2          | GL_FLOAT  | sizeof(Glyph)  | sizeof(Point<float>)                      | 1       |
 * | 3        | 4          | GL_FLOAT  | sizeof(Glyph)  | 2×sizeof(Point<float>)                    | 1       |
 * | 4        | 4          | GL_FLOAT  | sizeof(Glyph)  | 2×sizeof(Point<float>)+sizeof(Rect<float>)| 1       |
 *
 * ### Vertex attribute layout — background pass
 *
 * | Location | Components | Type      | Stride             | Offset                    | Divisor |
 * |----------|------------|-----------|--------------------|---------------------------|---------|
 * | 0        | 2          | GL_FLOAT  | —                  | — (quad VBO, non-instanced)| 0      |
 * | 1        | 2          | GL_FLOAT  | sizeof(Background) | 0                         | 1       |
 * | 2        | 2          | GL_FLOAT  | sizeof(Background) | 2×sizeof(float)           | 1       |
 * | 3        | 4          | GL_FLOAT  | sizeof(Background) | sizeof(Rect<float>)       | 1       |
 *
 * @note All functions in this file run on the **GL THREAD**.
 *
 * @see Terminal::Render::OpenGL
 * @see GLVertexLayout
 * @see TerminalGLRenderer.cpp
 */
#include "Screen.h"

namespace Terminal
{ /*____________________________________________________________________________*/

/**
 * @brief Issues an instanced draw call for an array of glyph instances.
 *
 * Selects the mono or emoji shader and atlas texture based on @p isEmoji,
 * uploads @p data to the instance VBO, configures per-instance vertex
 * attributes via `GLVertexLayout::setupAttributes()`, and draws all instances
 * with a single `glDrawArraysInstanced` call.
 *
 * @par Attribute layout
 * Four per-instance attributes are configured (locations 1–4):
 * - **loc 1** (`vec2`) — `Glyph::screenPosition` (top-left in physical pixels).
 * - **loc 2** (`vec2`) — `Glyph::glyphSize` (width × height in physical pixels).
 * - **loc 3** (`vec4`) — `Glyph::textureCoordinates` (UV rect in atlas space).
 * - **loc 4** (`vec4`) — `Glyph::foregroundColor{R,G,B,A}` (RGBA colour).
 *
 * @par Uniforms set
 * - `uViewportSize` (`vec2`) — physical viewport dimensions for the vertex shader.
 * - `uAtlasTexture` or `uEmojiTexture` (`sampler2D`) — bound to texture unit 0.
 *
 * @param data     Pointer to the first `Render::Glyph` in the instance array.
 *                 Must not be `nullptr` when @p count > 0.
 * @param count    Number of glyph instances to draw.  Must be > 0.
 * @param isEmoji  `true` to use the emoji shader and `emojiAtlasTexture`;
 *                 `false` to use the mono shader and `monoAtlasTexture`.
 *
 * @note **GL THREAD**.
 * @see GLVertexLayout::setupAttributes()
 * @see Render::Glyph
 * @see drawBackgrounds()
 */
void Render::OpenGL::drawInstances (
    const Glyph* data, int count, bool isEmoji)
{
    if (count > 0)
    {
        auto& shader { isEmoji ? emojiShader : monoShader };
        auto texture { isEmoji ? emojiAtlasTexture : monoAtlasTexture };

        if (shader != nullptr)
        {
            juce::gl::glBindVertexArray (vao);

            shader->use();
            shader->setUniform ("uViewportSize",
                                static_cast<float> (viewportWidth),
                                static_cast<float> (viewportHeight));
            shader->setUniform (isEmoji ? "uEmojiTexture" : "uAtlasTexture", 0);

            juce::gl::glActiveTexture (juce::gl::GL_TEXTURE0);
            juce::gl::glBindTexture (juce::gl::GL_TEXTURE_2D, texture);

            juce::gl::glBindBuffer (juce::gl::GL_ARRAY_BUFFER, instanceVBO);
            juce::gl::glBufferData (
                juce::gl::GL_ARRAY_BUFFER,
                static_cast<GLsizeiptr> (static_cast<size_t> (count) * sizeof (Glyph)),
                data,
                juce::gl::GL_DYNAMIC_DRAW);

            const GLVertexLayout::Attribute attrs[]
            {
                { 1, 2, juce::gl::GL_FLOAT, sizeof (Glyph), 0,                              1 },
                { 2, 2, juce::gl::GL_FLOAT, sizeof (Glyph), sizeof (juce::Point<float>),    1 },
                { 3, 4, juce::gl::GL_FLOAT, sizeof (Glyph), 2 * sizeof (juce::Point<float>), 1 },
                { 4, 4, juce::gl::GL_FLOAT, sizeof (Glyph), 2 * sizeof (juce::Point<float>) + sizeof (juce::Rectangle<float>), 1 }
            };
            GLVertexLayout::setupAttributes (attrs, 4);

            juce::gl::glDrawArraysInstanced (
                juce::gl::GL_TRIANGLE_STRIP,
                0,
                4,
                static_cast<GLsizei> (count));

            juce::gl::glVertexAttribDivisor (1, 0);
            juce::gl::glVertexAttribDivisor (2, 0);
            juce::gl::glVertexAttribDivisor (3, 0);
            juce::gl::glVertexAttribDivisor (4, 0);

            juce::gl::glBindVertexArray (0);
        }
    }
}

/**
 * @brief Issues an instanced draw call for an array of background colour quads.
 *
 * Uploads @p data to the instance VBO, configures three per-instance vertex
 * attributes via `GLVertexLayout::setupAttributes()`, and draws all instances
 * with a single `glDrawArraysInstanced` call using the background shader.
 *
 * @par Attribute layout
 * Three per-instance attributes are configured (locations 1–3):
 * - **loc 1** (`vec2`) — `Background::screenBounds` position (x, y in physical pixels).
 * - **loc 2** (`vec2`) — `Background::screenBounds` size (width, height in physical pixels).
 * - **loc 3** (`vec4`) — `Background::backgroundColor{R,G,B,A}` (RGBA colour).
 *
 * @par Uniforms set
 * - `uViewportSize` (`vec2`) — physical viewport dimensions for the vertex shader.
 *
 * @param data   Pointer to the first `Render::Background` in the instance array.
 *               Must not be `nullptr` when @p count > 0.
 * @param count  Number of background quads to draw.  Must be > 0.
 *
 * @note **GL THREAD**.
 * @see GLVertexLayout::setupAttributes()
 * @see Render::Background
 * @see drawInstances()
 */
void Render::OpenGL::drawBackgrounds (
    const Background* data, int count)
{
    if (count > 0 and backgroundShader != nullptr)
    {
        juce::gl::glBindVertexArray (vao);

        backgroundShader->use();
        backgroundShader->setUniform ("uViewportSize",
                                       static_cast<float> (viewportWidth),
                                       static_cast<float> (viewportHeight));

        juce::gl::glBindBuffer (juce::gl::GL_ARRAY_BUFFER, instanceVBO);
        juce::gl::glBufferData (juce::gl::GL_ARRAY_BUFFER,
                                static_cast<GLsizeiptr> (
                                    static_cast<size_t> (count) * sizeof (Background)),
                                data,
                                juce::gl::GL_DYNAMIC_DRAW);

        const GLVertexLayout::Attribute attrs[]
        {
            { 1, 2, juce::gl::GL_FLOAT, sizeof (Background), 0,                                1 },
            { 2, 2, juce::gl::GL_FLOAT, sizeof (Background), 2 * sizeof (float),               1 },
            { 3, 4, juce::gl::GL_FLOAT, sizeof (Background), sizeof (juce::Rectangle<float>),   1 }
        };
        GLVertexLayout::setupAttributes (attrs, 3);

        juce::gl::glDrawArraysInstanced (juce::gl::GL_TRIANGLE_STRIP,
                                         0,
                                         4,
                                         static_cast<GLsizei> (count));

        juce::gl::glVertexAttribDivisor (1, 0);
        juce::gl::glVertexAttribDivisor (2, 0);
        juce::gl::glVertexAttribDivisor (3, 0);

        juce::gl::glBindVertexArray (0);
    }
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Terminal
