/**
 * @file ImageAtlas.h
 * @brief Shelf-packed RGBA8 texture atlas for inline terminal images.
 *
 * `Terminal::ImageAtlas` manages a 4096×4096 RGBA8 texture atlas that stages
 * and uploads inline terminal images (e.g. Sixel, iTerm2 inline image protocol)
 * to the GPU using the same lock-free Mailbox handoff pattern as `jam::Glyph::Packer`.
 *
 * @see Terminal::ImageRegion
 * @see jam::Glyph::AtlasPacker
 * @see jam::Glyph::StagedBatch
 * @see jam::Mailbox
 */

#pragma once
#include <unordered_map>
#include <array>
#include <JuceHeader.h>

namespace Terminal
{ /*____________________________________________________________________________*/

/**
 * @struct ImageRegion
 * @brief Describes the atlas location and pixel dimensions of a staged image.
 *
 * Returned by `ImageAtlas::lookup()` to provide the renderer with enough
 * information to emit a textured quad: normalised UV coordinates for the
 * atlas texture sample and the original pixel dimensions for aspect-correct
 * scaling.
 *
 * @see ImageAtlas
 */
struct ImageRegion
{
    juce::Rectangle<float> uv;   ///< Normalised UV coordinates in the atlas.
    int widthPx  { 0 };          ///< Source image width in pixels.
    int heightPx { 0 };          ///< Source image height in pixels.
};

/**
 * @class ImageAtlas
 * @brief Shelf-packed RGBA8 texture atlas for inline terminal images.
 *
 * Follows the same ownership, staging, and upload pattern as the glyph atlas:
 * - Owned by `MainComponent` as a value member.
 * - Passed by reference through Tabs → Panes → Display → Screen.
 * - MESSAGE THREAD calls `stage()` then `publishStagedUploads()`.
 * - GL THREAD calls `consumeStagedUploads()` and uploads via `glTexSubImage2D`.
 * - CPU path mirrors via `getMutableCPUAtlas()` and direct `BitmapData` writes.
 *
 * Uses `jam::Glyph::AtlasPacker` for shelf packing, `jam::Mailbox<jam::Glyph::StagedBatch>`
 * for lock-free handoff, and an LRU map for budget-based eviction.
 *
 * @par Thread contract
 * | Method                     | Thread         |
 * |----------------------------|----------------|
 * | `stage()`                  | MESSAGE THREAD |
 * | `publishStagedUploads()`   | MESSAGE THREAD |
 * | `lookup()`                 | MESSAGE THREAD |
 * | `release()`                | MESSAGE THREAD |
 * | `evictIfNeeded()`          | MESSAGE THREAD |
 * | `advanceFrame()`           | MESSAGE THREAD |
 * | `ensureCPUAtlas()`         | MESSAGE THREAD |
 * | `consumeStagedUploads()`   | GL THREAD      |
 * | `createGLTexture()`        | GL THREAD      |
 * | `destroyGLTexture()`       | GL THREAD      |
 * | `getGLTexture()`           | GL THREAD      |
 *
 * @see ImageRegion
 * @see jam::Glyph::AtlasPacker
 * @see jam::Glyph::StagedBatch
 * @see jam::Mailbox
 */
class ImageAtlas
{
public:
    /**
     * @brief Constructs an ImageAtlas with a configurable byte budget.
     *
     * Initialises the shelf packer for a 4096×4096 RGBA8 atlas and sets
     * the eviction budget.  No GPU texture is allocated here; call
     * `createGLTexture()` on the GL thread when a GL context is available.
     *
     * @param budgetBytes  Maximum total RGBA pixel data to retain before
     *                     evicting old entries.  Default: 32 MiB.
     *
     * @note MESSAGE THREAD — must be constructed on the message thread.
     */
    explicit ImageAtlas (int budgetBytes = 32 * 1024 * 1024) noexcept;

    /** @brief Destructor. Call `destroyGLTexture()` on the GL thread before destroying. */
    ~ImageAtlas() = default;

    // =========================================================================
    // MESSAGE THREAD
    // =========================================================================

    /**
     * @brief Stages an RGBA8 image for upload and returns its stable image ID.
     *
     * Auto-assigns an ID via the internal counter, then delegates to `stageImpl()`.
     * Returns 0 on persistent failure (atlas full after eviction).
     *
     * @param rgba      Pointer to tightly-packed RGBA8 pixel data (4 bytes/pixel).
     * @param widthPx   Source image width in pixels.
     * @param heightPx  Source image height in pixels.
     * @return Stable image ID for subsequent `lookup()` and `release()` calls,
     *         or 0 on failure.
     *
     * @note MESSAGE THREAD only.
     * @see stageImpl()
     */
    uint32_t stage (const uint8_t* rgba, int widthPx, int heightPx) noexcept;

    /**
     * @brief Stages an RGBA8 image using a caller-provided image ID.
     *
     * Validates @p imageId is non-zero, then delegates to `stageImpl()`.
     * Used by the pending image pipeline: the READER THREAD reserves @p imageId
     * via `Grid::reserveImageId()` and writes it into Grid cells; the MESSAGE
     * THREAD then calls this method to bind the RGBA pixels to that same ID.
     *
     * @param imageId   Pre-reserved image ID (from `Grid::reserveImageId()`).
     * @param rgba      Pointer to tightly-packed RGBA8 pixel data (4 bytes/pixel).
     * @param widthPx   Source image width in pixels.
     * @param heightPx  Source image height in pixels.
     * @return `true` on success, `false` on failure.
     *
     * @note MESSAGE THREAD only.
     * @see stageImpl()
     * @see Grid::reserveImageId()
     * @see Grid::getDecodedImage()
     */
    bool stageWithId (uint32_t imageId, const uint8_t* rgba,
                      int widthPx, int heightPx) noexcept;

    /**
     * @brief Publishes the current write batch to the GL THREAD via atomic exchange.
     *
     * If the active write slot has at least one staged bitmap, performs a
     * lock-free `Mailbox::write()` (acq_rel) that hands the batch pointer to
     * the GL THREAD and retrieves any previously returned batch.  The write
     * slot then flips to the other buffer slot.  If the returned batch still
     * contains entries (GL thread has not yet processed the previous frame),
     * those entries are absorbed into the new write slot to prevent data loss.
     *
     * Must be called once per frame on the MESSAGE THREAD after all `stage()`
     * calls for that frame.
     *
     * @note MESSAGE THREAD only.
     * @see consumeStagedUploads()
     */
    void publishStagedUploads() noexcept;

    /**
     * @brief Looks up the region for a previously staged image.
     *
     * Updates the entry's `lastAccessFrame` for LRU accounting and returns
     * a const pointer to its `ImageRegion`.
     *
     * @param imageId  ID returned by a prior `stage()` call.
     * @return Const pointer to the `ImageRegion`, or `nullptr` if not found.
     *
     * @note MESSAGE THREAD only.
     */
    const ImageRegion* lookup (uint32_t imageId) noexcept;

    /**
     * @brief Removes an image from the regions map and frees its byte accounting.
     *
     * Does NOT free atlas texture space — the shelf packer has no individual-
     * region free operation.  Freed space is reclaimed only when `evictIfNeeded()`
     * resets the packer.
     *
     * @param imageId  ID returned by a prior `stage()` call.
     *
     * @note MESSAGE THREAD only.
     */
    void release (uint32_t imageId) noexcept;

    /**
     * @brief Clears all regions and resets the packer if total bytes exceed the budget.
     *
     * Eviction strategy: on budget overflow, clear everything and let images
     * re-stage on demand.  The packer is reset so all shelf space is reclaimed.
     *
     * @note MESSAGE THREAD only.
     */
    void evictIfNeeded() noexcept;

    /**
     * @brief Advances the LRU frame counter.
     *
     * Must be called once per rendered frame so that LRU age calculations
     * in `lookup()` remain accurate.  Typically called at the start of each
     * paint cycle alongside `jam::Glyph::Packer::advanceFrame()`.
     *
     * @note MESSAGE THREAD only.
     */
    void advanceFrame() noexcept;

    /**
     * @brief Updates the byte budget used by `evictIfNeeded()`.
     *
     * Called from `MainComponent::applyConfig()` after the Lua engine has loaded
     * the user's `nexus.image.atlas_budget` value.  Does not trigger immediate
     * eviction — the new budget takes effect on the next `evictIfNeeded()` call.
     *
     * @param bytes  New budget in bytes.  Must be positive.
     *
     * @note MESSAGE THREAD only.
     */
    void setBudgetBytes (int bytes) noexcept;

    /**
     * @brief Ensures the CPU atlas image is allocated.
     *
     * Creates a 4096×4096 ARGB `SoftwareImageType` image if not already valid.
     * Must be called before writing to the CPU atlas via `getMutableCPUAtlas()`.
     *
     * @note MESSAGE THREAD only.
     */
    void ensureCPUAtlas() noexcept;

    /**
     * @brief Returns a const reference to the CPU atlas image.
     *
     * The image may be invalid if `ensureCPUAtlas()` has not been called.
     *
     * @return Const reference to the CPU atlas `juce::Image`.
     *
     * @note MESSAGE THREAD only.
     */
    const juce::Image& getCPUAtlas() const noexcept;

    /**
     * @brief Returns a mutable reference to the CPU atlas image.
     *
     * Used by the CPU render path to write staged RGBA pixels directly into
     * the atlas via `juce::Image::BitmapData`.  Call `ensureCPUAtlas()` first.
     *
     * @return Mutable reference to the CPU atlas `juce::Image`.
     *
     * @note MESSAGE THREAD only.
     */
    juce::Image& getMutableCPUAtlas() noexcept;

    // =========================================================================
    // GL THREAD
    // =========================================================================

    /**
     * @brief Receives the current `StagedBatch` from the Mailbox for GL upload.
     *
     * Performs a lock-free atomic exchange on `uploadMailbox`.  Returns the
     * batch pointer published by the MESSAGE THREAD, or `nullptr` if none is
     * pending.  The caller is responsible for iterating `batch->items`, issuing
     * `glTexSubImage2D` for each entry, and calling `batch->reset()` after processing.
     *
     * @return Pointer to the `StagedBatch`, or `nullptr` if none was pending.
     *
     * @note GL THREAD only.
     */
    jam::Glyph::StagedBatch* consumeStagedUploads() noexcept;

    /**
     * @brief Allocates the GL texture for the image atlas.
     *
     * Creates a 4096×4096 `GL_RGBA` texture with `GL_LINEAR` filtering and
     * `GL_CLAMP_TO_EDGE` wrapping and zero-initialises it.
     *
     * @note GL THREAD only.
     */
    void createGLTexture() noexcept;

    /**
     * @brief Releases the GL texture.
     *
     * Calls `glDeleteTextures` if the handle is non-zero and resets it to 0.
     *
     * @note GL THREAD only.
     */
    void destroyGLTexture() noexcept;

    /**
     * @brief Returns the GL texture name.
     *
     * Returns 0 if `createGLTexture()` has not been called.
     *
     * @return The GL texture handle, or 0.
     *
     * @note GL THREAD only.
     */
    GLuint getGLTexture() const noexcept;

private:
    // =========================================================================
    // Private methods
    // =========================================================================

    /**
     * @brief Shared implementation body for `stage()` and `stageWithId()`.
     *
     * Allocates a packer region, deep-copies @p rgba into a `StagedBitmap`,
     * appends it to the active write batch, computes normalised UV coordinates,
     * and stores an `LRUEntry` under @p imageId.  Calls `evictIfNeeded()` and
     * retries once if the packer is initially full.
     *
     * @param imageId   ID under which to register the entry.
     * @param rgba      Pointer to tightly-packed RGBA8 pixel data (4 bytes/pixel).
     * @param widthPx   Source image width in pixels.
     * @param heightPx  Source image height in pixels.
     * @return `true` on success, `false` if the atlas is full after eviction.
     *
     * @note MESSAGE THREAD only.
     */
    bool stageImpl (uint32_t imageId, const uint8_t* rgba,
                    int widthPx, int heightPx) noexcept;

    // =========================================================================
    // Private types
    // =========================================================================

    /**
     * @struct LRUEntry
     * @brief Per-image bookkeeping stored in the regions map.
     */
    struct LRUEntry
    {
        ImageRegion region;             ///< UV coordinates and pixel dimensions.
        uint64_t    lastAccessFrame { 0 }; ///< Frame counter at last lookup().
        int         sizeBytes       { 0 }; ///< RGBA byte footprint (widthPx * heightPx * 4).
    };

    // =========================================================================
    // Constants
    // =========================================================================

    /** @brief Atlas texture dimension in texels (width and height). */
    static constexpr int atlasDimension { 4096 };

    // =========================================================================
    // Data
    // =========================================================================

    /** @brief Shelf packer; allocates rectangular regions within the 4096×4096 atlas. */
    jam::Glyph::AtlasPacker packer;

    /** @brief LRU map from imageId to entry. */
    std::unordered_map<uint32_t, LRUEntry> regions;

    /** @brief Monotonically increasing image ID counter. Starts at 1; 0 means invalid. */
    uint32_t nextImageId { 1 };

    /** @brief Current frame counter, incremented by advanceFrame(). */
    uint64_t currentFrame { 0 };

    /** @brief Total RGBA bytes currently tracked across all live entries. */
    int totalBytes { 0 };

    /** @brief Maximum total RGBA bytes before eviction. */
    int budgetBytes { 0 };

    /**
     * @brief Lock-free atomic slot for `StagedBatch` handoff between threads.
     *
     * Written by `publishStagedUploads()` on the MESSAGE THREAD (acq_rel) and
     * read by `consumeStagedUploads()` on the GL THREAD (acq_rel).  No mutex.
     */
    jam::Mailbox<jam::Glyph::StagedBatch> uploadMailbox;

    /**
     * @brief Double-buffer of `StagedBatch` storage; indexed by `writeSlot`.
     *
     * `uploadBatches[writeSlot]` is the active write target for `stage()`.
     * After `publishStagedUploads()` hands it to the GL THREAD, `writeSlot`
     * flips to the other index so the MESSAGE THREAD always has a free slot.
     */
    std::array<jam::Glyph::StagedBatch, 2> uploadBatches;

    /** @brief Index (0 or 1) of the `StagedBatch` slot currently being written. */
    int writeSlot { 0 };

    /** @brief GL RGBA texture handle; 0 until `createGLTexture()` is called on the GL thread. */
    GLuint glTexture { 0 };

    /** @brief CPU mirror of the atlas; invalid until `ensureCPUAtlas()` is called. */
    juce::Image cpuAtlas;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ImageAtlas)
};

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Terminal
