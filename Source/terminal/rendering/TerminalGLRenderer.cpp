/**
 * @file TerminalGLRenderer.cpp
 * @brief `Render::OpenGL` lifecycle, shader compilation, buffer creation, and atlas upload.
 *
 * Implements the non-draw portions of `Terminal::Render::OpenGL`:
 *
 * - **Context lifecycle** — `newOpenGLContextCreated()`, `openGLContextClosing()`.
 * - **Frame loop** — `renderOpenGL()`: viewport setup, snapshot acquisition,
 *   atlas upload, and the three-pass draw sequence (backgrounds → mono → emoji).
 * - **Shader compilation** — `compileShaders()` loads GLSL sources from
 *   `BinaryData` and delegates to `GLShaderCompiler::compile()`.
 * - **Buffer creation** — `createBuffers()` allocates the VAO, unit-quad VBO,
 *   and instance VBO.
 * - **Atlas upload** — `uploadStagedBitmaps()` drains the `GlyphAtlas` upload
 *   queue and issues `glTexSubImage2D` calls for each staged bitmap.
 *
 * ### Anonymous namespace helper
 *
 * `createAtlasTexture()` allocates and configures a 2-D OpenGL texture with
 * linear filtering and clamp-to-edge wrapping.  It is used to create both the
 * mono (R8) and emoji (RGBA8) atlas textures during context initialisation.
 *
 * ### Draw order
 *
 * ```
 * renderOpenGL()
 *   ├─ uploadStagedBitmaps()          // GL THREAD: glTexSubImage2D
 *   ├─ drawBackgrounds()              // GL THREAD: background.vert / background.frag
 *   ├─ drawInstances(mono,  false)    // GL THREAD: glyph.vert / glyph_mono.frag
 *   └─ drawInstances(emoji, true)     // GL THREAD: glyph.vert / glyph_emoji.frag
 * ```
 *
 * @note All functions in this file run on the **GL THREAD** unless explicitly
 *       noted otherwise.
 *
 * @see Terminal::Render::OpenGL
 * @see GLShaderCompiler
 * @see GlyphAtlas
 * @see TerminalGLDraw.cpp
 */
#include "Screen.h"
#include <array>

namespace
{

/**
 * @brief Allocate and configure a 2-D OpenGL texture for atlas use.
 *
 * Creates a texture with:
 * - Linear minification and magnification filters.
 * - Clamp-to-edge wrapping on both axes.
 * - Storage allocated via `glTexImage2D` with no initial data (`nullptr`).
 *
 * @param width          Texture width in texels.
 * @param height         Texture height in texels.
 * @param internalFormat Internal storage format (e.g. `GL_R8` for mono,
 *                       `GL_RGBA` for emoji).
 * @param format         Pixel data format for the initial upload (e.g.
 *                       `GL_RED`, `GL_RGBA`).
 * @return               The newly created OpenGL texture object ID.
 *
 * @note **GL THREAD** only.
 * @see Render::OpenGL::newOpenGLContextCreated()
 */
GLuint createAtlasTexture (int width, int height, GLenum internalFormat, GLenum format) noexcept
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

} // anonymous namespace

namespace Terminal
{ /*____________________________________________________________________________*/

/**
 * @brief Default constructor.
 *
 * All members are zero/null-initialised by their in-class defaults.
 * The OpenGL context is not created here; call `attachTo()` to start rendering.
 *
 * @note **MESSAGE THREAD**.
 */
Render::OpenGL::OpenGL() {}

/**
 * @brief Destructor — detaches the OpenGL context.
 *
 * Calls `openGLContext.detach()`, which triggers `openGLContextClosing()` on
 * the GL THREAD before the context is destroyed.  All GPU resources are
 * released inside that callback.
 *
 * @note **MESSAGE THREAD**.
 */
Render::OpenGL::~OpenGL()
{
    openGLContext.detach();
}

/**
 * @brief Attaches the OpenGL context to @p target and starts the render loop.
 *
 * Configures the context to require OpenGL 3.2 core profile, registers `this`
 * as the renderer, enables component painting, disables continuous repainting
 * (frames are triggered explicitly via `triggerRepaint()`), then attaches to
 * @p target.
 *
 * @param target  JUCE component to render into.
 *
 * @note **MESSAGE THREAD**.
 * @see triggerRepaint()
 */
void Render::OpenGL::attachTo (juce::Component& target) noexcept
{
    openGLContext.setOpenGLVersionRequired (juce::OpenGLContext::openGL3_2);
    openGLContext.setRenderer (this);
    openGLContext.setComponentPaintingEnabled (true);
    openGLContext.setContinuousRepainting (false);
    openGLContext.attachTo (target);
}

/**
 * @brief Detaches the OpenGL context from its current component.
 *
 * After this call, `openGLContextClosing()` will be invoked on the GL THREAD
 * to release GPU resources.
 *
 * @note **MESSAGE THREAD**.
 */
void Render::OpenGL::detach() noexcept
{
    openGLContext.detach();
}

/**
 * @brief Requests a repaint on the next vsync.
 *
 * Delegates to `juce::OpenGLContext::triggerRepaint()`.  Has no effect if the
 * context is not attached.
 *
 * @note **MESSAGE THREAD**.
 */
void Render::OpenGL::triggerRepaint() noexcept
{
    openGLContext.triggerRepaint();
}

/**
 * @brief Returns true if the OpenGL context is currently attached to a component.
 *
 * @return `true` if attached.
 *
 * @note **MESSAGE THREAD**.
 */
bool Render::OpenGL::isAttached() const noexcept
{
    return openGLContext.isAttached();
}

/**
 * @brief Called by JUCE when the OpenGL context is first created.
 *
 * Performs one-time GPU resource initialisation:
 * 1. Enables window transparency via `jreng::BackgroundBlur::enableGLTransparency()`.
 * 2. Compiles the mono, emoji, and background GLSL shader programs.
 * 3. Creates the VAO, unit-quad VBO, and instance VBO.
 * 4. Allocates the mono (R8) and emoji (RGBA8) atlas textures at
 *    `GlyphAtlas::atlasDimension()` × `GlyphAtlas::atlasDimension()`.
 * 5. Sets `contextReady` to `true` so the MESSAGE THREAD can detect readiness
 *    via `consumeContextReady()`.
 *
 * @note **GL THREAD**.
 * @see compileShaders()
 * @see createBuffers()
 * @see GlyphAtlas::atlasDimension()
 */
void Render::OpenGL::newOpenGLContextCreated()
{
    jreng::BackgroundBlur::enableGLTransparency();

    compileShaders();
    createBuffers();

    const int atlasDim { GlyphAtlas::atlasDimension() };
    monoAtlasTexture = createAtlasTexture (atlasDim, atlasDim, juce::gl::GL_R8, juce::gl::GL_RED);
    emojiAtlasTexture = createAtlasTexture (atlasDim, atlasDim, juce::gl::GL_RGBA, juce::gl::GL_RGBA);

    contextReady.store (true);
}

/**
 * @brief Called by JUCE on every vsync to render one frame.
 *
 * Skips rendering if any shader failed to compile.  Otherwise:
 *
 * 1. **Viewport** — queries the full framebuffer height via `glGetIntegerv`,
 *    converts the stored logical viewport to GL coordinates (Y-flipped), and
 *    calls `glViewport`.
 * 2. **Snapshot acquisition** — calls `Render::Mailbox::acquire()` to get the
 *    latest snapshot.  If a newer snapshot is available it replaces
 *    `currentSnapshot`; otherwise the previous snapshot is reused.
 * 3. **Clear** — clears the colour buffer to fully transparent black so the
 *    terminal composites correctly over the window background.
 * 4. **Draw** — if a snapshot is available, calls `uploadStagedBitmaps()`,
 *    then issues up to three draw passes: backgrounds, mono glyphs, emoji glyphs.
 *
 * @note **GL THREAD**.
 * @see uploadStagedBitmaps()
 * @see drawBackgrounds()
 * @see drawInstances()
 */
void Render::OpenGL::renderOpenGL()
{
    if (monoShader != nullptr and emojiShader != nullptr and backgroundShader != nullptr)
    {
        std::array<GLint, 4> fullViewport { 0, 0, 0, 0 };
        juce::gl::glGetIntegerv (juce::gl::GL_VIEWPORT, fullViewport.data());
        const int fullHeight { fullViewport.at (3) };

        const int glY { fullHeight - viewportY - viewportHeight };
        juce::gl::glViewport (viewportX, glY, viewportWidth, viewportHeight);

        if (snapshotMailbox != nullptr)
        {
            Snapshot* newest { snapshotMailbox->acquire() };

            if (newest != nullptr)
            {
                currentSnapshot = newest;
            }
        }

        juce::gl::glClearColor (
            0.0f, 0.0f, 0.0f, 0.0f); // Transparent clear for compositing
        juce::gl::glClear (juce::gl::GL_COLOR_BUFFER_BIT);

        if (currentSnapshot != nullptr)
        {
            uploadStagedBitmaps();

            juce::gl::glEnable (juce::gl::GL_BLEND);
            juce::gl::glBlendFunc (
                juce::gl::GL_SRC_ALPHA, juce::gl::GL_ONE_MINUS_SRC_ALPHA);

            // Draw cell backgrounds BEFORE glyphs so glyphs appear on top
            if (currentSnapshot->backgroundCount > 0)
            {
                drawBackgrounds (currentSnapshot->backgrounds.get(), currentSnapshot->backgroundCount);
            }

            if (currentSnapshot->monoCount > 0)
            {
                drawInstances (currentSnapshot->mono.get(), currentSnapshot->monoCount, false);
            }

            if (currentSnapshot->emojiCount > 0)
            {
                drawInstances (currentSnapshot->emoji.get(), currentSnapshot->emojiCount, true);
            }

            juce::gl::glDisable (juce::gl::GL_BLEND);
        }
    }
}

/**
 * @brief Called by JUCE when the OpenGL context is about to be destroyed.
 *
 * Releases all GPU resources in the correct order:
 * - Resets the three shader program `unique_ptr`s (calls `glDeleteProgram`).
 * - Deletes the mono and emoji atlas textures if non-zero.
 * - Deletes the VAO if non-zero.
 * - Deletes the quad VBO and instance VBO if non-zero.
 *
 * All handles are zeroed after deletion to prevent double-free.
 *
 * @note **GL THREAD**.
 */
void Render::OpenGL::openGLContextClosing()
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

/**
 * @brief Compiles the mono, emoji, and background GLSL shader programs.
 *
 * Loads GLSL source strings from `BinaryData`:
 * - `glyph.vert`        — shared vertex shader for both glyph passes.
 * - `glyph_mono.frag`   — fragment shader for monochrome (R8) glyph sampling.
 * - `glyph_emoji.frag`  — fragment shader for RGBA emoji glyph sampling.
 * - `background.vert`   — vertex shader for background colour quads.
 * - `background.frag`   — fragment shader for background colour quads.
 *
 * Delegates compilation and linking to `GLShaderCompiler::compile()`.  If the
 * current GL context is unavailable, all shader pointers remain null and
 * `renderOpenGL()` will skip rendering.
 *
 * @note **GL THREAD**.
 * @see GLShaderCompiler::compile()
 */
void Render::OpenGL::compileShaders()
{
    juce::String glyphVertShader { BinaryData::getString ("glyph.vert") };
    juce::String monoFragShader { BinaryData::getString ("glyph_mono.frag") };
    juce::String emojiFragShader { BinaryData::getString ("glyph_emoji.frag") };
    juce::String bgVertShader { BinaryData::getString ("background.vert") };
    juce::String bgFragShader { BinaryData::getString ("background.frag") };

    auto* glContext { juce::OpenGLContext::getCurrentContext() };
    if (glContext != nullptr)
    {
        monoShader = GLShaderCompiler::compile (*glContext, glyphVertShader, monoFragShader);
        emojiShader = GLShaderCompiler::compile (*glContext, glyphVertShader, emojiFragShader);
        backgroundShader = GLShaderCompiler::compile (*glContext, bgVertShader, bgFragShader);
    }
}

/**
 * @brief Creates the VAO, unit-quad VBO, and instance VBO.
 *
 * Allocates GPU buffer objects used by all three draw passes:
 *
 * - **VAO** — a single vertex array object shared across all draw calls.
 * - **Quad VBO** (`quadVBO`) — holds four 2-D vertices forming a unit quad
 *   in normalised [0, 1] space: `{(0,0), (1,0), (0,1), (1,1)}`.  Bound to
 *   attribute location 0 as a non-instanced `vec2`.  Uploaded once as
 *   `GL_STATIC_DRAW`.
 * - **Instance VBO** (`instanceVBO`) — allocated but not filled here.
 *   Filled each frame in `drawInstances()` and `drawBackgrounds()` via
 *   `GL_DYNAMIC_DRAW`.
 *
 * @note **GL THREAD**.
 * @see drawInstances()
 * @see drawBackgrounds()
 */
void Render::OpenGL::createBuffers()
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

/**
 * @brief Uploads any pending atlas bitmaps to the mono/emoji GPU textures.
 *
 * Calls `GlyphAtlas::consumeStagedBitmaps()` to drain the upload queue, then
 * iterates the returned array and issues one `glTexSubImage2D` call per entry.
 *
 * Steps:
 * 1. Sets `GL_UNPACK_ALIGNMENT` to 1 to handle non-power-of-two row widths
 *    (required for R8 mono bitmaps).
 * 2. For each `StagedBitmap`, selects the target texture (`emojiAtlasTexture`
 *    or `monoAtlasTexture`) based on `StagedBitmap::kind`.
 * 3. Binds the target texture and calls `glTexSubImage2D` with the staged
 *    region and pixel data.
 * 4. Restores `GL_UNPACK_ALIGNMENT` to 4 after all uploads.
 *
 * @note **GL THREAD**.
 * @see GlyphAtlas::consumeStagedBitmaps()
 * @see StagedBitmap
 */
void Render::OpenGL::uploadStagedBitmaps()
{
    if (glyphAtlas != nullptr)
    {
        juce::HeapBlock<StagedBitmap> stagedBitmaps;
        int stagedCount { 0 };
        glyphAtlas->consumeStagedBitmaps (stagedBitmaps, stagedCount);

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
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Terminal
