/**
 * @file jreng_gl_context.cpp
 * @brief GLContext GL-thread method implementations.
 *
 * All methods in this file execute on the **GL THREAD**.
 *
 * @see jreng_gl_context.h
 */
namespace jreng::Glyph
{ /*____________________________________________________________________________*/

// =============================================================================
// GL lifecycle
// =============================================================================

void GLContext::createContext() noexcept
{
    compileShaders();
    createBuffers();
    contextInitialised = true;
}

void GLContext::closeContext() noexcept
{
    if (contextInitialised)
    {
        contextInitialised = false;

        monoShader.reset();
        emojiShader.reset();
        backgroundShader.reset();

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

        monoAtlas  = 0;
        emojiAtlas = 0;
    }
}

// =============================================================================
// Per-frame operations
// =============================================================================

void GLContext::uploadStagedBitmaps (jreng::Typeface& typeface) noexcept
{
    auto& atlas { *jreng::GlyphAtlas::getContext() };

    if (atlas.getMonoAtlas() == 0)
    {
        const int atlasDim { getAtlasDimension() };
        atlas.setMonoAtlas (static_cast<uint32_t> (
            createAtlasTexture (atlasDim, atlasDim,
                                juce::gl::GL_R8,
                                juce::gl::GL_RED)));
        atlas.setEmojiAtlas (static_cast<uint32_t> (
            createAtlasTexture (atlasDim, atlasDim,
                                juce::gl::GL_RGBA,
                                juce::gl::GL_RGBA)));
    }

    monoAtlas  = static_cast<GLuint> (atlas.getMonoAtlas());
    emojiAtlas = static_cast<GLuint> (atlas.getEmojiAtlas());

    juce::HeapBlock<StagedBitmap> stagedBitmaps;
    int stagedCount { 0 };
    typeface.consumeStagedBitmaps (stagedBitmaps, stagedCount);

    if (stagedCount > 0)
    {
        juce::gl::glPixelStorei (juce::gl::GL_UNPACK_ALIGNMENT, 1);

        for (int i { 0 }; i < stagedCount; ++i)
        {
            const auto& staged { stagedBitmaps[i] };
            GLuint targetTexture { (staged.type == Packer::Type::emoji)
                                       ? emojiAtlas
                                       : monoAtlas };

            if (targetTexture != 0)
            {
                juce::gl::glBindTexture (juce::gl::GL_TEXTURE_2D, targetTexture);

                const GLenum pixelFormat { (staged.type == Packer::Type::emoji)
                                               ? juce::gl::GL_BGRA
                                               : juce::gl::GL_RED };

                juce::gl::glTexSubImage2D (juce::gl::GL_TEXTURE_2D,
                                           0,
                                           staged.region.getX(),
                                           staged.region.getY(),
                                           staged.region.getWidth(),
                                           staged.region.getHeight(),
                                           pixelFormat,
                                           juce::gl::GL_UNSIGNED_BYTE,
                                           staged.pixelData.get());
            }
        }

        juce::gl::glPixelStorei (juce::gl::GL_UNPACK_ALIGNMENT, 4);
    }
}

void GLContext::setViewportSize (int width, int height) noexcept
{
    viewportWidth  = width;
    viewportHeight = height;
}

void GLContext::push (int x, int y, int w, int h, int fullHeight) noexcept
{
    juce::gl::glViewport (x, fullHeight - y - h, w, h);
    viewportWidth  = w;
    viewportHeight = h;

    juce::gl::glEnable (juce::gl::GL_BLEND);
    juce::gl::glBlendFunc (juce::gl::GL_SRC_ALPHA,
                           juce::gl::GL_ONE_MINUS_SRC_ALPHA);
}

void GLContext::pop() noexcept
{
    juce::gl::glDisable (juce::gl::GL_BLEND);
}

void GLContext::drawQuads (const Render::Quad* data, int count, bool isEmoji) noexcept
{
    if (count > 0)
    {
        auto& shader  { isEmoji ? emojiShader : monoShader };
        auto  texture { isEmoji ? emojiAtlas : monoAtlas };

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
                static_cast<GLsizeiptr> (static_cast<size_t> (count) * sizeof (Render::Quad)),
                data,
                juce::gl::GL_DYNAMIC_DRAW);

            const jreng::GLVertexLayout::Attribute attrs[]
            {
                { 1, 2, juce::gl::GL_FLOAT, sizeof (Render::Quad), 0,                               1 },
                { 2, 2, juce::gl::GL_FLOAT, sizeof (Render::Quad), sizeof (juce::Point<float>),     1 },
                { 3, 4, juce::gl::GL_FLOAT, sizeof (Render::Quad), 2 * sizeof (juce::Point<float>), 1 },
                { 4, 4, juce::gl::GL_FLOAT, sizeof (Render::Quad), 2 * sizeof (juce::Point<float>) + sizeof (juce::Rectangle<float>), 1 }
            };
            jreng::GLVertexLayout::setupAttributes (attrs, 4);

            juce::gl::glDrawArraysInstanced (juce::gl::GL_TRIANGLE_STRIP,
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

void GLContext::drawBackgrounds (const Render::Background* data, int count) noexcept
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
                                    static_cast<size_t> (count) * sizeof (Render::Background)),
                                data,
                                juce::gl::GL_DYNAMIC_DRAW);

        const jreng::GLVertexLayout::Attribute attrs[]
        {
            { 1, 2, juce::gl::GL_FLOAT, sizeof (Render::Background), 0,                              1 },
            { 2, 2, juce::gl::GL_FLOAT, sizeof (Render::Background), 2 * sizeof (float),             1 },
            { 3, 4, juce::gl::GL_FLOAT, sizeof (Render::Background), sizeof (juce::Rectangle<float>), 1 }
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

// =============================================================================
// Font and glyph draw
// =============================================================================

void GLContext::setFont (jreng::Font& font) noexcept
{
    currentFont = &font;
}

void GLContext::drawGlyphs (const uint16_t* glyphCodes,
                                  const juce::Point<float>* positions,
                                  int count) noexcept
{
    jassert (currentFont != nullptr);

    if (count > 0 and currentFont != nullptr)
    {
        juce::Array<Render::Quad> quads;

        for (int i { 0 }; i < count; ++i)
        {
            jreng::Glyph::Region* region { currentFont->getGlyph (glyphCodes[i]) };

            if (region != nullptr)
            {
                const float colR { 1.0f };
                const float colG { 1.0f };
                const float colB { 1.0f };
                const float colA { 1.0f };

                Render::Quad quad;
                quad.screenPosition     = { positions[i].x + static_cast<float> (region->bearingX),
                                            positions[i].y - static_cast<float> (region->bearingY) };
                quad.glyphSize          = { static_cast<float> (region->widthPixels),
                                            static_cast<float> (region->heightPixels) };
                quad.textureCoordinates = region->textureCoordinates;
                quad.foregroundColorR   = colR;
                quad.foregroundColorG   = colG;
                quad.foregroundColorB   = colB;
                quad.foregroundColorA   = colA;

                quads.add (quad);
            }
        }

        if (quads.size() > 0)
        {
            const bool isEmoji { currentFont->isEmoji() };
            drawQuads (quads.data(), quads.size(), isEmoji);
        }
    }
}

// =============================================================================
// State queries
// =============================================================================

void GLContext::setGraphicsContext (juce::Graphics&) noexcept
{
}

void GLContext::prepareFrame (const uint64_t*, int, int, int, int) noexcept
{
}

bool GLContext::isReady() const noexcept
{
    return monoShader != nullptr;
}

// =============================================================================
// Private helpers
// =============================================================================

void GLContext::compileShaders() noexcept
{
    auto* glContext { juce::OpenGLContext::getCurrentContext() };
    if (glContext != nullptr)
    {
        const juce::String glyphVertSrc   { Shaders::glyphVert };
        const juce::String monoFragSrc    { Shaders::glyphMonoFrag };
        const juce::String emojiFragSrc   { Shaders::glyphEmojiFrag };
        const juce::String bgVertSrc      { Shaders::backgroundVert };
        const juce::String bgFragSrc      { Shaders::backgroundFrag };

        monoShader       = jreng::GLShaderCompiler::compile (*glContext, glyphVertSrc, monoFragSrc);
        emojiShader      = jreng::GLShaderCompiler::compile (*glContext, glyphVertSrc, emojiFragSrc);
        backgroundShader = jreng::GLShaderCompiler::compile (*glContext, bgVertSrc, bgFragSrc);
    }
}

void GLContext::createBuffers() noexcept
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
    juce::gl::glVertexAttribPointer (0, 2, juce::gl::GL_FLOAT, juce::gl::GL_FALSE, 0, nullptr);

    juce::gl::glGenBuffers (1, &instanceVBO);

    juce::gl::glBindVertexArray (0);
}

GLuint GLContext::createAtlasTexture (int width, int height,
                                           GLenum internalFormat,
                                           GLenum format) noexcept
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

/**______________________________END OF NAMESPACE______________________________*/
} // namespace jreng::Glyph
