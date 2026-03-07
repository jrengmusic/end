/**
 * @file GlyphAtlas.h
 * @brief LRU glyph cache with staged GPU upload for the terminal renderer.
 *
 * GlyphAtlas is the central glyph-rasterization and texture-management system.
 * It owns two 4096×4096 OpenGL texture atlases — one for monochrome (R8) glyphs
 * and one for RGBA8 color emoji — and keeps them populated via an LRU eviction
 * policy.
 *
 * ### Architecture overview
 *
 * ```
 * MESSAGE THREAD                          GL THREAD
 * ─────────────────────────────────────   ──────────────────────────────
 * GlyphAtlas::getOrRasterize()            GlyphAtlas::consumeStagedBitmaps()
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
 * - **macOS** (`GlyphAtlas.mm`) — CoreText + CGBitmapContext.
 * - **Linux / Windows** (`GlyphAtlas.cpp`) — FreeType outline rendering.
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
 * @note `GlyphAtlas` itself is **not** thread-safe.  The only cross-thread
 *       operation is the upload queue, which is protected by `uploadMutex`.
 *
 * @see AtlasPacker
 * @see LRUGlyphCache
 * @see GlyphConstraint
 * @see BoxDrawing
 */

#pragma once

#include <JuceHeader.h>

#include "AtlasPacker.h"
#include "GlyphConstraint.h"

#include <ft2build.h>
#include FT_FREETYPE_H

#include <mutex>

/**
 * @struct GlyphKey
 * @brief Unique identity of a rasterized glyph in the atlas.
 *
 * A GlyphKey fully identifies one rasterized glyph instance.  Two glyphs with
 * the same codepoint but different font faces, sizes, or cell spans produce
 * distinct keys and are stored as separate atlas entries.
 *
 * @note Must be used on the **MESSAGE THREAD** only.
 *
 * @see LRUGlyphCache
 * @see GlyphAtlas
 */
// MESSAGE THREAD
struct GlyphKey
{
    /** @brief FreeType / CoreText glyph index within the font face. */
    uint32_t glyphIndex { 0 };

    /**
     * @brief Opaque font handle.
     *
     * On macOS this is a `CTFontRef`; on Linux/Windows it is an `FT_Face`.
     * Stored as `void*` to keep the header platform-agnostic.
     */
    void* fontFace { nullptr };

    /** @brief Logical point size at which the glyph was rasterized. */
    float fontSize { 0.0f };

    /**
     * @brief Number of terminal cells the glyph spans horizontally.
     *
     * Wide characters (e.g. CJK) span 2 cells; most glyphs span 1.
     * Nerd Font icons may span 1 or 2 depending on their constraint.
     */
    uint8_t cellSpan { 0 };

    /**
     * @brief Equality comparison for use as an unordered_map key.
     * @param other The key to compare against.
     * @return `true` if all four fields are identical.
     */
    bool operator== (const GlyphKey& other) const noexcept
    {
        return glyphIndex == other.glyphIndex 
            and fontFace == other.fontFace
            and fontSize == other.fontSize
            and cellSpan == other.cellSpan;
    }
};

namespace std
{
    /**
     * @brief std::hash specialization for GlyphKey.
     *
     * Combines hashes of all four fields with bit-shifted XOR to reduce
     * collisions across the typical glyph-key distribution.
     */
    template<>
    struct hash<GlyphKey>
    {
        /**
         * @brief Compute hash for a GlyphKey.
         * @param key The key to hash.
         * @return Combined hash value.
         */
        size_t operator() (const GlyphKey& key) const noexcept
        {
            return std::hash<uint32_t>{}(key.glyphIndex) 
                 ^ (std::hash<const void*>{}(key.fontFace) << 1)
                 ^ (std::hash<float>{}(key.fontSize) << 2)
                 ^ (std::hash<uint8_t>{}(key.cellSpan) << 3);
        }
    };
}

/**
 * @struct AtlasGlyph
 * @brief Rasterized glyph descriptor: atlas location and bearing metrics.
 *
 * Returned by `LRUGlyphCache::get()` and `LRUGlyphCache::insert()`.  The
 * renderer uses `textureCoordinates` to sample the atlas texture and
 * `bearingX` / `bearingY` to position the quad relative to the baseline.
 *
 * @see GlyphAtlas::getOrRasterize()
 * @see LRUGlyphCache
 */
struct AtlasGlyph
{
    /**
     * @brief Normalized UV rectangle within the atlas texture.
     *
     * Origin is top-left; x/y are the UV offset, width/height are the UV
     * extent.  Divide pixel coordinates by `atlasSize` (4096) to obtain these.
     */
    juce::Rectangle<float> textureCoordinates;

    /** @brief Width of the rasterized bitmap in physical pixels. */
    int widthPixels { 0 };

    /** @brief Height of the rasterized bitmap in physical pixels. */
    int heightPixels { 0 };

    /**
     * @brief Horizontal bearing: pixels from the pen origin to the left edge
     *        of the bitmap.
     *
     * Corresponds to `FT_GlyphSlot::bitmap_left` (FreeType) or
     * `CGRect::origin.x` (CoreText).  May be negative for glyphs that extend
     * left of the pen.
     */
    int bearingX { 0 };

    /**
     * @brief Vertical bearing: pixels from the baseline to the top edge of
     *        the bitmap.
     *
     * Corresponds to `FT_GlyphSlot::bitmap_top` (FreeType) or
     * `CGRect::origin.y + height` (CoreText).  Positive values place the
     * bitmap above the baseline.
     */
    int bearingY { 0 };
};

/**
 * @struct StagedBitmap
 * @brief Pixel data queued for upload to an OpenGL atlas texture.
 *
 * Produced on the **MESSAGE THREAD** by `GlyphAtlas::stageForUpload()` and
 * consumed on the **GL THREAD** by `GlyphAtlas::consumeStagedBitmaps()`.
 * Ownership of `pixelData` transfers to the consumer via `std::move`.
 *
 * @see GlyphAtlas::stageForUpload()
 * @see GlyphAtlas::consumeStagedBitmaps()
 */
struct StagedBitmap
{
    /**
     * @brief Identifies which atlas texture this bitmap targets.
     *
     * - `mono`  — R8 grayscale atlas (monochrome glyphs, box drawing).
     * - `emoji` — RGBA8 color atlas (color emoji).
     */
    enum class AtlasKind { mono, emoji };
    
    /** @brief Target atlas for this upload. */
    AtlasKind kind;

    /**
     * @brief Destination rectangle within the atlas texture, in texels.
     *
     * Passed directly to `glTexSubImage2D` as the x/y offset and width/height.
     */
    juce::Rectangle<int> region;

    /**
     * @brief Heap-allocated pixel data.
     *
     * Layout depends on `kind`:
     * - `mono`  — 1 byte per pixel (R8, linear grey).
     * - `emoji` — 4 bytes per pixel (BGRA on FreeType, premultiplied BGRA on
     *             CoreText with `kCGBitmapByteOrder32Host`).
     */
    juce::HeapBlock<uint8_t> pixelData;

    /** @brief Total byte count of `pixelData` (width × height × bpp). */
    size_t pixelDataSize { 0 };

    /**
     * @brief OpenGL pixel format constant for `glTexSubImage2D`.
     *
     * - `GL_RED`  for mono bitmaps.
     * - `GL_BGRA` for emoji bitmaps.
     */
    int format { 0 };
};

/**
 * @class LRUGlyphCache
 * @brief Fixed-capacity glyph cache with least-recently-used eviction.
 *
 * Wraps an `std::unordered_map<GlyphKey, CacheEntry>` and tracks a monotonic
 * frame counter.  On each cache hit, `lastAccessFrame` is updated to the
 * current frame.  When the map reaches `capacityLimit`, `evictLRU()` removes
 * the oldest 10 % of entries (those with the largest `currentFrame -
 * lastAccessFrame` delta).
 *
 * @par Eviction strategy
 * `evictLRU()` builds a temporary age list on the heap, partial-sorts it to
 * find the `targetRemove` oldest entries, then erases them from the map.  This
 * avoids allocating a sorted copy of the entire map on every eviction.
 *
 * @par Thread context
 * **MESSAGE THREAD** — not thread-safe.  All methods must be called from the
 * same thread.
 *
 * @see GlyphAtlas
 * @see GlyphKey
 * @see AtlasGlyph
 */
// MESSAGE THREAD - NOT thread-safe
class LRUGlyphCache
{
public:
    /**
     * @struct CacheEntry
     * @brief Internal storage for one cached glyph.
     */
    struct CacheEntry
    {
        /** @brief The rasterized glyph descriptor. */
        AtlasGlyph glyph;

        /**
         * @brief Frame number of the most recent access.
         *
         * Updated by `get()` on every cache hit.  Used by `evictLRU()` to
         * compute entry age as `currentFrame - lastAccessFrame`.
         */
        uint64_t lastAccessFrame { 0 };
    };

    /**
     * @brief Construct a cache with the given capacity limit.
     * @param capacityLimit Maximum number of entries before eviction triggers.
     *        The internal map is pre-reserved to half this value.
     */
    explicit LRUGlyphCache (uint32_t capacityLimit) noexcept
        : capacityLimit (capacityLimit)
    {
        cache.reserve (capacityLimit / 2);
    }
    
    /**
     * @brief Look up a glyph by key, updating its access frame on hit.
     * @param key The glyph identity to look up.
     * @return Pointer to the cached `AtlasGlyph`, or `nullptr` on miss.
     * @note The returned pointer is valid until the next `insert()` or
     *       `clear()` call (which may rehash the map).
     */
    AtlasGlyph* get (const GlyphKey& key) noexcept
    {
        auto it { cache.find (key) };
        AtlasGlyph* result { nullptr };

        if (it != cache.end())
        {
            it->second.lastAccessFrame = currentFrame;
            result = &it->second.glyph;
        }

        return result;
    }

    /**
     * @brief Insert a new glyph into the cache, evicting LRU entries if full.
     *
     * If the cache is at capacity, `evictLRU()` is called first to free space.
     * The glyph is then emplaced with `currentFrame` as its access time.
     *
     * @param key   The glyph identity (must not already be present).
     * @param glyph The rasterized glyph descriptor to store.
     * @return Pointer to the newly inserted `AtlasGlyph`, or `nullptr` if
     *         insertion failed (duplicate key).
     */
    AtlasGlyph* insert (const GlyphKey& key, const AtlasGlyph& glyph) noexcept
    {
        if (cache.size() >= capacityLimit)
        {
            evictLRU();
        }

        auto [it, success] = cache.emplace (
            key,
            CacheEntry { glyph, currentFrame }
        );

        AtlasGlyph* result { nullptr };

        if (success)
        {
            result = &it->second.glyph;
        }

        return result;
    }
    
    /**
     * @brief Advance the frame counter by one.
     *
     * Must be called once per rendered frame so that LRU age calculations
     * remain accurate.  Typically called from `GlyphAtlas::advanceFrame()`.
     */
    void advanceFrame() noexcept
    {
        ++currentFrame;
    }
    
    /**
     * @brief Remove all entries and reset the frame counter to zero.
     *
     * Called when the atlas texture is invalidated (e.g. on font size change
     * or GL context loss).
     */
    void clear() noexcept
    {
        cache.clear();
        currentFrame = 0;
    }
    
    /** @brief Current number of entries in the cache. */
    size_t getSize() const noexcept { return cache.size(); }

    /** @brief Maximum number of entries before eviction triggers. */
    size_t getCapacity() const noexcept { return capacityLimit; }

    /** @brief Monotonic frame counter; incremented by `advanceFrame()`. */
    uint64_t getCurrentFrame() const noexcept { return currentFrame; }
    
private:
    /** @brief Primary storage: glyph key → cache entry. */
    std::unordered_map<GlyphKey, CacheEntry> cache;

    /** @brief Maximum number of entries before LRU eviction triggers. */
    uint32_t capacityLimit { 0 };

    /** @brief Monotonic frame counter used to compute entry age. */
    uint64_t currentFrame { 0 };
    
    /**
     * @struct AgeEntry
     * @brief Temporary record used during LRU eviction sorting.
     */
    struct AgeEntry
    {
        /** @brief Key of the cache entry. */
        GlyphKey key;

        /** @brief Age in frames: `currentFrame - lastAccessFrame`. */
        uint64_t age;
    };

    /**
     * @brief Build a flat age list from the current cache contents.
     *
     * Allocates a `HeapBlock<AgeEntry>` of `cache.size()` elements and
     * populates it with each entry's key and computed age.
     *
     * @param ageList Output block; allocated inside this function.
     * @return Number of entries written (equals `cache.size()`).
     */
    int buildAgeList (juce::HeapBlock<AgeEntry>& ageList) const noexcept
    {
        const int count { static_cast<int> (cache.size()) };
        ageList.allocate (static_cast<size_t> (count), false);

        int idx { 0 };
        for (const auto& [key, entry] : cache)
        {
            ageList[idx].key = key;
            ageList[idx].age = currentFrame - entry.lastAccessFrame;
            ++idx;
        }

        return count;
    }

    /**
     * @brief Evict the oldest 10 % of cache entries.
     *
     * Uses `std::partial_sort` on a temporary age list to identify the
     * `targetRemove` (= max(1, capacity/10)) oldest entries, then erases them
     * from the map.  This keeps the eviction cost proportional to the number
     * of entries removed rather than the full cache size.
     */
    void evictLRU() noexcept
    {
        if (not cache.empty())
        {
            const uint32_t targetRemove { std::max (1u, capacityLimit / 10) };
            juce::HeapBlock<AgeEntry> ageList;
            const int ageCount { buildAgeList (ageList) };

            std::partial_sort (
                ageList.get(),
                ageList.get() + std::min (static_cast<size_t> (ageCount), static_cast<size_t> (targetRemove)),
                ageList.get() + ageCount,
                [] (const AgeEntry& a, const AgeEntry& b) { return a.age > b.age; }
            );

            const int removeCount { static_cast<int> (std::min (static_cast<size_t> (targetRemove), static_cast<size_t> (ageCount))) };

            for (int i { 0 }; i < removeCount; ++i)
            {
                cache.erase (ageList[i].key);
            }
        }
    }
};

/**
 * @class GlyphAtlas
 * @brief Two-atlas glyph cache: rasterizes on MESSAGE THREAD, uploads on GL THREAD.
 *
 * GlyphAtlas is the primary interface between the font subsystem and the OpenGL
 * renderer.  It manages two 4096×4096 texture atlases (mono R8 and emoji RGBA8),
 * rasterizes missing glyphs on demand, and stages the resulting bitmaps for
 * upload by the GL thread.
 *
 * @par Usage pattern
 * @code
 * // MESSAGE THREAD — called once per cell per frame
 * AtlasGlyph* g = atlas.getOrRasterize (key, fontHandle, isEmoji,
 *                                        constraint, cellW, cellH, baseline);
 * if (g != nullptr)
 *     renderer.submitQuad (*g);
 *
 * // GL THREAD — called once per frame before draw
 * juce::HeapBlock<StagedBitmap> pending;
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
 * When a `GlyphConstraint` is active, `rasterizeGlyph()` transforms the glyph
 * outline (FreeType) or CGContext (CoreText) to fit/cover/stretch the icon
 * within the cell according to the constraint's scale mode, alignment, and
 * padding.  The result is always stored at full cell dimensions so the renderer
 * can use a uniform quad size.
 *
 * @see LRUGlyphCache
 * @see AtlasPacker
 * @see GlyphConstraint
 * @see BoxDrawing
 * @see StagedBitmap
 */
// MESSAGE THREAD - Rasterization and staging
// GL THREAD - Texture upload
class GlyphAtlas
{
public:
    /**
     * @brief Construct the atlas, initializing both packers and LRU caches.
     *
     * Both atlases are set to `atlasSize × atlasSize` (4096×4096).  The mono
     * LRU is capped at 19 000 entries; the emoji LRU at 4 000 entries.
     */
    GlyphAtlas();

    /** @brief Default destructor; all resources are RAII-managed. */
    ~GlyphAtlas() = default;

    /**
     * @brief Return a cached glyph or rasterize it on demand.
     *
     * Looks up `key` in the appropriate LRU cache (mono or emoji).  On a miss,
     * calls `rasterizeGlyph()` to produce the bitmap, allocates an atlas region
     * via `AtlasPacker::allocate()`, stages the bitmap for GL upload, and
     * inserts the resulting `AtlasGlyph` into the cache.
     *
     * @param key        Unique glyph identity (index, face, size, span).
     * @param fontHandle Opaque font handle (`CTFontRef` on macOS, `FT_Face`
     *                   on Linux/Windows).
     * @param isEmoji    `true` to use the RGBA8 emoji atlas; `false` for mono.
     * @param constraint Nerd Font scaling/alignment descriptor.  Pass a
     *                   default-constructed `GlyphConstraint` for normal glyphs.
     * @param cellWidth  Terminal cell width in physical pixels.
     * @param cellHeight Terminal cell height in physical pixels.
     * @param baseline   Pixels from the cell top to the text baseline.
     * @return Pointer to the `AtlasGlyph` descriptor, or `nullptr` if the
     *         atlas is full or rasterization produced an empty bitmap.
     *
     * @note **MESSAGE THREAD** only.
     * @see rasterizeGlyph()
     * @see GlyphConstraint
     */
    AtlasGlyph* getOrRasterize (const GlyphKey& key, void* fontHandle, bool isEmoji,
                                 const GlyphConstraint& constraint,
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
     * @return Pointer to the `AtlasGlyph` descriptor, or `nullptr` if the
     *         mono atlas is full.
     *
     * @note **MESSAGE THREAD** only.
     * @see BoxDrawing::rasterize()
     * @see BoxDrawing::isProcedural()
     */
    AtlasGlyph* getOrRasterizeBoxDrawing (uint32_t codepoint, int cellWidth, int cellHeight, int baseline) noexcept;
    
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
     * @brief Advance the LRU frame counter for both caches.
     *
     * Must be called once per rendered frame so that LRU age calculations
     * remain accurate.  Typically called at the start of each paint cycle.
     *
     * @note **MESSAGE THREAD** only.
     */
    void advanceFrame() noexcept;
    
    /**
     * @brief Width of the atlas texture in texels.
     * @return Always `atlasSize` (4096).
     */
    int getAtlasWidth() const noexcept { return atlasWidth; }

    /**
     * @brief Height of the atlas texture in texels.
     * @return Always `atlasSize` (4096).
     */
    int getAtlasHeight() const noexcept { return atlasHeight; }

    /**
     * @brief Compile-time atlas dimension (width == height == 4096).
     * @return 4096.
     */
    static constexpr int atlasDimension() noexcept { return atlasSize; }  // NOLINT: method name matches constant intent
    
    /**
     * @struct CacheStats
     * @brief Snapshot of LRU cache occupancy for diagnostics.
     *
     * @see GlyphAtlas::getCacheStats()
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
    /** @brief Side length of both atlas textures in texels (4096). */
    inline static constexpr int atlasSize { 4096 };

    /** @brief Maximum entries in the mono LRU cache. */
    inline static constexpr uint32_t monoLruCapacity { 19000 };

    /** @brief Maximum entries in the emoji LRU cache. */
    inline static constexpr uint32_t emojiLruCapacity { 4000 };
    
    /**
     * @brief Rasterize a single glyph and stage it for atlas upload.
     *
     * Platform-specific implementation:
     * - **macOS** (`GlyphAtlas.mm`): CoreText + CGBitmapContext.
     * - **Linux/Windows** (`GlyphAtlas.cpp`): FreeType outline rendering.
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
     * @return Populated `AtlasGlyph` on success; zero-dimension glyph on
     *         failure (atlas full, rasterization error, etc.).
     *
     * @note **MESSAGE THREAD** only.
     */
    AtlasGlyph rasterizeGlyph (const GlyphKey& key, void* fontHandle, bool isEmoji,
                                const GlyphConstraint& constraint,
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
     * @note macOS (`GlyphAtlas.mm`) only.
     */
    static void flipBitmapVertically (uint8_t* data, int width, int height, int bytesPerPixel) noexcept;

    /** @brief Whether synthetic bold is applied during rasterization. */
    bool embolden { true };

    /** @brief Atlas texture width in texels (always `atlasSize`). */
    int atlasWidth { 0 };

    /** @brief Atlas texture height in texels (always `atlasSize`). */
    int atlasHeight { 0 };
    
    /** @brief Shelf packer for the mono (R8) atlas. */
    AtlasPacker monoPacker;

    /** @brief Shelf packer for the emoji (RGBA8) atlas. */
    AtlasPacker emojiPacker;
    
    /** @brief LRU cache for mono glyphs (capacity: 19 000). */
    LRUGlyphCache monoLRU;

    /** @brief LRU cache for emoji glyphs (capacity: 4 000). */
    LRUGlyphCache emojiLRU;
    
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
};
