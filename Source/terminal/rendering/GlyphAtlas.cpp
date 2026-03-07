/**
 * @file GlyphAtlas.cpp
 * @brief FreeType rasterization backend for GlyphAtlas (Linux / Windows).
 *
 * This translation unit is compiled on all platforms **except** macOS
 * (`JUCE_MAC`).  On macOS, `GlyphAtlas.mm` provides the CoreText
 * implementation of `GlyphAtlas::rasterizeGlyph()` instead.
 *
 * ### Responsibilities
 * - `GlyphAtlas` constructor / `clear()` / `advanceFrame()` — platform-neutral.
 * - `getOrRasterize()` — cache lookup + rasterization dispatch.
 * - `getOrRasterizeBoxDrawing()` — procedural box/braille rasterization.
 * - `stageForUpload()` — thread-safe bitmap staging for GL upload.
 * - `rasterizeGlyph()` (FreeType path) — outline transform + rendering.
 *
 * ### FreeType rasterization modes
 * | Condition                        | Mode                                   |
 * |----------------------------------|----------------------------------------|
 * | Normal monochrome glyph          | `FT_RENDER_MODE_LIGHT` (greyscale AA)  |
 * | Normal glyph + embolden          | `FT_Outline_Embolden` then light mode  |
 * | Constrained NF icon (outline)    | Outline transform → light mode         |
 * | Color emoji (`FT_PIXEL_MODE_BGRA`)| `FT_LOAD_RENDER | FT_LOAD_COLOR`      |
 *
 * @see GlyphAtlas.h
 * @see GlyphAtlas.mm
 * @see BoxDrawing
 */

#include "GlyphAtlas.h"
#include "BoxDrawing.h"
#include FT_OUTLINE_H

/**
 * @brief Construct the atlas with both packers and LRU caches initialized.
 *
 * Sets atlas dimensions to `atlasSize × atlasSize` (4096×4096), constructs
 * both `AtlasPacker` instances at that size, and initializes the mono and
 * emoji LRU caches at their respective capacity limits.
 */
GlyphAtlas::GlyphAtlas()
    : atlasWidth (atlasSize)
    , atlasHeight (atlasSize)
    , monoPacker (atlasSize, atlasSize)
    , emojiPacker (atlasSize, atlasSize)
    , monoLRU (monoLruCapacity)
    , emojiLRU (emojiLruCapacity)
{
}

/**
 * @brief Return a cached glyph or rasterize it on demand.
 *
 * Selects the appropriate LRU cache based on `isEmoji`, then performs a
 * cache lookup.  On a miss, delegates to `rasterizeGlyph()` and inserts the
 * result into the cache.  Glyphs with zero dimensions (rasterization failure
 * or atlas full) are not inserted.
 *
 * @param key        Unique glyph identity.
 * @param fontHandle Opaque FT_Face handle.
 * @param isEmoji    `true` to use the RGBA8 emoji atlas.
 * @param constraint Nerd Font layout descriptor.
 * @param cellWidth  Cell width in physical pixels.
 * @param cellHeight Cell height in physical pixels.
 * @param baseline   Pixels from cell top to baseline.
 * @return Pointer to the `AtlasGlyph`, or `nullptr` on failure.
 *
 * @note MESSAGE THREAD only.
 */
AtlasGlyph* GlyphAtlas::getOrRasterize (const GlyphKey& key, void* fontHandle, bool isEmoji,
                                          const GlyphConstraint& constraint,
                                          int cellWidth, int cellHeight, int baseline) noexcept
{
    auto& lru { isEmoji ? emojiLRU : monoLRU };

    AtlasGlyph* cached { lru.get (key) };
    AtlasGlyph* result { cached };

    if (cached == nullptr)
    {
        auto glyph { rasterizeGlyph (key, fontHandle, isEmoji, constraint, cellWidth, cellHeight, baseline) };

        if (glyph.widthPixels > 0 and glyph.heightPixels > 0)
        {
            result = lru.insert (key, glyph);
        }
    }

    return result;
}

/**
 * @brief Copy pixel data into the upload queue under `uploadMutex`.
 *
 * Grows the `uploadQueue` HeapBlock by doubling capacity when full, then
 * appends a new `StagedBitmap` with a deep copy of the source pixels.
 * The OpenGL pixel format is set to `GL_BGRA` for emoji and `GL_RED` for
 * mono bitmaps.
 *
 * @param pixelData Source pixel buffer (not transferred; copied internally).
 * @param width     Bitmap width in pixels.
 * @param height    Bitmap height in pixels.
 * @param region    Destination rectangle within the atlas texture.
 * @param isEmoji   `true` for RGBA8 (4 bpp); `false` for R8 (1 bpp).
 *
 * @note MESSAGE THREAD only (acquires `uploadMutex`).
 */
void GlyphAtlas::stageForUpload (uint8_t* pixelData, int width, int height,
                                   const juce::Rectangle<int>& region, bool isEmoji) noexcept
{
    const std::lock_guard<std::mutex> lock (uploadMutex);

    if (uploadCount >= uploadCapacity)
    {
        const int newCapacity { std::max (16, uploadCapacity * 2) };
        juce::HeapBlock<StagedBitmap> grown;
        grown.allocate (static_cast<size_t> (newCapacity), true);

        for (int i { 0 }; i < uploadCount; ++i)
        {
            grown[i] = std::move (uploadQueue[i]);
        }

        uploadQueue = std::move (grown);
        uploadCapacity = newCapacity;
    }

    StagedBitmap staged;
    staged.kind = isEmoji ? StagedBitmap::AtlasKind::emoji : StagedBitmap::AtlasKind::mono;
    staged.region = region;

    const size_t bpp { isEmoji ? size_t { 4 } : size_t { 1 } };
    staged.pixelDataSize = static_cast<size_t> (width) * static_cast<size_t> (height) * bpp;
    staged.pixelData.allocate (staged.pixelDataSize, false);
    std::memcpy (staged.pixelData.get(), pixelData, staged.pixelDataSize);
    staged.format = isEmoji ? juce::gl::GL_BGRA : juce::gl::GL_RED;

    uploadQueue[uploadCount] = std::move (staged);
    ++uploadCount;
}

/**
 * @brief Invalidate all cached glyphs and reset both atlas packers.
 *
 * Clears both LRU caches (freeing all `AtlasGlyph` entries) and resets both
 * `AtlasPacker` instances to empty.  The upload queue is also drained by
 * zeroing `uploadCount`.  Call this when the font size changes or the GL
 * context is recreated.
 *
 * @note MESSAGE THREAD only.
 */
void GlyphAtlas::clear() noexcept
{
    monoLRU.clear();
    emojiLRU.clear();
    monoPacker.reset();
    emojiPacker.reset();
    uploadCount = 0;
}

/**
 * @brief Advance the LRU frame counter for both caches.
 *
 * Delegates to `LRUGlyphCache::advanceFrame()` on both the mono and emoji
 * caches.  Must be called once per rendered frame.
 *
 * @note MESSAGE THREAD only.
 */
void GlyphAtlas::advanceFrame() noexcept
{
    monoLRU.advanceFrame();
    emojiLRU.advanceFrame();
}

/**
 * @brief Return a cached box-drawing glyph or rasterize it procedurally.
 *
 * Constructs a synthetic `GlyphKey` with `fontFace = nullptr` and
 * `fontSize = 0` to uniquely identify box-drawing glyphs independently of
 * any font.  On a cache miss, calls `BoxDrawing::rasterize()` to fill a
 * `cellWidth × cellHeight` R8 buffer, allocates a mono atlas region, and
 * stages the bitmap for upload.
 *
 * The resulting `AtlasGlyph` always has:
 * - `widthPixels  = cellWidth`
 * - `heightPixels = cellHeight`
 * - `bearingX     = 0`
 * - `bearingY     = baseline`
 *
 * @param codepoint  Unicode codepoint (U+2500–U+257F, U+2580–U+259F, or
 *                   U+2800–U+28FF).
 * @param cellWidth  Cell width in physical pixels.
 * @param cellHeight Cell height in physical pixels.
 * @param baseline   Pixels from cell top to baseline.
 * @return Pointer to the `AtlasGlyph`, or `nullptr` if the mono atlas is full.
 *
 * @note MESSAGE THREAD only.
 * @see BoxDrawing::rasterize()
 */
AtlasGlyph* GlyphAtlas::getOrRasterizeBoxDrawing (uint32_t codepoint, int cellWidth, int cellHeight, int baseline) noexcept
{
    GlyphKey key;
    key.glyphIndex = codepoint;
    key.fontFace   = nullptr;
    key.fontSize   = 0.0f;
    key.cellSpan   = 1;

    AtlasGlyph* cached { monoLRU.get (key) };
    AtlasGlyph* result { cached };

    if (cached == nullptr)
    {
        const int bitmapW { cellWidth };
        const int bitmapH { cellHeight };
        const size_t bufSize { static_cast<size_t> (bitmapW) * static_cast<size_t> (bitmapH) };
        juce::HeapBlock<uint8_t> buf (bufSize, true);

        BoxDrawing::rasterize (codepoint, bitmapW, bitmapH, buf.get());

        auto region { monoPacker.allocate (bitmapW, bitmapH) };

        if (not region.isEmpty())
        {
            AtlasGlyph glyph;
            glyph.widthPixels  = bitmapW;
            glyph.heightPixels = bitmapH;
            glyph.bearingX     = 0;
            glyph.bearingY     = baseline;

            const float aw { static_cast<float> (atlasWidth) };
            const float ah { static_cast<float> (atlasHeight) };
            glyph.textureCoordinates = juce::Rectangle<float> {
                static_cast<float> (region.getX()) / aw,
                static_cast<float> (region.getY()) / ah,
                static_cast<float> (bitmapW) / aw,
                static_cast<float> (bitmapH) / ah };

            stageForUpload (buf.get(), bitmapW, bitmapH, region, false);

            result = monoLRU.insert (key, glyph);
        }
    }

    return result;
}

#if JUCE_MAC
// CoreText implementation in GlyphAtlas.mm
#else

/**
 * @brief Copy rows from an FT_Bitmap into a flat destination buffer.
 *
 * Handles both positive-pitch (top-down) and negative-pitch (bottom-up)
 * FreeType bitmaps.  For negative pitch, the source pointer is adjusted to
 * point at the first row before copying.
 *
 * @param bitmap       Source FreeType bitmap.
 * @param dest         Destination buffer (must be `width × rows × bpp` bytes).
 * @param bytesPerPixel Bytes per pixel: 1 for `FT_PIXEL_MODE_GRAY`, 4 for
 *                     `FT_PIXEL_MODE_BGRA`.
 *
 * @note Called from `rasterizeGlyph()` on the MESSAGE THREAD.
 */
static void copyBitmapRows (const FT_Bitmap& bitmap,
                             uint8_t* dest,
                             size_t bytesPerPixel) noexcept
{
    const size_t srcStride { static_cast<size_t> (std::abs (bitmap.pitch)) };
    const size_t dstStride { static_cast<size_t> (bitmap.width) * bytesPerPixel };
    const unsigned char* srcRow { bitmap.buffer };

    if (bitmap.pitch < 0)
    {
        srcRow = bitmap.buffer + static_cast<ptrdiff_t> ((1 - static_cast<ptrdiff_t> (bitmap.rows)) * bitmap.pitch);
    }

    for (unsigned int y { 0 }; y < bitmap.rows; ++y)
    {
        std::memcpy (dest + static_cast<ptrdiff_t> (y) * static_cast<ptrdiff_t> (dstStride),
                     srcRow + static_cast<ptrdiff_t> (y) * static_cast<ptrdiff_t> (srcStride),
                     dstStride);
    }
}

/**
 * @brief Rasterize a single glyph using FreeType (Linux / Windows backend).
 *
 * Dispatches to one of three rasterization paths based on the glyph type:
 *
 * @par Path 1 — Constrained NF icon (outline glyph + active constraint)
 * The FreeType outline points are transformed in-place to implement the
 * constraint's scale mode (fit / cover / adaptiveScale / stretch), alignment
 * (H/V), and padding.  The glyph is then rendered into a `cellWidth ×
 * cellHeight` buffer and stored at full cell dimensions in the mono atlas.
 * `bearingX = 0`, `bearingY = baseline`.
 *
 * @par Path 2 — Normal monochrome glyph
 * Optionally emboldens the outline with `FT_Outline_Embolden` (1/64 px),
 * then renders with `FT_RENDER_MODE_LIGHT`.  Stored at natural bitmap
 * dimensions.  `bearingX` and `bearingY` come from `bitmap_left` /
 * `bitmap_top`.
 *
 * @par Path 3 — Color emoji (`FT_PIXEL_MODE_BGRA`)
 * Loaded with `FT_LOAD_RENDER | FT_LOAD_COLOR`.  Stored in the RGBA8 emoji
 * atlas at natural bitmap dimensions.
 *
 * @param key        Glyph identity (index, face, size, span).
 * @param fontHandle Opaque `FT_Face` handle.
 * @param isEmoji    `true` for color emoji rendering.
 * @param constraint Nerd Font layout descriptor.
 * @param cellWidth  Cell width in physical pixels.
 * @param cellHeight Cell height in physical pixels.
 * @param baseline   Pixels from cell top to baseline.
 * @return Populated `AtlasGlyph` on success; zero-dimension glyph on failure.
 *
 * @note MESSAGE THREAD only.
 * @see GlyphConstraint
 * @see copyBitmapRows()
 */
AtlasGlyph GlyphAtlas::rasterizeGlyph (const GlyphKey& key, void* fontHandle, bool isEmoji,
                                         const GlyphConstraint& constraint,
                                         int cellWidth, int cellHeight, int baseline) noexcept
{
    FT_Face face { static_cast<FT_Face> (fontHandle) };
    jassert (face != nullptr);

    AtlasGlyph glyph;

    if (face != nullptr)
    {
        int loadFlags { FT_LOAD_TARGET_LIGHT };

        if (isEmoji)
        {
            loadFlags |= FT_LOAD_RENDER | FT_LOAD_COLOR;
        }

        if (FT_Load_Glyph (face, key.glyphIndex, loadFlags) == 0)
        {
            if (not isEmoji and face->glyph->format == FT_GLYPH_FORMAT_OUTLINE)
            {
                if (constraint.isActive())
                {
                    const int spanCells { std::max (1, static_cast<int> (key.cellSpan)) };
                    const float effCellW { static_cast<float> (cellWidth * spanCells) };
                    const float effCellH { static_cast<float> (cellHeight) };

                    const float inset { GlyphConstraint::iconInset };
                    const float targetW { effCellW * (1.0f - constraint.padLeft - constraint.padRight - inset * 2.0f) };
                    const float targetH { effCellH * (1.0f - constraint.padTop - constraint.padBottom - inset * 2.0f) };

                    const float glyphNaturalW { static_cast<float> (face->glyph->metrics.width) / 64.0f };
                    const float glyphNaturalH { static_cast<float> (face->glyph->metrics.height) / 64.0f };

                    float scaleX { 1.0f };
                    float scaleY { 1.0f };

                    if (glyphNaturalW > 0.0f and glyphNaturalH > 0.0f)
                    {
                        scaleX = targetW / glyphNaturalW;
                        scaleY = targetH / glyphNaturalH;

                        if (constraint.scaleMode == GlyphConstraint::ScaleMode::fit)
                        {
                            const float uniform { std::min (scaleX, scaleY) };
                            scaleX = std::min (1.0f, uniform);
                            scaleY = scaleX;
                        }
                        else if (constraint.scaleMode == GlyphConstraint::ScaleMode::cover)
                        {
                            const float uniform { std::min (scaleX, scaleY) };
                            scaleX = uniform;
                            scaleY = uniform;
                        }
                        else if (constraint.scaleMode == GlyphConstraint::ScaleMode::adaptiveScale)
                        {
                            // Same formula as fit: never upscale. Semantically distinct
                            // from fit (NF patcher 'pa' without '!' flag).
                            const float uniform { std::min (scaleX, scaleY) };
                            scaleX = std::min (1.0f, uniform);
                            scaleY = scaleX;
                        }
                    }

                    if (constraint.maxAspectRatio > 0.0f)
                    {
                        const float scaledW { glyphNaturalW * scaleX };
                        const float scaledH { glyphNaturalH * scaleY };

                        if (scaledH > 0.0f and scaledW / scaledH > constraint.maxAspectRatio)
                        {
                            scaleX = scaledH * constraint.maxAspectRatio / glyphNaturalW;
                        }
                    }

                    const float scaledW { glyphNaturalW * scaleX };
                    const float scaledH { glyphNaturalH * scaleY };

                    float posX { constraint.padLeft * effCellW };
                    float posY { constraint.padBottom * effCellH };

                    if (constraint.alignH == GlyphConstraint::Align::center)
                    {
                        posX = (effCellW - scaledW) * 0.5f;
                    }
                    else if (constraint.alignH == GlyphConstraint::Align::end)
                    {
                        posX = effCellW - scaledW - constraint.padRight * effCellW;
                    }

                    if (constraint.alignV == GlyphConstraint::Align::center)
                    {
                        posY = (effCellH - scaledH) * 0.5f;
                    }
                    else if (constraint.alignV == GlyphConstraint::Align::end)
                    {
                        posY = effCellH - scaledH - constraint.padTop * effCellH;
                    }

                    FT_Outline& outline { face->glyph->outline };
                    const float naturalX { static_cast<float> (face->glyph->metrics.horiBearingX) / 64.0f };
                    const float naturalY { static_cast<float> (face->glyph->metrics.horiBearingY) / 64.0f };

                    for (int p { 0 }; p < outline.n_points; ++p)
                    {
                        float px { static_cast<float> (outline.points[p].x) / 64.0f };
                        float py { static_cast<float> (outline.points[p].y) / 64.0f };

                        px -= naturalX;
                        py -= (naturalY - glyphNaturalH);

                        px *= scaleX;
                        py *= scaleY;

                        px += posX;
                        py += posY;

                        outline.points[p].x = static_cast<FT_Pos> (px * 64.0f);
                        outline.points[p].y = static_cast<FT_Pos> (py * 64.0f);
                    }

                    FT_Render_Glyph (face->glyph, FT_RENDER_MODE_LIGHT);

                    auto& bitmap { face->glyph->bitmap };

                    if (bitmap.width > 0 and bitmap.rows > 0)
                    {
                        const int cellBitmapW { cellWidth * spanCells };
                        const int cellBitmapH { cellHeight };
                        auto region { monoPacker.allocate (cellBitmapW, cellBitmapH) };

                        if (not region.isEmpty())
                        {
                            glyph.widthPixels = cellBitmapW;
                            glyph.heightPixels = cellBitmapH;
                            glyph.bearingX = 0;
                            glyph.bearingY = baseline;

                            const float aw { static_cast<float> (atlasWidth) };
                            const float ah { static_cast<float> (atlasHeight) };
                            glyph.textureCoordinates = juce::Rectangle<float> {
                                static_cast<float> (region.getX()) / aw,
                                static_cast<float> (region.getY()) / ah,
                                static_cast<float> (cellBitmapW) / aw,
                                static_cast<float> (cellBitmapH) / ah };

                            const size_t cellBufSize { static_cast<size_t> (cellBitmapW) * static_cast<size_t> (cellBitmapH) };
                            juce::HeapBlock<uint8_t> cellBuf (cellBufSize, true);

                            const int bitmapOffsetX { face->glyph->bitmap_left };
                            const int bitmapOffsetY { cellBitmapH - face->glyph->bitmap_top };

                            const int srcW { static_cast<int> (bitmap.width) };
                            const int srcH { static_cast<int> (bitmap.rows) };
                            const size_t srcStride { static_cast<size_t> (std::abs (bitmap.pitch)) };

                            for (int row { 0 }; row < srcH; ++row)
                            {
                                const int dstRow { bitmapOffsetY + row };

                                if (dstRow >= 0 and dstRow < cellBitmapH)
                                {
                                    for (int col { 0 }; col < srcW; ++col)
                                    {
                                        const int dstCol { bitmapOffsetX + col };

                                        if (dstCol >= 0 and dstCol < cellBitmapW)
                                        {
                                            const size_t srcIdx { static_cast<size_t> (row) * srcStride + static_cast<size_t> (col) };
                                            const size_t dstIdx { static_cast<size_t> (dstRow) * static_cast<size_t> (cellBitmapW) + static_cast<size_t> (dstCol) };
                                            cellBuf[dstIdx] = bitmap.buffer[srcIdx];
                                        }
                                    }
                                }
                            }

                            stageForUpload (cellBuf.get(), cellBitmapW, cellBitmapH, region, false);
                        }
                    }
                }
                else
                {
                    if (embolden)
                    {
                        FT_Outline_Embolden (&face->glyph->outline, 1 << 6);
                    }

                    FT_Render_Glyph (face->glyph, FT_RENDER_MODE_LIGHT);

                    auto& bitmap { face->glyph->bitmap };

                    if (bitmap.width > 0 and bitmap.rows > 0)
                    {
                        auto region { monoPacker.allocate (static_cast<int> (bitmap.width), static_cast<int> (bitmap.rows)) };

                        if (not region.isEmpty())
                        {
                            glyph.widthPixels = static_cast<int> (bitmap.width);
                            glyph.heightPixels = static_cast<int> (bitmap.rows);
                            glyph.bearingX = face->glyph->bitmap_left;
                            glyph.bearingY = face->glyph->bitmap_top;

                            const float aw { static_cast<float> (atlasWidth) };
                            const float ah { static_cast<float> (atlasHeight) };
                            glyph.textureCoordinates = juce::Rectangle<float> {
                                static_cast<float> (region.getX()) / aw,
                                static_cast<float> (region.getY()) / ah,
                                static_cast<float> (bitmap.width) / aw,
                                static_cast<float> (bitmap.rows) / ah };

                            const size_t bufSize { static_cast<size_t> (bitmap.width) * bitmap.rows };
                            juce::HeapBlock<uint8_t> buf (bufSize, true);

                            if (bitmap.pixel_mode == FT_PIXEL_MODE_GRAY)
                            {
                                copyBitmapRows (bitmap, buf.get(), 1);
                            }

                            stageForUpload (buf.get(), glyph.widthPixels, glyph.heightPixels, region, false);
                        }
                    }
                }
            }
            else
            {
                auto& bitmap { face->glyph->bitmap };

                if (bitmap.width > 0 and bitmap.rows > 0)
                {
                    auto region { emojiPacker.allocate (static_cast<int> (bitmap.width), static_cast<int> (bitmap.rows)) };

                    if (not region.isEmpty())
                    {
                        glyph.widthPixels = static_cast<int> (bitmap.width);
                        glyph.heightPixels = static_cast<int> (bitmap.rows);
                        glyph.bearingX = face->glyph->bitmap_left;
                        glyph.bearingY = face->glyph->bitmap_top;

                        const float aw { static_cast<float> (atlasWidth) };
                        const float ah { static_cast<float> (atlasHeight) };
                        glyph.textureCoordinates = juce::Rectangle<float> {
                            static_cast<float> (region.getX()) / aw,
                            static_cast<float> (region.getY()) / ah,
                            static_cast<float> (bitmap.width) / aw,
                            static_cast<float> (bitmap.rows) / ah };

                        const size_t bufSize { static_cast<size_t> (bitmap.width) * bitmap.rows * 4 };
                        juce::HeapBlock<uint8_t> buf (bufSize, true);

                        if (bitmap.pixel_mode == FT_PIXEL_MODE_BGRA)
                        {
                            copyBitmapRows (bitmap, buf.get(), 4);
                        }

                        stageForUpload (buf.get(), glyph.widthPixels, glyph.heightPixels, region, true);
                    }
                }
            }
        }
    }

    return glyph;
}

#endif
