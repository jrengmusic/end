/**
 * @file jreng_glyph_atlas.h
 * @brief LRU glyph cache with staged GPU upload for the terminal renderer.
 *
 * Atlas is the central glyph-rasterization and texture-management system.
 * It owns two 4096×4096 OpenGL texture atlases — one for monochrome (R8) glyphs
 * and one for RGBA8 color emoji — and keeps them populated via an LRU eviction
 * policy.
 *
 * ### Architecture overview
 *
 * ```
 * MESSAGE THREAD                          GL THREAD
 * ─────────────────────────────────────   ──────────────────────────────
 * Atlas::getOrRasterize()                 Atlas::consumeStagedBitmaps()
 *   └─ LRUGlyphCache::get()                 └─ glTexSubImage2D() per region
 *   └─ rasterizeGlyph()  (FT / CT)
 *   └─ AtlasPacker::allocate()
 *   └─ stageForUpload()  ──────────────► uploadQueue  (mutex-protected)
 * ```
 *
 * ### Two atlases
 * | Atlas      | Format | Capacity | Contents                          |
 * |------------|--------|----------|-----------------------------------|
 * | mono       | R8     | 19 000   | Monochrome glyphs, box drawing    |
 * | emoji      | RGBA8  | 4 000    | Color emoji bitmaps               |
 *
 * ### Platform backends
 * - **macOS** (`jreng_glyph_atlas.mm`) — CoreText + CGBitmapContext.
 * - **Linux / Windows** (`jreng_glyph_atlas.cpp`) — FreeType outline rendering.
 *
 * ### LRU eviction
 * Each atlas has an independent LRUGlyphCache.  When the cache reaches
 * capacity, `evictLRU()` removes the oldest 10 % of entries (by last-access
 * frame).  The atlas texture regions freed by eviction are reclaimed by
 * resetting the AtlasPacker on `clear()`.
 *
 * ### Staged upload protocol
 * Rasterization happens on the MESSAGE THREAD.  The resulting pixel data is
 * copied into a `StagedBitmap` and pushed onto `uploadQueue` under
 * `uploadMutex`.  The GL THREAD calls `consumeStagedBitmaps()` once per frame
 * to drain the queue and issue `glTexSubImage2D` calls.
 *
 * @note `Atlas` itself is **not** thread-safe.  The only cross-thread
 *       operation is the upload queue, which is protected by `uploadMutex`.
 *
 * @see AtlasPacker
 * @see jreng::Glyph::LRUCache
 * @see jreng::Glyph::Constraint
 * @see jreng::Glyph::BoxDrawing
 */

#pragma once

#include <mutex>

#if JUCE_MAC
    #include <CoreGraphics/CoreGraphics.h>
#endif

namespace jreng::Glyph
{

/**
 * @brief Atlas texture dimension presets.
 *
 * - `standard` (4096) — GPU rendering via GLTextRenderer.
 * - `compact`  (2048) — CPU rendering via GraphicsTextRenderer. Better cache locality.
 */
enum class AtlasSize : int
{
    standard = 4096,
    compact  = 2048
};

/**
 * @class Atlas
 * @brief Two-atlas glyph cache: rasterizes on MESSAGE THREAD, uploads on GL THREAD.
 *
 * Atlas is the primary interface between the font subsystem and the OpenGL
 * renderer.  It manages two 4096×4096 texture atlases (mono R8 and emoji RGBA8),
 * rasterizes missing glyphs on demand, and stages the resulting bitmaps for
 * upload by the GL thread.
 *
 * @par Usage pattern
 * @code
 * // MESSAGE THREAD — called once per cell per frame
 * jreng::Glyph::Region* g = atlas.getOrRasterize (key, fontHandle, isEmoji,
 *                                               constraint, cellW, cellH, baseline);
 * if (g != nullptr)
 *     renderer.submitQuad (*g);
 *
 * // GL THREAD — called once per frame before draw
 * juce::HeapBlock<jreng::Glyph::StagedBitmap> pending;
 * int count { 0 };
 * atlas.consumeStagedBitmaps (pending, count);
 * for (int i = 0; i < count; ++i)
 *     uploadToTexture (pending[i]);
 * @endcode
 *
 * @par Thread context
 * - **MESSAGE THREAD**: `getOrRasterize()`, `getOrRasterizeBoxDrawing()`,
 *   `advanceFrame()`, `clear()`, `setEmbolden()`, `getCacheStats()`.
 * - **GL THREAD**: `consumeStagedBitmaps()`, `hasStagedBitmaps()`.
 * - The upload queue is protected by `uploadMutex`; all other state is
 *   MESSAGE THREAD only.
 *
 * @par Nerd Font icon layout
 * When a `Constraint` is active, `rasterizeGlyph()` transforms the glyph
 * outline (FreeType) or CGContext (CoreText) to fit/cover/stretch the icon
 * within the cell according to the constraint's scale mode, alignment, and
 * padding.  The result is always stored at full cell dimensions so the renderer
 * can use a uniform quad size.
 *
 * @see jreng::Glyph::LRUCache
 * @see AtlasPacker
 * @see jreng::Glyph::Constraint
 * @see jreng::Glyph::BoxDrawing
 * @see jreng::Glyph::StagedBitmap
 */
// MESSAGE THREAD - Rasterization and staging
// GL THREAD - Texture upload
class Atlas
{
public:
    /**
     * @brief Identifies which atlas texture a glyph targets.
     *
     * - `mono`  — R8 grayscale atlas (monochrome glyphs, box drawing).
     * - `emoji` — RGBA8 color atlas (color emoji).
     */
    using Type = jreng::Glyph::Type;

    /**
     * @brief Constructs an atlas with the specified dimension.
     * @param size  Preset atlas dimension.
     */
    explicit Atlas (AtlasSize size) noexcept;

    /**
     * @brief Constructs a standard-size atlas (4096×4096).
     *
     * Both atlases are set to `AtlasSize::standard` (4096×4096).  The mono
     * LRU is capped at 19 000 entries; the emoji LRU at 4 000 entries.
     */
    Atlas() : Atlas (AtlasSize::standard) {}

    /** @brief Destructor; releases pooled CGColorSpaceRef objects on macOS. */
    ~Atlas();

    /**
     * @brief Return a cached glyph or rasterize it on demand.
     *
     * Looks up `key` in the appropriate LRU cache (mono or emoji).  On a miss,
     * calls `rasterizeGlyph()` to produce the bitmap, allocates an atlas region
     * via `AtlasPacker::allocate()`, stages the bitmap for GL upload, and
     * inserts the resulting `Region` into the cache.
     *
     * @param key        Unique glyph identity (index, face, size, span).
     * @param fontHandle Opaque font handle (`CTFontRef` on macOS, `FT_Face`
     *                   on Linux/Windows).
     * @param isEmoji    `true` to use the RGBA8 emoji atlas; `false` for mono.
     * @param constraint Nerd Font scaling/alignment descriptor.  Pass a
     *                   default-constructed `Constraint` for normal glyphs.
     * @param cellWidth  Terminal cell width in physical pixels.
     * @param cellHeight Terminal cell height in physical pixels.
     * @param baseline   Pixels from the cell top to the text baseline.
     * @return Pointer to the `Region` descriptor, or `nullptr` if
     *         the atlas is full or rasterization produced an empty bitmap.
     *
     * @note **MESSAGE THREAD** only.
     * @see rasterizeGlyph()
     * @see Constraint
     */
    Region* getOrRasterize (const Key& key, void* fontHandle, bool isEmoji,
                            const Constraint& constraint,
                            int cellWidth, int cellHeight, int baseline) noexcept;

    /**
     * @brief Return a cached box-drawing glyph or rasterize it procedurally.
     *
     * Handles Unicode box-drawing characters (U+2500–U+257F), block elements
     * (U+2580–U+259F), and braille patterns (U+2800–U+28FF) by delegating to
     * `BoxDrawing::rasterize()`.  The result is stored in the mono atlas at
     * full cell dimensions with `bearingX = 0` and `bearingY = baseline`.
     *
     * @param codepoint  Unicode codepoint in the box-drawing / braille range.
     * @param cellWidth  Terminal cell width in physical pixels.
     * @param cellHeight Terminal cell height in physical pixels.
     * @param baseline   Pixels from the cell top to the text baseline.
     * @return Pointer to the `Region` descriptor, or `nullptr` if
     *         the mono atlas is full.
     *
     * @note **MESSAGE THREAD** only.
     * @see BoxDrawing::rasterize()
     * @see BoxDrawing::isProcedural()
     */
    Region* getOrRasterizeBoxDrawing (uint32_t codepoint, int cellWidth, int cellHeight, int baseline) noexcept;

    /**
     * @brief Enable or disable synthetic bold (outline embolden).
     *
     * When enabled, FreeType glyphs are processed with `FT_Outline_Embolden`
     * (1/64 px) before rendering; CoreText glyphs use fill+stroke mode with a
     * 1 pt stroke.  Has no effect on emoji or constrained icon glyphs.
     *
     * @param enabled `true` to enable synthetic bold; `false` to disable.
     * @note **MESSAGE THREAD** only.
     */
    void setEmbolden (bool enabled) noexcept { embolden = enabled; }

    /**
     * @brief Query whether synthetic bold is currently enabled.
     * @return `true` if embolden is active.
     */
    bool getEmbolden() const noexcept { return embolden; }

    /**
     * @brief Set the display scale factor used by the CoreText backend.
     *
     * The display scale (points-to-pixels ratio) is used when computing the
     * AA fringe padding in `rasterizeGlyph()` on macOS.  Should match the
     * current screen's backing scale factor.
     *
     * @param scale Display scale factor (e.g. 2.0 on a Retina display).
     * @note **MESSAGE THREAD** only.
     */
    void setDisplayScale (float scale) noexcept { displayScale = scale; }

    /**
     * @brief Invalidate all cached glyphs and reset both atlas packers.
     *
     * Clears both LRU caches and resets both `AtlasPacker` instances to empty.
     * The upload queue is also drained.  Call this when the font size changes
     * or the GL context is recreated.
     *
     * @note **MESSAGE THREAD** only.
     */
    void clear() noexcept;

    /**
     * @brief Resize the atlas and clear all cached glyphs.
     *
     * Sets the atlas dimension to @p size and calls clear() to reset
     * both LRU caches and atlas packers. All glyphs re-rasterize on demand.
     *
     * @param size  New atlas dimension.
     * @note **MESSAGE THREAD** only.
     */
    void setAtlasSize (AtlasSize size) noexcept
    {
        atlasWidth  = static_cast<int> (size);
        atlasHeight = static_cast<int> (size);
        clear();
    }

    /**
     * @brief Advance the LRU frame counter for both caches.
     *
     * Must be called once per rendered frame so that LRU age calculations
     * remain accurate.  Typically called at the start of each paint cycle.
     *
     * @note **MESSAGE THREAD** only.
     */
    void advanceFrame() noexcept;

    /**
     * @brief Returns the atlas dimension in texels.
     * @return Atlas side length (e.g. 4096 or 2048).
     */
    int getAtlasDimension() const noexcept { return atlasWidth; }

    /**
     * @struct CacheStats
     * @brief Snapshot of LRU cache occupancy for diagnostics.
     *
     * @see Atlas::getCacheStats()
     */
    struct CacheStats
    {
        /** @brief Current number of entries in the mono LRU cache. */
        size_t monoSize;

        /** @brief Maximum capacity of the mono LRU cache (19 000). */
        size_t monoCapacity;

        /** @brief Current number of entries in the emoji LRU cache. */
        size_t emojiSize;

        /** @brief Maximum capacity of the emoji LRU cache (4 000). */
        size_t emojiCapacity;

        /** @brief Current frame counter (from the mono LRU). */
        uint64_t frameCount;
    };

    /**
     * @brief Return a snapshot of LRU cache occupancy.
     *
     * Useful for debug overlays and performance monitoring.
     *
     * @return `CacheStats` with current sizes, capacities, and frame count.
     * @note **MESSAGE THREAD** only.
     */
    CacheStats getCacheStats() const noexcept
    {
        return {
            monoLRU.getSize(),
            monoLRU.getCapacity(),
            emojiLRU.getSize(),
            emojiLRU.getCapacity(),
            monoLRU.getCurrentFrame()
        };
    }

    // ===========================================================================
    // Upload Queue Access (GL THREAD)
    // ===========================================================================

    /**
     * @brief Drain the staged-bitmap queue for GL upload.
     *
     * Transfers ownership of the entire upload queue to the caller under
     * `uploadMutex`, then resets the internal queue to empty.  The caller
     * (GL thread) is responsible for issuing `glTexSubImage2D` for each
     * `StagedBitmap` in `out[0..outCount-1]`.
     *
     * @param[out] out      Receives the `HeapBlock<StagedBitmap>` array.
     * @param[out] outCount Number of valid entries in `out`.
     *
     * @note **GL THREAD** only.
     * @see hasStagedBitmaps()
     * @see StagedBitmap
     */
    void consumeStagedBitmaps (juce::HeapBlock<StagedBitmap>& out, int& outCount) noexcept
    {
        const std::lock_guard<std::mutex> lock (uploadMutex);
        out = std::move (uploadQueue);
        outCount = uploadCount;
        uploadQueue.allocate (static_cast<size_t> (uploadCapacity), true);
        uploadCount = 0;
    }

    /**
     * @brief Check whether any bitmaps are waiting for GL upload.
     *
     * Cheap predicate for the GL thread to skip `consumeStagedBitmaps()` when
     * there is nothing to upload.
     *
     * @return `true` if at least one `StagedBitmap` is queued.
     * @note **GL THREAD** only (acquires `uploadMutex`).
     */
    bool hasStagedBitmaps() const noexcept
    {
        const std::lock_guard<std::mutex> lock (uploadMutex);
        return uploadCount > 0;
    }

private:
    /** @brief Maximum entries in the mono LRU cache. */
    inline static constexpr uint32_t monoLruCapacity { 19000 };

    /** @brief Maximum entries in the emoji LRU cache. */
    inline static constexpr uint32_t emojiLruCapacity { 4000 };

    /**
     * @brief Rasterize a single glyph and stage it for atlas upload.
     *
     * Platform-specific implementation:
     * - **macOS** (`jreng_glyph_atlas.mm`): CoreText + CGBitmapContext.
     * - **Linux/Windows** (`jreng_glyph_atlas.cpp`): FreeType outline rendering.
     *
     * When `constraint.isActive()` is `true`, the glyph outline is transformed
     * to fit the constraint's scale mode, alignment, and padding before
     * rendering.  The resulting bitmap is always `cellWidth × cellHeight` for
     * constrained glyphs, or the natural glyph dimensions otherwise.
     *
     * @param key        Glyph identity.
     * @param fontHandle Opaque font handle.
     * @param isEmoji    `true` for RGBA8 color rendering.
     * @param constraint Nerd Font layout descriptor.
     * @param cellWidth  Cell width in physical pixels.
     * @param cellHeight Cell height in physical pixels.
     * @param baseline   Pixels from cell top to baseline.
     * @return Populated `Region` on success; zero-dimension glyph
     *         on failure (atlas full, rasterization error, etc.).
     *
     * @note **MESSAGE THREAD** only.
     */
    Region rasterizeGlyph (const Key& key, void* fontHandle, bool isEmoji,
                           const Constraint& constraint,
                           int cellWidth, int cellHeight, int baseline) noexcept;

    /**
     * @brief Copy pixel data into the upload queue under `uploadMutex`.
     *
     * Grows the queue's `HeapBlock` capacity by doubling if needed, then
     * appends a new `StagedBitmap` with a deep copy of `pixelData`.
     *
     * @param pixelData Pointer to the source pixel buffer (not transferred).
     * @param width     Bitmap width in pixels.
     * @param height    Bitmap height in pixels.
     * @param region    Destination rectangle within the atlas texture.
     * @param isEmoji   `true` for RGBA8 (emoji atlas); `false` for R8 (mono).
     *
     * @note **MESSAGE THREAD** only (acquires `uploadMutex`).
     */
    void stageForUpload (uint8_t* pixelData, int width, int height, const juce::Rectangle<int>& region, bool isEmoji) noexcept;

    /**
     * @brief Flip a pixel buffer vertically in-place.
     *
     * CoreText renders into a CGBitmapContext with a bottom-left origin, so
     * the resulting bitmap is upside-down relative to OpenGL's top-left
     * convention.  This function swaps rows in-place using a temporary row
     * buffer.
     *
     * @param data          Pointer to the pixel buffer.
     * @param width         Image width in pixels.
     * @param height        Image height in pixels.
     * @param bytesPerPixel Bytes per pixel (1 for mono, 4 for RGBA).
     *
     * @note macOS (`jreng_glyph_atlas.mm`) only.
     */
    static void flipBitmapVertically (uint8_t* data, int width, int height, int bytesPerPixel) noexcept;

    /** @brief Whether synthetic bold is applied during rasterization. */
    bool embolden { true };

    /** @brief Display scale factor used by the CoreText backend for AA padding. */
    float displayScale { 1.0f };

    /** @brief Atlas texture width in texels (set from AtlasSize at construction). */
    int atlasWidth { 0 };

    /** @brief Atlas texture height in texels (set from AtlasSize at construction). */
    int atlasHeight { 0 };

    /** @brief Shelf packer for the mono (R8) atlas. */
    AtlasPacker monoPacker;

    /** @brief Shelf packer for the emoji (RGBA8) atlas. */
    AtlasPacker emojiPacker;

    /** @brief LRU cache for mono glyphs (capacity: 19 000). */
    LRUCache monoLRU;

    /** @brief LRU cache for emoji glyphs (capacity: 4 000). */
    LRUCache emojiLRU;

    /**
     * @brief Heap array of bitmaps waiting for GL upload.
     *
     * Grown by doubling when `uploadCount >= uploadCapacity`.  Protected by
     * `uploadMutex` for cross-thread access.
     */
    juce::HeapBlock<StagedBitmap> uploadQueue;

    /** @brief Number of valid entries currently in `uploadQueue`. */
    int uploadCount { 0 };

    /** @brief Allocated capacity of `uploadQueue` in entries. */
    int uploadCapacity { 0 };

    /**
     * @brief Mutex protecting `uploadQueue`, `uploadCount`, and `uploadCapacity`.
     *
     * Held briefly by `stageForUpload()` (MESSAGE THREAD) and
     * `consumeStagedBitmaps()` / `hasStagedBitmaps()` (GL THREAD).
     */
    mutable std::mutex uploadMutex;

#if JUCE_MAC
    /** @brief Pooled DeviceGray color space for mono rasterization (macOS). */
    CGColorSpaceRef monoColorSpace { nullptr };

    /** @brief Pooled DeviceRGB color space for emoji rasterization (macOS). */
    CGColorSpaceRef emojiColorSpace { nullptr };

    /** @brief Backing pixel buffer for the pooled mono CGBitmapContext (macOS). */
    juce::HeapBlock<uint8_t> monoPoolBuffer;

    /** @brief Pooled CGBitmapContext for mono rasterization; grows on high-watermark (macOS). */
    CGContextRef monoPoolContext { nullptr };

    /** @brief Current pooled mono context width in pixels (macOS). */
    int monoPoolWidth { 0 };

    /** @brief Current pooled mono context height in pixels (macOS). */
    int monoPoolHeight { 0 };

    /** @brief Backing pixel buffer for the pooled emoji CGBitmapContext (macOS). */
    juce::HeapBlock<uint8_t> emojiPoolBuffer;

    /** @brief Pooled CGBitmapContext for emoji rasterization; grows on high-watermark (macOS). */
    CGContextRef emojiPoolContext { nullptr };

    /** @brief Current pooled emoji context width in pixels (macOS). */
    int emojiPoolWidth { 0 };

    /** @brief Current pooled emoji context height in pixels (macOS). */
    int emojiPoolHeight { 0 };
#endif
};

} // namespace jreng::Glyph
