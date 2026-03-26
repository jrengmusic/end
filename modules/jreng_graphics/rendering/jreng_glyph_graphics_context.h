/**
 * @file jreng_glyph_graphics_context.h
 * @brief Software rendering backend for glyph and background draw calls via juce::Graphics.
 *
 * `GraphicsContext` is the CPU counterpart of `GLContext`.
 * It satisfies the same duck-type contract so that `Screen<Renderer>` and
 * `TextLayout::draw<GraphicsContext>` work identically with either backend.
 *
 * Glyph rendering uses `juce::Image` atlases that mirror the GL atlas layout.
 * All compositing uses direct `juce::Image::BitmapData` pixel writes.
 * A single `g.drawImageAt()` blit per frame.
 *
 * @par Thread contract
 * All methods must be called on the **MESSAGE THREAD** (inside `paint()`).
 *
 * @see jreng::Glyph::GLContext
 * @see jreng::Glyph::Render
 */
#pragma once

namespace jreng::Glyph
{

/**
 * @class GraphicsContext
 * @brief Software renderer compositing glyphs and backgrounds via direct BitmapData pixel writes.
 *
 * Mirrors the `GLContext` duck-type contract. `Screen<GraphicsContext>`
 * and `TextLayout::draw<GraphicsContext>` compile and work identically.
 *
 * @par Typical frame sequence
 * @code
 * // Once at startup:
 * renderer.createContext();
 *
 * // Each frame (MESSAGE THREAD, inside paint()):
 * renderer.setGraphicsContext (g);
 * renderer.push (x, y, w, h, fullHeight);
 * renderer.uploadStagedBitmaps (typeface);
 * renderer.drawBackgrounds (bgData, bgCount);
 * renderer.drawQuads (monoData, monoCount, false);
 * renderer.drawQuads (emojiData, emojiCount, true);
 * renderer.pop();
 *
 * // On shutdown:
 * renderer.closeContext();
 * @endcode
 *
 * @note **MESSAGE THREAD** only for all methods.
 */
class GraphicsContext
{
public:
    GraphicsContext()  = default;
    ~GraphicsContext() = default;

    // =========================================================================
    // Lifecycle
    // =========================================================================

    /**
     * @brief Allocate atlas images.
     *
     * Creates the mono (SingleChannel) and emoji (ARGB) atlas images using
     * `SoftwareImageType` to guarantee direct pixel access.
     *
     * @note **MESSAGE THREAD**.
     */
    void createContext() noexcept;

    /**
     * @brief Release atlas images.
     *
     * Replaces both atlas images with null images.
     *
     * @note **MESSAGE THREAD**.
     */
    void closeContext() noexcept;

    // =========================================================================
    // Per-frame operations
    // =========================================================================

    /**
     * @brief Drain the staged bitmap queue and copy pixels into atlas images.
     *
     * Calls `typeface.consumeStagedBitmaps()` to acquire bitmaps rasterised
     * since the last frame, then copies each one into the appropriate atlas
     * image (mono or emoji) via `juce::Image::BitmapData`.
     *
     * @param typeface  The typeface whose staged bitmaps should be uploaded.
     *
     * @note **MESSAGE THREAD**.
     */
    void uploadStagedBitmaps (jreng::Typeface& typeface) noexcept;

    /**
     * @brief Store viewport dimensions.
     *
     * @param width   Viewport width in physical pixels.
     * @param height  Viewport height in physical pixels.
     *
     * @note **MESSAGE THREAD**.
     */
    void setViewportSize (int width, int height) noexcept;

    /**
     * @brief Begin a frame: ensure render target size, store viewport offset.
     *
     * Allocates or resizes the render target image if the viewport dimensions
     * changed. The render target persists between frames; clearing is handled by prepareFrame().
     *
     * @param x           Physical pixel X origin of the viewport.
     * @param y           Physical pixel Y origin of the viewport (top-down).
     * @param w           Viewport width in physical pixels.
     * @param h           Viewport height in physical pixels.
     * @param fullHeight  Full window height in physical pixels (unused).
     *
     * @note **MESSAGE THREAD**.
     */
    void push (int x, int y, int w, int h, int fullHeight) noexcept;

    /**
     * @brief End a frame: blit the composited render target to the graphics context.
     *
     * Applies the inverse display-scale transform and draws the render target
     * image to the bound `juce::Graphics` context in a single operation.
     *
     * @note **MESSAGE THREAD**.
     */
    void pop() noexcept;

    /**
     * @brief Apply scroll shift and clear dirty rows on the persistent render target.
     *
     * Clears the render target for dirty or scrolled rows.
     *
     * @param dirtyRows       Bitmask of rows that changed (bit per row).
     * @param scrollDelta     Lines scrolled up since last frame.
     * @param cellHeight      Physical pixel height of one cell row.
     * @param totalRows       Number of visible rows.
     * @param scrollOffset    Current viewport scrollback offset.
     *
     * @note **MESSAGE THREAD**.
     */
    void prepareFrame (const uint64_t* dirtyRows, int scrollDelta,
                        int cellHeight, int totalRows, int scrollOffset) noexcept;

    /**
     * @brief Draw positioned glyph quads from a `Render::Quad` array.
     *
     * For each quad: reads glyph pixels from the atlas via BitmapData and
     * composites onto the render target using premultiplied alpha blending.
     *
     * @param data     Pointer to the array of `Render::Quad` instances.
     * @param count    Number of instances to draw.
     * @param isEmoji  `true` to use the ARGB emoji atlas.
     *
     * @note **MESSAGE THREAD**.
     */
    void drawQuads (const Render::Quad* data, int count, bool isEmoji) noexcept;

    /**
     * @brief Draw background rectangles from a `Render::Background` array.
     *
     * For each background: fills rectangles on the render target via direct
     * BitmapData pixel writes.
     *
     * @param data   Pointer to the array of `Render::Background` instances.
     * @param count  Number of instances to draw.
     *
     * @note **MESSAGE THREAD**.
     */
    void drawBackgrounds (const Render::Background* data, int count) noexcept;

    // =========================================================================
    // TextLayout duck-type (setFont + drawGlyphs)
    // =========================================================================

    /**
     * @brief Set the current font for subsequent drawGlyphs calls.
     * @param font  Lightweight font carrying typeface, size, style, emoji flag.
     * @note **MESSAGE THREAD**.
     */
    void setFont (jreng::Font& font) noexcept;

    /**
     * @brief Draw positioned glyphs using the current font.
     *
     * For each glyph: calls `currentFont->getGlyph(code)` for atlas lookup,
     * computes source and destination rects, and blits from the atlas image.
     *
     * @param glyphCodes  Array of font-internal glyph indices.
     * @param positions   Array of baseline anchor points in pixel space.
     * @param count       Number of elements in both arrays.
     * @note **MESSAGE THREAD**.
     */
    void drawGlyphs (const uint16_t* glyphCodes,
                      const juce::Point<float>* positions,
                      int count) noexcept;

    // =========================================================================
    // State queries
    // =========================================================================

    /**
     * @brief Returns `true` if atlas images are allocated and ready for drawing.
     * @return `true` if the renderer is ready.
     * @note **MESSAGE THREAD**.
     */
    bool isReady() const noexcept;

    /**
     * @brief Compile-time atlas texture side length in texels.
     * @return 2048.
     */
    static constexpr int getAtlasDimension() noexcept
    {
        return static_cast<int> (AtlasSize::compact);
    }

    // =========================================================================
    // Graphics context binding
    // =========================================================================

    /**
     * @brief Bind the juce::Graphics context for the current frame.
     *
     * Must be called before `push()`. The pointer is valid only for
     * the duration of the current `paint()` call.
     *
     * @param g  The graphics context from `Component::paint()`.
     * @note **MESSAGE THREAD**.
     */
    void setGraphicsContext (juce::Graphics& g) noexcept;

private:
    // =========================================================================
    // Private helpers
    // =========================================================================

    /**
     * @brief Composite a mono glyph from the atlas onto the render target.
     *
     * Reads alpha coverage from the mono atlas, multiplies by the foreground
     * colour, and blends onto the render target using premultiplied src-over.
     *
     * @param targetData  Locked BitmapData of the render target (readWrite).
     * @param atlasData   Locked BitmapData of the mono atlas (readOnly).
     * @param quad        The quad describing position, UVs, and foreground colour.
     */
    void compositeMonoGlyph (juce::Image::BitmapData& targetData,
                              const juce::Image::BitmapData& atlasData,
                              const Render::Quad& quad) noexcept;

    /**
     * @brief Composite an emoji glyph from the atlas onto the render target.
     *
     * Reads premultiplied ARGB pixels from the emoji atlas and blends onto
     * the render target using premultiplied src-over.
     *
     * @param targetData  Locked BitmapData of the render target (readWrite).
     * @param atlasData   Locked BitmapData of the emoji atlas (readOnly).
     * @param quad        The quad describing position and UVs.
     */
    void compositeEmojiGlyph (juce::Image::BitmapData& targetData,
                               const juce::Image::BitmapData& atlasData,
                               const Render::Quad& quad) noexcept;

    // =========================================================================
    // Data
    // =========================================================================

    /** @brief Shared mono glyph atlas (SingleChannel, alpha coverage). Created by the first instance, released by the last. */
    static juce::Image sharedMonoAtlas;

    /** @brief Shared emoji glyph atlas (ARGB, full colour). Created by the first instance, released by the last. */
    static juce::Image sharedEmojiAtlas;

    /** @brief Reference count for shared atlas images. Incremented in createContext, decremented in closeContext. */
    static int sharedAtlasRefCount;

    /** @brief True if this instance called createContext(). Guards closeContext() from unmatched decrement. */
    bool contextInitialised { false };

    /** @brief Viewport-sized ARGB render target for compositing. Blitted to Graphics in pop(). */
    juce::Image renderTarget;

    /** @brief Physical pixel X offset of the viewport, stored in push(). */
    int frameOffsetX { 0 };

    /** @brief Physical pixel Y offset of the viewport, stored in push(). */
    int frameOffsetY { 0 };

    /** @brief Previous frame's scroll offset for detecting viewport scroll-back changes. */
    int previousScrollOffset { -1 };

    /** @brief Graphics context bound for the current frame. nullptr between frames. */
    juce::Graphics* graphics { nullptr };

    /** @brief Current font set via setFont(); used by drawGlyphs(). nullptr until setFont() is called. */
    jreng::Font* currentFont { nullptr };

    /** @brief Viewport width in physical pixels. */
    int viewportWidth  { 0 };

    /** @brief Viewport height in physical pixels. */
    int viewportHeight { 0 };

    /** @brief Staged bitmap drain buffer, reused across frames to avoid reallocation. */
    juce::HeapBlock<jreng::Glyph::StagedBitmap> stagedBuffer;

    /** @brief Number of valid entries in stagedBuffer after consumeStagedBitmaps(). */
    int stagedCount { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GraphicsContext)
};

} // namespace jreng::Glyph
