/**
 * @file jreng_gl_text_renderer.h
 * @brief Standalone instanced quad renderer for glyph and background draw calls.
 *
 * `GLTextRenderer` owns all OpenGL resources required to draw glyph and
 * background instances: shader programs, atlas textures, VAO, and VBOs.
 * It exposes a minimal GL-thread API so any host component can delegate
 * instanced rendering without owning GL state directly.
 *
 * ### Shader pipeline
 *
 * Three shader programs are compiled once in `contextCreated()`:
 * - **monoShader**       — R8 atlas path (regular + bold + italic glyphs).
 * - **emojiShader**      — RGBA8 atlas path (colour emoji).
 * - **backgroundShader** — coloured background rectangles.
 *
 * Shader sources are embedded as `constexpr const char*` in
 * `jreng_glyph_shaders.h` so the module has no dependency on app-level
 * `BinaryData`.
 *
 * ### Thread contract
 *
 * All methods must be called on the **GL THREAD** (inside
 * `glContextCreated`, `renderOpenGL`, `glContextClosing`, or equivalent).
 * No method is thread-safe; the caller is responsible for synchronisation.
 *
 * @see jreng::Glyph::Shaders
 * @see jreng::Glyph::Atlas
 * @see jreng::Glyph::Render
 */
#pragma once

namespace jreng::Glyph
{

/**
 * @class GLTextRenderer
 * @brief Owns GL resources for instanced glyph and background quad rendering.
 *
 * Compile shaders and allocate textures/buffers by calling `contextCreated()`
 * when the OpenGL context is first established.  Call `contextClosing()` before
 * the context is torn down to release all GPU objects.  Between those two calls,
 * use `uploadStagedBitmaps()`, `setViewportSize()`, `drawQuads()`, and
 * `drawBackgrounds()` once per frame on the GL thread.
 *
 * @par Typical frame sequence
 * @code
 * // Once per GL context lifetime:
 * renderer.contextCreated();
 *
 * // Each frame (GL THREAD):
 * renderer.setViewportSize (w, h);
 * renderer.uploadStagedBitmaps (atlas);
 * renderer.drawBackgrounds (bgData, bgCount);
 * renderer.drawQuads (monoData, monoCount, false);
 * renderer.drawQuads (emojiData, emojiCount, true);
 *
 * // On context teardown:
 * renderer.contextClosing();
 * @endcode
 *
 * @note **GL THREAD** only for all methods.
 *
 * @see jreng::Glyph::Atlas
 * @see jreng::Glyph::Render::Quad
 * @see jreng::Glyph::Render::Background
 */
class GLTextRenderer
{
public:
    GLTextRenderer()  = default;
    ~GLTextRenderer() = default;

    // =========================================================================
    // GL lifecycle
    // =========================================================================

    /**
     * @brief Compile shaders, allocate buffers, and create atlas textures.
     *
     * Must be called once when the OpenGL context is first created, before
     * any draw calls.  Compiles the three shader programs from the embedded
     * `jreng::Glyph::Shaders` constexpr strings, creates the shared VAO and
     * VBOs, and allocates a mono (R8) and an emoji (RGBA8) atlas texture,
     * each `Atlas::atlasDimension() × Atlas::atlasDimension()` texels.
     *
     * @note **GL THREAD**.
     */
    void contextCreated() noexcept;

    /**
     * @brief Release all GL resources owned by this renderer.
     *
     * Resets the three shader unique_ptrs and deletes the atlas textures,
     * VAO, and VBOs.  Safe to call even if `contextCreated()` was never
     * called (all handles initialise to 0).
     *
     * @note **GL THREAD**.
     */
    void contextClosing() noexcept;

    // =========================================================================
    // Per-frame operations
    // =========================================================================

    /**
     * @brief Drain the atlas upload queue and issue `glTexSubImage2D` calls.
     *
     * Calls `atlas.consumeStagedBitmaps()` to acquire any bitmaps rasterised
     * on the message thread since the last frame, then uploads each one to
     * the appropriate atlas texture (mono or emoji) via `glTexSubImage2D`.
     *
     * @param atlas  The glyph atlas whose staged bitmaps should be uploaded.
     *
     * @note **GL THREAD**.
     * @see jreng::Glyph::Atlas::consumeStagedBitmaps()
     */
    void uploadStagedBitmaps (Atlas& atlas) noexcept;

    /**
     * @brief Store the current viewport dimensions for use in shader uniforms.
     *
     * `drawQuads()` and `drawBackgrounds()` pass these values as
     * `uViewportSize` to transform pixel-space positions into NDC.
     *
     * @param width   Viewport width in physical pixels.
     * @param height  Viewport height in physical pixels.
     *
     * @note **GL THREAD**.
     */
    void setViewportSize (int width, int height) noexcept;

    /**
     * @brief Draw instanced glyph quads from a `Render::Quad` array.
     *
     * Uploads @p count entries from @p data to the instance VBO and issues
     * a single `glDrawArraysInstanced` call.  Uses the mono or emoji shader
     * and atlas texture depending on @p isEmoji.
     *
     * @param data     Pointer to the array of `Render::Quad` instances.
     * @param count    Number of instances to draw.
     * @param isEmoji  `true` to use the RGBA8 emoji atlas and shader.
     *
     * @note **GL THREAD**.
     */
    void drawQuads (const Render::Quad* data, int count, bool isEmoji) noexcept;

    /**
     * @brief Draw instanced background quads from a `Render::Background` array.
     *
     * Uploads @p count entries from @p data to the instance VBO and issues
     * a single `glDrawArraysInstanced` call using the background shader.
     *
     * @param data   Pointer to the array of `Render::Background` instances.
     * @param count  Number of instances to draw.
     *
     * @note **GL THREAD**.
     */
    void drawBackgrounds (const Render::Background* data, int count) noexcept;

    // =========================================================================
    // State queries
    // =========================================================================

    /**
     * @brief Returns `true` if GL resources are initialised and ready for drawing.
     *
     * Checks that `monoShader` is non-null.  Returns `false` before
     * `contextCreated()` is called or after `contextClosing()` clears it.
     *
     * @return `true` if the renderer is ready.
     *
     * @note **GL THREAD**.
     */
    bool isReady() const noexcept;

    /**
     * @brief Compile-time atlas texture side length in texels.
     *
     * Delegates to `Atlas::atlasDimension()`.
     *
     * @return 4096.
     */
    static constexpr int atlasDimension() noexcept
    {
        return Atlas::atlasDimension();
    }

private:
    // =========================================================================
    // Private helpers
    // =========================================================================

    /**
     * @brief Compile all three shader programs from the embedded GLSL sources.
     *
     * Sources are read from `jreng::Glyph::Shaders::*` constexpr strings.
     * Requires an active `juce::OpenGLContext`.  Called from `contextCreated()`.
     *
     * @note **GL THREAD**.
     */
    void compileShaders() noexcept;

    /**
     * @brief Create the VAO, static quad VBO, and dynamic instance VBO.
     *
     * Sets up a unit quad (four vertices at the corners of a 1×1 square)
     * as the base geometry for all instanced draw calls.  Called from
     * `contextCreated()`.
     *
     * @note **GL THREAD**.
     */
    void createBuffers() noexcept;

    /**
     * @brief Allocate a 2-D OpenGL texture for an atlas.
     *
     * Creates a `GL_TEXTURE_2D` with `GL_LINEAR` filtering and
     * `GL_CLAMP_TO_EDGE` wrap, then calls `glTexImage2D` to allocate
     * @p width × @p height texels of the requested format.
     *
     * @param width           Texture width in texels.
     * @param height          Texture height in texels.
     * @param internalFormat  OpenGL internal format (e.g. `GL_R8`, `GL_RGBA`).
     * @param format          Pixel data format matching @p internalFormat.
     * @return                The new texture handle, or 0 on failure.
     *
     * @note **GL THREAD**.
     */
    static GLuint createAtlasTexture (int width, int height,
                                      GLenum internalFormat,
                                      GLenum format) noexcept;

    // =========================================================================
    // Data
    // =========================================================================

    /** @brief Compiled mono (R8) glyph shader program. Null until `contextCreated()`. */
    std::unique_ptr<juce::OpenGLShaderProgram> monoShader;

    /** @brief Compiled emoji (RGBA8) glyph shader program. Null until `contextCreated()`. */
    std::unique_ptr<juce::OpenGLShaderProgram> emojiShader;

    /** @brief Compiled background quad shader program. Null until `contextCreated()`. */
    std::unique_ptr<juce::OpenGLShaderProgram> backgroundShader;

    /** @brief OpenGL handle for the mono (R8) atlas texture. 0 until `contextCreated()`. */
    GLuint monoAtlasTexture  { 0 };

    /** @brief OpenGL handle for the emoji (RGBA8) atlas texture. 0 until `contextCreated()`. */
    GLuint emojiAtlasTexture { 0 };

    /** @brief Vertex array object shared by all draw calls. 0 until `contextCreated()`. */
    GLuint vao         { 0 };

    /** @brief Static VBO holding the unit quad corner vertices. 0 until `contextCreated()`. */
    GLuint quadVBO     { 0 };

    /** @brief Dynamic VBO for per-instance data; re-uploaded every draw call. 0 until `contextCreated()`. */
    GLuint instanceVBO { 0 };

    /** @brief Viewport width in physical pixels; set via `setViewportSize()`. */
    int viewportWidth  { 0 };

    /** @brief Viewport height in physical pixels; set via `setViewportSize()`. */
    int viewportHeight { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GLTextRenderer)
};

} // namespace jreng::Glyph
