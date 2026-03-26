/**
 * @file jreng_gl_context.h
 * @brief Standalone instanced quad renderer for glyph and background draw calls.
 *
 * `GLContext` owns all OpenGL resources required to draw glyph and
 * background instances: shader programs, atlas textures, VAO, and VBOs.
 * It exposes a minimal GL-thread API so any host component can delegate
 * instanced rendering without owning GL state directly.
 *
 * ### Shader pipeline
 *
 * Three shader programs are compiled once in `createContext()`:
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
 * `createContext`, `renderOpenGL`, `closeContext`, or equivalent).
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
 * @class GLContext
 * @brief Owns GL resources for instanced glyph and background quad rendering.
 *
 * Compile shaders and allocate textures/buffers by calling `createContext()`
 * when the OpenGL context is first established.  Call `closeContext()` before
 * the context is torn down to release all GPU objects.  Between those two calls,
 * use `uploadStagedBitmaps()`, `setViewportSize()`, `drawQuads()`, and
 * `drawBackgrounds()` once per frame on the GL thread.
 *
 * @par Typical frame sequence
 * @code
 * // Once per GL context lifetime:
 * renderer.createContext();
 *
 * // Each frame (GL THREAD):
 * renderer.setViewportSize (w, h);
 * renderer.uploadStagedBitmaps (atlas);
 * renderer.drawBackgrounds (bgData, bgCount);
 * renderer.drawQuads (monoData, monoCount, false);
 * renderer.drawQuads (emojiData, emojiCount, true);
 *
 * // On context teardown:
 * renderer.closeContext();
 * @endcode
 *
 * @note **GL THREAD** only for all methods.
 *
 * @see jreng::Glyph::Atlas
 * @see jreng::Glyph::Render::Quad
 * @see jreng::Glyph::Render::Background
 */
class GLContext
{
public:
    GLContext()  = default;
    ~GLContext() = default;

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
     * each `Atlas::getAtlasDimension() × Atlas::getAtlasDimension()` texels.
     *
     * @note **GL THREAD**.
     */
    void createContext() noexcept;

    /**
     * @brief Release all GL resources owned by this renderer.
     *
     * Resets the three shader unique_ptrs and deletes the atlas textures,
     * VAO, and VBOs.  Safe to call even if `createContext()` was never
     * called (all handles initialise to 0).
     *
     * @note **GL THREAD**.
     */
    void closeContext() noexcept;

    // =========================================================================
    // Per-frame operations
    // =========================================================================

    /**
     * @brief Drain the atlas upload queue and issue `glTexSubImage2D` calls.
     *
     * Calls `font.consumeStagedBitmaps()` to acquire any bitmaps rasterised
     * on the message thread since the last frame, then uploads each one to
     * the appropriate atlas texture (mono or emoji) via `glTexSubImage2D`.
     *
     * @param typeface  The typeface whose staged bitmaps should be uploaded.
     *
     * @note **GL THREAD**.
     * @see jreng::Typeface::consumeStagedBitmaps()
     */
    void uploadStagedBitmaps (jreng::Typeface& typeface) noexcept;

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
     * @brief Set up per-frame GL state for rendering.
     *
     * Configures the GL viewport with Y-flip for top-down coordinate space,
     * stores viewport dimensions for shader uniforms, and enables alpha blending.
     *
     * @param x           Physical pixel X origin of the viewport.
     * @param y           Physical pixel Y origin of the viewport (top-down).
     * @param w           Viewport width in physical pixels.
     * @param h           Viewport height in physical pixels.
     * @param fullHeight  Full window height in physical pixels (for GL Y-flip).
     *
     * @note **GL THREAD**.
     */
    void push (int x, int y, int w, int h, int fullHeight) noexcept;

    /**
     * @brief Tear down per-frame GL state after rendering.
     *
     * Disables alpha blending.
     *
     * @note **GL THREAD**.
     */
    void pop() noexcept;

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
     * @brief No-op for GL renderer. Required by the duck-type contract.
     *
     * `GraphicsContext` binds a `juce::Graphics` context here.
     * The GL renderer ignores this call — it renders via the OpenGL context.
     *
     * @param g  Unused.
     * @note **GL THREAD**.
     */
    void setGraphicsContext (juce::Graphics& g) noexcept;

    /**
     * @brief No-op for GL renderer. Required by the duck-type contract.
     * @note **GL THREAD**.
     */
    void prepareFrame (const uint64_t*, int, int, int, int) noexcept;

    /**
     * @brief Set the current font for subsequent drawGlyphs calls.
     * @param font  Lightweight font carrying typeface, size, style, emoji flag.
     * @note **GL THREAD**.
     */
    void setFont (jreng::Font& font) noexcept;

    /**
     * @brief Draw positioned glyphs using the current font.
     *
     * For each glyph: calls font.getGlyph() for atlas lookup, builds a
     * Render::Quad, batches, and submits via instanced draw.
     *
     * @param glyphCodes  Array of font-internal glyph indices.
     * @param positions   Array of baseline anchor points in pixel space.
     * @param count       Number of elements in both arrays.
     * @note **GL THREAD**.
     */
    void drawGlyphs (const uint16_t* glyphCodes,
                      const juce::Point<float>* positions,
                      int count) noexcept;

    /**
     * @brief Returns `true` if GL resources are initialised and ready for drawing.
     *
     * Checks that `monoShader` is non-null.  Returns `false` before
     * `createContext()` is called or after `closeContext()` clears it.
     *
     * @return `true` if the renderer is ready.
     *
     * @note **GL THREAD**.
     */
    bool isReady() const noexcept;

    /**
     * @brief Compile-time atlas texture side length in texels.
     *
     * @return 4096.
     */
    static constexpr int getAtlasDimension() noexcept
    {
        return static_cast<int> (AtlasSize::standard);
    }

private:
    // =========================================================================
    // Private helpers
    // =========================================================================

    /**
     * @brief Compile all three shader programs from the embedded GLSL sources.
     *
     * Sources are read from `jreng::Glyph::Shaders::*` constexpr strings.
     * Requires an active `juce::OpenGLContext`.  Called from `createContext()`.
     *
     * @note **GL THREAD**.
     */
    void compileShaders() noexcept;

    /**
     * @brief Create the VAO, static quad VBO, and dynamic instance VBO.
     *
     * Sets up a unit quad (four vertices at the corners of a 1×1 square)
     * as the base geometry for all instanced draw calls.  Called from
     * `createContext()`.
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

    /** @brief Compiled mono (R8) glyph shader program. Null until `createContext()`. */
    std::unique_ptr<juce::OpenGLShaderProgram> monoShader;

    /** @brief Compiled emoji (RGBA8) glyph shader program. Null until `createContext()`. */
    std::unique_ptr<juce::OpenGLShaderProgram> emojiShader;

    /** @brief Compiled background quad shader program. Null until `createContext()`. */
    std::unique_ptr<juce::OpenGLShaderProgram> backgroundShader;

    /** @brief Shared mono (R8) atlas texture. Created by the first instance, deleted by the last. */
    static GLuint sharedMonoAtlas;

    /** @brief Shared emoji (RGBA8) atlas texture. Created by the first instance, deleted by the last. */
    static GLuint sharedEmojiAtlas;

    /** @brief Reference count for shared atlas textures. Incremented in createContext, decremented in closeContext. */
    static int sharedAtlasRefCount;

    /** @brief True if this instance called createContext(). Guards closeContext(). */
    bool contextInitialised { false };

    /** @brief Vertex array object shared by all draw calls. 0 until `createContext()`. */
    GLuint vao         { 0 };

    /** @brief Static VBO holding the unit quad corner vertices. 0 until `createContext()`. */
    GLuint quadVBO     { 0 };

    /** @brief Dynamic VBO for per-instance data; re-uploaded every draw call. 0 until `createContext()`. */
    GLuint instanceVBO { 0 };

    /** @brief Viewport width in physical pixels; set via `setViewportSize()`. */
    int viewportWidth  { 0 };

    /** @brief Viewport height in physical pixels; set via `setViewportSize()`. */
    int viewportHeight { 0 };

    /** @brief Current font set via setFont(); used by drawGlyphs(). nullptr until setFont() is called. */
    jreng::Font* currentFont { nullptr };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GLContext)
};

} // namespace jreng::Glyph
