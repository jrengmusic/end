/**
 * @file jreng_gl_text_renderer.cpp
 * @brief GLTextRenderer GL-thread method implementations.
 *
 * All methods in this file execute on the **GL THREAD**.
 *
 * @see jreng_gl_text_renderer.h
 */
namespace jreng::Glyph
{ /*____________________________________________________________________________*/

// =============================================================================
// Static definitions
// =============================================================================

GLuint GLTextRenderer::sharedMonoAtlas     { 0 };
GLuint GLTextRenderer::sharedEmojiAtlas    { 0 };
int    GLTextRenderer::sharedAtlasRefCount { 0 };

// =============================================================================
// GL lifecycle
// =============================================================================

void GLTextRenderer::contextCreated() noexcept
{
    compileShaders();
    createBuffers();

    if (sharedAtlasRefCount == 0)
    {
        const int atlasDim { jreng::Typeface::atlasDimension() };
        sharedMonoAtlas  = createAtlasTexture (atlasDim, atlasDim,
                                               juce::gl::GL_R8,
                                               juce::gl::GL_RED);
        sharedEmojiAtlas = createAtlasTexture (atlasDim, atlasDim,
                                               juce::gl::GL_RGBA,
                                               juce::gl::GL_RGBA);
    }

    ++sharedAtlasRefCount;
}

void GLTextRenderer::contextClosing() noexcept
{
    monoShader.reset();
    emojiShader.reset();
    backgroundShader.reset();

    --sharedAtlasRefCount;

    if (sharedAtlasRefCount == 0)
    {
        if (sharedMonoAtlas != 0)
        {
            juce::gl::glDeleteTextures (1, &sharedMonoAtlas);
            sharedMonoAtlas = 0;
        }

        if (sharedEmojiAtlas != 0)
        {
            juce::gl::glDeleteTextures (1, &sharedEmojiAtlas);
            sharedEmojiAtlas = 0;
        }
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

// =============================================================================
// Per-frame operations
// =============================================================================

void GLTextRenderer::uploadStagedBitmaps (jreng::Typeface& typeface) noexcept
{
    juce::HeapBlock<StagedBitmap> stagedBitmaps;
    int stagedCount { 0 };
    typeface.consumeStagedBitmaps (stagedBitmaps, stagedCount);

    if (stagedCount > 0)
    {
        juce::gl::glPixelStorei (juce::gl::GL_UNPACK_ALIGNMENT, 1);

        for (int i { 0 }; i < stagedCount; ++i)
        {
            const auto& staged { stagedBitmaps[i] };
            GLuint targetTexture { (staged.type == Atlas::Type::emoji)
                                       ? sharedEmojiAtlas
                                       : sharedMonoAtlas };

            if (targetTexture != 0)
            {
                juce::gl::glBindTexture (juce::gl::GL_TEXTURE_2D, targetTexture);

                const GLenum pixelFormat { (staged.type == Atlas::Type::emoji)
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

void GLTextRenderer::setViewportSize (int width, int height) noexcept
{
    viewportWidth  = width;
    viewportHeight = height;
}

void GLTextRenderer::drawQuads (const Render::Quad* data, int count, bool isEmoji) noexcept
{
    if (count > 0)
    {
        auto& shader  { isEmoji ? emojiShader : monoShader };
        auto  texture { isEmoji ? sharedEmojiAtlas : sharedMonoAtlas };

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

void GLTextRenderer::drawBackgrounds (const Render::Background* data, int count) noexcept
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

void GLTextRenderer::setFont (jreng::Font& font) noexcept
{
    currentFont = &font;
}

void GLTextRenderer::drawGlyphs (const uint16_t* glyphCodes,
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

bool GLTextRenderer::isReady() const noexcept
{
    return monoShader != nullptr;
}

// =============================================================================
// Private helpers
// =============================================================================

void GLTextRenderer::compileShaders() noexcept
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

void GLTextRenderer::createBuffers() noexcept
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

GLuint GLTextRenderer::createAtlasTexture (int width, int height,
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
