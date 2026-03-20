/**
 * @file ScreenGL.cpp
 * @brief Screen GL thread methods: shader compilation, buffer management, and draw calls.
 *
 * @note All functions in this file run on the **GL THREAD**.
 *
 * @see Screen
 * @see Screen.h
 * @see Screen.cpp (MESSAGE THREAD methods)
 */
#include "Screen.h"
#include <array>

namespace Terminal
{ /*____________________________________________________________________________*/

GLuint Screen::createAtlasTexture (int width, int height, GLenum internalFormat, GLenum format) noexcept
{
    GLuint texture { 0 };
    juce::gl::glGenTextures (1, &texture);
    juce::gl::glBindTexture (juce::gl::GL_TEXTURE_2D, texture);

    juce::gl::glTexParameteri (juce::gl::GL_TEXTURE_2D,
                               juce::gl::GL_TEXTURE_MIN_FILTER,
                               juce::gl::GL_LINEAR);
    juce::gl::glTexParameteri (juce::gl::GL_TEXTURE_2D,
                               juce::gl::GL_TEXTURE_MAG_FILTER,
                               juce::gl::GL_LINEAR);
    juce::gl::glTexParameteri (juce::gl::GL_TEXTURE_2D,
                               juce::gl::GL_TEXTURE_WRAP_S,
                               juce::gl::GL_CLAMP_TO_EDGE);
    juce::gl::glTexParameteri (juce::gl::GL_TEXTURE_2D,
                               juce::gl::GL_TEXTURE_WRAP_T,
                               juce::gl::GL_CLAMP_TO_EDGE);

    juce::gl::glTexImage2D (juce::gl::GL_TEXTURE_2D,
                            0,
                            internalFormat,
                            width,
                            height,
                            0,
                            format,
                            juce::gl::GL_UNSIGNED_BYTE,
                            nullptr);

    return texture;
}

void Screen::glContextCreated()
{
    jreng::BackgroundBlur::enableGLTransparency();

    compileShaders();
    createBuffers();

    const int atlasDim { GlyphAtlas::atlasDimension() };
    monoAtlasTexture = createAtlasTexture (atlasDim, atlasDim, juce::gl::GL_R8, juce::gl::GL_RED);
    emojiAtlasTexture = createAtlasTexture (atlasDim, atlasDim, juce::gl::GL_RGBA, juce::gl::GL_RGBA);
}

void Screen::renderOpenGL (int originX, int originY, int fullHeight)
{
    if (monoShader != nullptr and emojiShader != nullptr and backgroundShader != nullptr)
    {
        const int glX { originX + glViewportX };
        const int glY { fullHeight - originY - glViewportY - glViewportHeight };
        juce::gl::glViewport (glX, glY, glViewportWidth, glViewportHeight);

        Render::Snapshot* snapshot { resources.snapshotBuffer.read() };

        if (snapshot != nullptr)
        {
            uploadStagedBitmaps();

            juce::gl::glEnable (juce::gl::GL_BLEND);
            juce::gl::glBlendFunc (
                juce::gl::GL_SRC_ALPHA, juce::gl::GL_ONE_MINUS_SRC_ALPHA);

            if (snapshot->backgroundCount > 0)
            {
                drawBackgrounds (snapshot->backgrounds.get(), snapshot->backgroundCount);
            }

            if (snapshot->monoCount > 0)
            {
                drawInstances (snapshot->mono.get(), snapshot->monoCount, false);
            }

            if (snapshot->emojiCount > 0)
            {
                drawInstances (snapshot->emoji.get(), snapshot->emojiCount, true);
            }

            drawCursor (*snapshot);

            juce::gl::glDisable (juce::gl::GL_BLEND);
        }
    }
}

void Screen::glContextClosing()
{
    monoShader.reset();
    emojiShader.reset();
    backgroundShader.reset();

    if (monoAtlasTexture != 0)
    {
        juce::gl::glDeleteTextures (1, &monoAtlasTexture);
        monoAtlasTexture = 0;
    }

    if (emojiAtlasTexture != 0)
    {
        juce::gl::glDeleteTextures (1, &emojiAtlasTexture);
        emojiAtlasTexture = 0;
    }

    if (vao != 0)
    {
        juce::gl::glDeleteVertexArrays (1, &vao);
        vao = 0;
    }

    if (quadVBO != 0)
    {
        juce::gl::glDeleteBuffers (1, &quadVBO);
        quadVBO = 0;
    }

    if (instanceVBO != 0)
    {
        juce::gl::glDeleteBuffers (1, &instanceVBO);
        instanceVBO = 0;
    }
}

bool Screen::isGLContextReady() const noexcept
{
    return monoShader != nullptr;
}

void Screen::compileShaders()
{
    juce::String glyphVertShader { BinaryData::getString ("glyph.vert") };
    juce::String monoFragShader { BinaryData::getString ("glyph_mono.frag") };
    juce::String emojiFragShader { BinaryData::getString ("glyph_emoji.frag") };
    juce::String bgVertShader { BinaryData::getString ("background.vert") };
    juce::String bgFragShader { BinaryData::getString ("background.frag") };

    auto* glContext { juce::OpenGLContext::getCurrentContext() };
    if (glContext != nullptr)
    {
        monoShader = jreng::GLShaderCompiler::compile (*glContext, glyphVertShader, monoFragShader);
        emojiShader = jreng::GLShaderCompiler::compile (*glContext, glyphVertShader, emojiFragShader);
        backgroundShader = jreng::GLShaderCompiler::compile (*glContext, bgVertShader, bgFragShader);
    }
}

void Screen::createBuffers()
{
    juce::gl::glGenVertexArrays (1, &vao);
    juce::gl::glBindVertexArray (vao);

    float quadVertices[] { 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f };

    juce::gl::glGenBuffers (1, &quadVBO);
    juce::gl::glBindBuffer (juce::gl::GL_ARRAY_BUFFER, quadVBO);
    juce::gl::glBufferData (juce::gl::GL_ARRAY_BUFFER,
                            sizeof (quadVertices),
                            quadVertices,
                            juce::gl::GL_STATIC_DRAW);

    juce::gl::glEnableVertexAttribArray (0);
    juce::gl::glVertexAttribPointer (
        0, 2, juce::gl::GL_FLOAT, juce::gl::GL_FALSE, 0, nullptr);

    juce::gl::glGenBuffers (1, &instanceVBO);

    juce::gl::glBindVertexArray (0);
}

void Screen::uploadStagedBitmaps()
{
    juce::HeapBlock<StagedBitmap> stagedBitmaps;
    int stagedCount { 0 };
    resources.glyphAtlas.consumeStagedBitmaps (stagedBitmaps, stagedCount);

    if (stagedCount > 0)
    {
        juce::gl::glPixelStorei (juce::gl::GL_UNPACK_ALIGNMENT, 1);

        for (int i { 0 }; i < stagedCount; ++i)
        {
            const auto& staged { stagedBitmaps[i] };
            GLuint targetTexture { (staged.kind == StagedBitmap::AtlasKind::emoji)
                                       ? emojiAtlasTexture
                                       : monoAtlasTexture };

            if (targetTexture != 0)
            {
                juce::gl::glBindTexture (juce::gl::GL_TEXTURE_2D, targetTexture);

                juce::gl::glTexSubImage2D (juce::gl::GL_TEXTURE_2D,
                                           0,
                                           staged.region.getX(),
                                           staged.region.getY(),
                                           staged.region.getWidth(),
                                           staged.region.getHeight(),
                                           static_cast<GLenum> (staged.format),
                                           juce::gl::GL_UNSIGNED_BYTE,
                                           staged.pixelData.get());
            }
        }

        juce::gl::glPixelStorei (juce::gl::GL_UNPACK_ALIGNMENT, 4);
    }
}

void Screen::drawInstances (const Render::Glyph* data, int count, bool isEmoji)
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
                                static_cast<float> (glViewportWidth),
                                static_cast<float> (glViewportHeight));
            shader->setUniform (isEmoji ? "uEmojiTexture" : "uAtlasTexture", 0);

            juce::gl::glActiveTexture (juce::gl::GL_TEXTURE0);
            juce::gl::glBindTexture (juce::gl::GL_TEXTURE_2D, texture);

            juce::gl::glBindBuffer (juce::gl::GL_ARRAY_BUFFER, instanceVBO);
            juce::gl::glBufferData (
                juce::gl::GL_ARRAY_BUFFER,
                static_cast<GLsizeiptr> (static_cast<size_t> (count) * sizeof (Render::Glyph)),
                data,
                juce::gl::GL_DYNAMIC_DRAW);

            const jreng::GLVertexLayout::Attribute attrs[]
            {
                { 1, 2, juce::gl::GL_FLOAT, sizeof (Render::Glyph), 0,                              1 },
                { 2, 2, juce::gl::GL_FLOAT, sizeof (Render::Glyph), sizeof (juce::Point<float>),    1 },
                { 3, 4, juce::gl::GL_FLOAT, sizeof (Render::Glyph), 2 * sizeof (juce::Point<float>), 1 },
                { 4, 4, juce::gl::GL_FLOAT, sizeof (Render::Glyph), 2 * sizeof (juce::Point<float>) + sizeof (juce::Rectangle<float>), 1 }
            };
            jreng::GLVertexLayout::setupAttributes (attrs, 4);

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

void Screen::drawBackgrounds (const Render::Background* data, int count)
{
    if (count > 0 and backgroundShader != nullptr)
    {
        juce::gl::glBindVertexArray (vao);

        backgroundShader->use();
        backgroundShader->setUniform ("uViewportSize",
                                       static_cast<float> (glViewportWidth),
                                       static_cast<float> (glViewportHeight));

        juce::gl::glBindBuffer (juce::gl::GL_ARRAY_BUFFER, instanceVBO);
        juce::gl::glBufferData (juce::gl::GL_ARRAY_BUFFER,
                                static_cast<GLsizeiptr> (
                                    static_cast<size_t> (count) * sizeof (Render::Background)),
                                data,
                                juce::gl::GL_DYNAMIC_DRAW);

        const jreng::GLVertexLayout::Attribute attrs[]
        {
            { 1, 2, juce::gl::GL_FLOAT, sizeof (Render::Background), 0,                                1 },
            { 2, 2, juce::gl::GL_FLOAT, sizeof (Render::Background), 2 * sizeof (float),               1 },
            { 3, 4, juce::gl::GL_FLOAT, sizeof (Render::Background), sizeof (juce::Rectangle<float>),   1 }
        };
        jreng::GLVertexLayout::setupAttributes (attrs, 3);

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

/**
 * @brief Draws the cursor as a geometric background quad from snapshot data.
 *
 * The cursor is a coloured rectangle whose shape depends on the DECSCUSR Ps
 * value in the snapshot:
 * - Shapes 0, 1, 2 (block): full cell rectangle.
 * - Shapes 3, 4 (underline): thin strip at the bottom of the cell.
 * - Shapes 5, 6 (bar): thin strip at the left edge of the cell.
 *
 * The cursor is hidden when any of these conditions hold:
 * - DECTCEM cursor mode is off (`cursorVisible == false`).
 * - The blink phase is in the hidden half (`cursorBlinkOn == false`).
 * - The terminal component is unfocused (`cursorFocused == false`).
 * - The viewport is scrolled back (`scrollOffset > 0`).
 *
 * Colour is determined by OSC 12 override if active (all three R/G/B >= 0),
 * otherwise falls back to the theme's `cursorColour`.
 *
 * Drawn after glyphs so the cursor overlays the cell content.
 *
 * @param snapshot  The current frame's snapshot (already acquired by `renderOpenGL`).
 * @note **GL THREAD** only.
 */
void Screen::drawCursor (const Render::Snapshot& snapshot)
{
    const int col { snapshot.cursorPosition.x };
    const int row { snapshot.cursorPosition.y };

    const bool shouldDraw { snapshot.cursorVisible
                            and snapshot.cursorBlinkOn
                            and snapshot.cursorFocused
                            and snapshot.scrollOffset == 0
                            and col >= 0 and col < snapshot.gridWidth
                            and row >= 0 and row < snapshot.gridHeight };

    if (shouldDraw)
    {
        // User glyph cursor (shape 0 or cursor.force): draw the pre-built
        // glyph from the snapshot.  Emoji codepoints live in the RGBA atlas
        // and must go through the emoji shader path to preserve native colour.
        if (snapshot.hasCursorGlyph)
        {
            drawInstances (&snapshot.cursorGlyph, 1, snapshot.cursorGlyphIsEmoji);
        }
        else
        {
            // Geometric cursor (shapes 1–6): coloured rectangle.
            // Colour was resolved once on the message thread in updateSnapshot()
            // so the GL thread never reads resources.terminalColors here.
            const float r { snapshot.cursorDrawColorR };
            const float g { snapshot.cursorDrawColorG };
            const float b { snapshot.cursorDrawColorB };

            const float cellX { static_cast<float> (col * physCellWidth) };
            const float cellY { static_cast<float> (row * physCellHeight) };
            const float cellW { static_cast<float> (physCellWidth) };
            const float cellH { static_cast<float> (physCellHeight) };

            static constexpr float cursorThickness { 0.15f };

            float x { cellX };
            float y { cellY };
            float w { cellW };
            float h { cellH };

            switch (snapshot.cursorShape)
            {
                case 3:
                case 4:
                {
                    // Underline: thin strip at cell bottom.
                    const float thickness { std::max (1.0f, cellH * cursorThickness) };
                    y = cellY + cellH - thickness;
                    h = thickness;
                    break;
                }

                case 5:
                case 6:
                {
                    // Bar: thin strip at cell left edge.
                    const float thickness { std::max (1.0f, cellW * cursorThickness) };
                    w = thickness;
                    break;
                }

                default:
                    // Block (shapes 1, 2): full cell.
                    break;
            }

            const Render::Background cursorBg
            {
                juce::Rectangle<float> { x, y, w, h },
                r, g, b, 1.0f
            };

            drawBackgrounds (&cursorBg, 1);
        }
    }
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Terminal
