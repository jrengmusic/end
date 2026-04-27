/**
 * @file ImageAtlas.cpp
 * @brief Implementation of Terminal::ImageAtlas.
 *
 * All MESSAGE THREAD methods: `stage()`, `publishStagedUploads()`, `lookup()`,
 * `release()`, `evictIfNeeded()`, `advanceFrame()`, `ensureCPUAtlas()`,
 * `getCPUAtlas()`, `getMutableCPUAtlas()`.
 *
 * All GL THREAD methods: `consumeStagedUploads()`, `createGLTexture()`,
 * `destroyGLTexture()`, `getGLTexture()`.
 *
 * @see ImageAtlas.h
 */

#include "ImageAtlas.h"

namespace Terminal
{ /*____________________________________________________________________________*/

// MESSAGE THREAD

/**
 * @brief Constructs an ImageAtlas with the given budget.
 *
 * Initialises the shelf packer for a 4096×4096 RGBA8 atlas and stores the byte
 * budget for eviction decisions.
 *
 * @param budgetBytes  Maximum total RGBA pixel bytes before eviction.
 *
 * @note MESSAGE THREAD.
 */
ImageAtlas::ImageAtlas (int budgetBytes_) noexcept
    : packer (atlasDimension, atlasDimension)
    , budgetBytes (budgetBytes_)
{
}

/**
 * @brief Stages an RGBA8 image for upload and returns its stable image ID.
 *
 * Allocates a region in the shelf packer, deep-copies the pixel data into a
 * `StagedBitmap`, appends it to the active write batch, computes normalised UV
 * coordinates, and stores the `LRUEntry` in the regions map.
 *
 * If the packer reports atlas-full (empty rect), `evictIfNeeded()` is called
 * and the allocation is retried once.  Returns 0 on persistent failure.
 *
 * @param rgba      Pointer to tightly-packed RGBA8 pixel data (4 bytes/pixel).
 * @param widthPx   Source image width in pixels.
 * @param heightPx  Source image height in pixels.
 * @return Stable image ID, or 0 on failure.
 *
 * @note MESSAGE THREAD only.
 */
uint32_t ImageAtlas::stage (const uint8_t* rgba, int widthPx, int heightPx) noexcept
{
    jassert (rgba != nullptr);
    jassert (widthPx > 0 and heightPx > 0);

    juce::Rectangle<int> region { packer.allocate (widthPx, heightPx) };

    if (region.isEmpty())
    {
        evictIfNeeded();
        region = packer.allocate (widthPx, heightPx);
    }

    uint32_t resultId { 0 };

    if (not region.isEmpty())
    {
        const size_t byteCount { static_cast<size_t> (widthPx) * static_cast<size_t> (heightPx) * 4u };

        jam::Glyph::StagedBitmap bitmap;
        bitmap.type = jam::Glyph::Type::emoji;
        bitmap.region = region;
        bitmap.pixelData.allocate (byteCount, false);
        std::memcpy (bitmap.pixelData.get(), rgba, byteCount);
        bitmap.pixelDataSize = byteCount;

        uploadBatches[static_cast<size_t> (writeSlot)].append (std::move (bitmap));

        const float atlasF { static_cast<float> (atlasDimension) };

        ImageRegion imageRegion;
        imageRegion.uv = juce::Rectangle<float>
        {
            static_cast<float> (region.getX())      / atlasF,
            static_cast<float> (region.getY())      / atlasF,
            static_cast<float> (widthPx)             / atlasF,
            static_cast<float> (heightPx)            / atlasF
        };
        imageRegion.widthPx  = widthPx;
        imageRegion.heightPx = heightPx;

        const int sizeBytes { widthPx * heightPx * 4 };

        LRUEntry entry;
        entry.region          = imageRegion;
        entry.lastAccessFrame = currentFrame;
        entry.sizeBytes       = sizeBytes;

        const uint32_t assignedId { nextImageId };
        regions[assignedId] = entry;
        ++nextImageId;
        totalBytes += sizeBytes;

        resultId = assignedId;
    }

    return resultId;
}

/**
 * @brief Stages an RGBA8 image using a caller-provided image ID.
 *
 * Identical to `stage()` except the entry is stored under @p imageId rather
 * than the internal auto-incrementing counter.  Called by the MESSAGE THREAD
 * when draining the pending image queue produced by READER THREAD decoders.
 *
 * @param imageId   Pre-reserved ID (from `Grid::reserveImageId()`).
 * @param rgba      Pointer to tightly-packed RGBA8 pixel data (4 bytes/pixel).
 * @param widthPx   Source image width in pixels.
 * @param heightPx  Source image height in pixels.
 * @return `true` on success, `false` if the atlas is full after eviction.
 *
 * @note MESSAGE THREAD only.
 */
bool ImageAtlas::stageWithId (uint32_t imageId, const uint8_t* rgba,
                              int widthPx, int heightPx) noexcept
{
    jassert (rgba != nullptr);
    jassert (widthPx > 0 and heightPx > 0);
    jassert (imageId != 0);

    juce::Rectangle<int> region { packer.allocate (widthPx, heightPx) };

    if (region.isEmpty())
    {
        evictIfNeeded();
        region = packer.allocate (widthPx, heightPx);
    }

    bool success { false };

    if (not region.isEmpty())
    {
        const size_t byteCount { static_cast<size_t> (widthPx) * static_cast<size_t> (heightPx) * 4u };

        jam::Glyph::StagedBitmap bitmap;
        bitmap.type = jam::Glyph::Type::emoji;
        bitmap.region = region;
        bitmap.pixelData.allocate (byteCount, false);
        std::memcpy (bitmap.pixelData.get(), rgba, byteCount);
        bitmap.pixelDataSize = byteCount;

        uploadBatches[static_cast<size_t> (writeSlot)].append (std::move (bitmap));

        const float atlasF { static_cast<float> (atlasDimension) };

        ImageRegion imageRegion;
        imageRegion.uv = juce::Rectangle<float>
        {
            static_cast<float> (region.getX())  / atlasF,
            static_cast<float> (region.getY())  / atlasF,
            static_cast<float> (widthPx)         / atlasF,
            static_cast<float> (heightPx)        / atlasF
        };
        imageRegion.widthPx  = widthPx;
        imageRegion.heightPx = heightPx;

        const int sizeBytes { widthPx * heightPx * 4 };

        LRUEntry entry;
        entry.region          = imageRegion;
        entry.lastAccessFrame = currentFrame;
        entry.sizeBytes       = sizeBytes;

        regions[imageId] = entry;
        totalBytes += sizeBytes;

        success = true;
    }

    return success;
}

/**
 * @brief Publishes the current write batch to the GL THREAD via atomic exchange.
 *
 * If the active write slot has at least one staged bitmap, performs a lock-free
 * `Mailbox::write()` (acq_rel) that hands the batch pointer to the GL THREAD
 * and retrieves any previously returned batch.  The write slot then flips to
 * the other buffer slot.  If the returned batch still contains entries (GL
 * thread has not yet processed the previous frame), those entries are absorbed
 * into the new write slot to prevent data loss.
 *
 * The self-reference guard (`returned != &uploadBatches[writeSlot]`) is critical:
 * without it, self-absorb iterates while reallocating the HeapBlock, causing UB
 * when the GL thread falls behind.
 *
 * @note MESSAGE THREAD only.
 */
void ImageAtlas::publishStagedUploads() noexcept
{
    auto& batch { uploadBatches[static_cast<size_t> (writeSlot)] };

    if (batch.count > 0)
    {
        auto* returned { uploadMailbox.write (&batch) };
        writeSlot = (writeSlot == 0) ? 1 : 0;

        if (returned != nullptr
            and returned != &uploadBatches[static_cast<size_t> (writeSlot)]
            and returned->count > 0)
        {
            uploadBatches[static_cast<size_t> (writeSlot)].absorb (*returned);
        }
    }
}

/**
 * @brief Looks up the region for a previously staged image.
 *
 * Updates `lastAccessFrame` for LRU accounting.
 *
 * @param imageId  ID returned by a prior `stage()` call.
 * @return Const pointer to the `ImageRegion`, or `nullptr` if not found.
 *
 * @note MESSAGE THREAD only.
 */
const ImageRegion* ImageAtlas::lookup (uint32_t imageId) noexcept
{
    const ImageRegion* result { nullptr };

    auto it { regions.find (imageId) };

    if (it != regions.end())
    {
        it->second.lastAccessFrame = currentFrame;
        result = &it->second.region;
    }

    return result;
}

/**
 * @brief Removes an image from the regions map and subtracts its byte count.
 *
 * Does NOT free atlas texture space — the shelf packer has no individual-
 * region free operation.  Freed space is reclaimed only when `evictIfNeeded()`
 * resets the packer.
 *
 * @param imageId  ID returned by a prior `stage()` call.
 *
 * @note MESSAGE THREAD only.
 */
void ImageAtlas::release (uint32_t imageId) noexcept
{
    auto it { regions.find (imageId) };

    if (it != regions.end())
    {
        totalBytes -= it->second.sizeBytes;
        regions.erase (it);
    }
}

/**
 * @brief Clears all regions and resets the packer if the byte budget is exceeded.
 *
 * Eviction strategy: on budget overflow, clear the entire map and reset the
 * packer so all shelf space is reclaimed.  Images must be re-staged on demand.
 *
 * @note MESSAGE THREAD only.
 */
void ImageAtlas::evictIfNeeded() noexcept
{
    if (totalBytes > budgetBytes)
    {
        regions.clear();
        packer.reset();
        totalBytes  = 0;
        nextImageId = 1;
    }
}

/**
 * @brief Advances the LRU frame counter.
 *
 * Must be called once per rendered frame so that LRU age calculations in
 * `lookup()` remain accurate.
 *
 * @note MESSAGE THREAD only.
 */
void ImageAtlas::advanceFrame() noexcept
{
    ++currentFrame;
}

/**
 * @brief Updates the byte budget used by `evictIfNeeded()`.
 *
 * @param bytes  New budget in bytes.
 *
 * @note MESSAGE THREAD only.
 */
void ImageAtlas::setBudgetBytes (int bytes) noexcept
{
    jassert (bytes > 0);
    budgetBytes = bytes;
}

/**
 * @brief Ensures the CPU atlas image is allocated.
 *
 * Creates a 4096×4096 ARGB `SoftwareImageType` image if not already valid.
 *
 * @note MESSAGE THREAD only.
 */
void ImageAtlas::ensureCPUAtlas() noexcept
{
    if (not cpuAtlas.isValid())
    {
        cpuAtlas = juce::Image (juce::Image::ARGB,
                                 atlasDimension, atlasDimension,
                                 true,
                                 juce::SoftwareImageType());
    }
}

/**
 * @brief Returns a const reference to the CPU atlas image.
 *
 * @return Const reference to the CPU atlas `juce::Image`.
 *
 * @note MESSAGE THREAD only.
 */
const juce::Image& ImageAtlas::getCPUAtlas() const noexcept
{
    return cpuAtlas;
}

/**
 * @brief Returns a mutable reference to the CPU atlas image.
 *
 * @return Mutable reference to the CPU atlas `juce::Image`.
 *
 * @note MESSAGE THREAD only.
 */
juce::Image& ImageAtlas::getMutableCPUAtlas() noexcept
{
    return cpuAtlas;
}

// GL THREAD

/**
 * @brief Receives the current `StagedBatch` from the Mailbox for GL upload.
 *
 * @return Pointer to the `StagedBatch`, or `nullptr` if none was pending.
 *
 * @note GL THREAD only.
 */
jam::Glyph::StagedBatch* ImageAtlas::consumeStagedUploads() noexcept
{
    return uploadMailbox.read();
}

/**
 * @brief Allocates the GL texture for the image atlas.
 *
 * Creates a 4096×4096 `GL_RGBA` texture with `GL_LINEAR` filtering and
 * `GL_CLAMP_TO_EDGE` wrapping, zero-initialised via `glTexImage2D`.
 *
 * @note GL THREAD only.
 */
void ImageAtlas::createGLTexture() noexcept
{
    juce::gl::glGenTextures (1, &glTexture);
    juce::gl::glBindTexture (juce::gl::GL_TEXTURE_2D, glTexture);

    juce::gl::glTexParameteri (juce::gl::GL_TEXTURE_2D, juce::gl::GL_TEXTURE_MIN_FILTER, juce::gl::GL_LINEAR);
    juce::gl::glTexParameteri (juce::gl::GL_TEXTURE_2D, juce::gl::GL_TEXTURE_MAG_FILTER, juce::gl::GL_LINEAR);
    juce::gl::glTexParameteri (juce::gl::GL_TEXTURE_2D, juce::gl::GL_TEXTURE_WRAP_S,     juce::gl::GL_CLAMP_TO_EDGE);
    juce::gl::glTexParameteri (juce::gl::GL_TEXTURE_2D, juce::gl::GL_TEXTURE_WRAP_T,     juce::gl::GL_CLAMP_TO_EDGE);

    juce::gl::glTexImage2D (juce::gl::GL_TEXTURE_2D, 0,
                             juce::gl::GL_RGBA,
                             atlasDimension, atlasDimension, 0,
                             juce::gl::GL_RGBA, juce::gl::GL_UNSIGNED_BYTE,
                             nullptr);

    juce::gl::glBindTexture (juce::gl::GL_TEXTURE_2D, 0);
}

/**
 * @brief Releases the GL texture.
 *
 * @note GL THREAD only.
 */
void ImageAtlas::destroyGLTexture() noexcept
{
    if (glTexture != 0)
    {
        juce::gl::glDeleteTextures (1, &glTexture);
        glTexture = 0;
    }
}

/**
 * @brief Returns the GL texture name.
 *
 * @return The GL texture handle, or 0 if not yet created.
 *
 * @note GL THREAD only.
 */
GLuint ImageAtlas::getGLTexture() const noexcept
{
    return glTexture;
}

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Terminal
