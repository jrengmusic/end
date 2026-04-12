/**
 * @file jreng_glyph_atlas.cpp
 * @brief FreeType rasterization backend for Atlas (Linux / Windows).
 *
 * This translation unit is compiled on all platforms **except** macOS
 * (`JUCE_MAC`).  On macOS, `jreng_glyph_atlas.mm` provides the CoreText
 * implementation of `Atlas::rasterizeGlyph()` instead.
 *
 * ### Responsibilities
 * - `Atlas` constructor / `clear()` / `advanceFrame()` — platform-neutral.
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
 * @see jreng_glyph_atlas.h
 * @see jreng_glyph_atlas.mm
 * @see BoxDrawing
 */

// Included via unity build (jreng_glyph.cpp) — jreng_glyph.h already in scope
#include FT_OUTLINE_H

static constexpr float ftFixed { static_cast<float> (jreng::Typeface::ftFixedScale) };

namespace jreng::Glyph
{

#if ! JUCE_MAC

/**
 * @brief Construct the atlas with both packers and LRU caches initialized.
 *
 * Sets atlas dimensions from `size`, constructs both `AtlasPacker` instances
 * at that dimension, and initializes the mono and emoji LRU caches at their
 * respective capacity limits.
 *
 * @param size  Preset atlas dimension.
 */
Atlas::Atlas (AtlasSize size) noexcept
    : atlasWidth (static_cast<int> (size))
    , atlasHeight (static_cast<int> (size))
    , monoPacker (static_cast<int> (size), static_cast<int> (size))
    , emojiPacker (static_cast<int> (size), static_cast<int> (size))
    , monoLRU (monoLruCapacity)
    , emojiLRU (emojiLruCapacity)
{
}

Atlas::~Atlas()
{
}

#endif

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
 * @return Pointer to the `Region`, or `nullptr` on failure.
 *
 * @note MESSAGE THREAD only.
 */
Region* Atlas::getOrRasterize (const Key& key, void* fontHandle, bool isEmoji,
                               const Constraint& constraint,
                               int cellWidth, int cellHeight, int baseline) noexcept
{
    auto& lru { isEmoji ? emojiLRU : monoLRU };

    Region* cached { lru.get (key) };
    Region* result { cached };

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
void Atlas::stageForUpload (uint8_t* pixelData, int width, int height,
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
            jassert (i >= 0 and i < uploadCount);
            grown[i] = std::move (uploadQueue[i]);
        }

        uploadQueue = std::move (grown);
        uploadCapacity = newCapacity;
    }

    StagedBitmap staged;
    staged.type = isEmoji ? Type::emoji : Type::mono;
    staged.region = region;

    const size_t bpp { isEmoji ? size_t { 4 } : size_t { 1 } };
    staged.pixelDataSize = static_cast<size_t> (width) * static_cast<size_t> (height) * bpp;
    staged.pixelData.allocate (staged.pixelDataSize, false);
    std::memcpy (staged.pixelData.get(), pixelData, staged.pixelDataSize);

    jassert (uploadCount < uploadCapacity);
    uploadQueue[uploadCount] = std::move (staged);
    ++uploadCount;
}

/**
 * @brief Invalidate all cached glyphs and reset both atlas packers.
 *
 * Clears both LRU caches (freeing all `Region` entries) and resets both
 * `AtlasPacker` instances to empty.  The upload queue is also drained by
 * zeroing `uploadCount`.  Call this when the font size changes or the GL
 * context is recreated.
 *
 * @note MESSAGE THREAD only.
 */
void Atlas::clear() noexcept
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
void Atlas::advanceFrame() noexcept
{
    monoLRU.advanceFrame();
    emojiLRU.advanceFrame();
}

/**
 * @brief Return a cached box-drawing glyph or rasterize it procedurally.
 *
 * Constructs a synthetic `Key` with `fontFace = nullptr` and
 * `fontSize = 0` to uniquely identify box-drawing glyphs independently of
 * any font.  On a cache miss, calls `BoxDrawing::rasterize()` to fill a
 * `cellWidth × cellHeight` R8 buffer, allocates a mono atlas region, and
 * stages the bitmap for upload.
 *
 * The resulting `Region` always has:
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
 * @return Pointer to the `Region`, or `nullptr` if the mono atlas is full.
 *
 * @note MESSAGE THREAD only.
 * @see BoxDrawing::rasterize()
 */
Region* Atlas::getOrRasterizeBoxDrawing (uint32_t codepoint, int cellWidth, int cellHeight, int baseline) noexcept
{
    Key key;
    key.glyphIndex = codepoint;
    key.fontFace   = nullptr;
    key.fontSize   = 0.0f;
    key.span       = 1;

    Region* cached { monoLRU.get (key) };
    Region* result { cached };

    if (cached == nullptr)
    {
        const int bitmapW { cellWidth };
        const int bitmapH { cellHeight };
        const size_t bufSize { static_cast<size_t> (bitmapW) * static_cast<size_t> (bitmapH) };
        juce::HeapBlock<uint8_t> buf (bufSize, true);

        BoxDrawing::rasterize (codepoint, bitmapW, bitmapH, buf.get(), embolden);

        auto region { monoPacker.allocate (bitmapW, bitmapH) };

        if (not region.isEmpty())
        {
            Region glyph;
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
// CoreText implementation in jreng_glyph_atlas.mm
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
 * @return Populated `Region` on success; zero-dimension glyph on failure.
 *
 * @note MESSAGE THREAD only.
 * @see Constraint
 * @see copyBitmapRows()
 */
Region Atlas::rasterizeGlyph (const Key& key, void* fontHandle, bool isEmoji,
                              const Constraint& constraint,
                              int cellWidth, int cellHeight, int baseline) noexcept
{
    FT_Face face { static_cast<FT_Face> (fontHandle) };
    jassert (face != nullptr);

    Region glyph;

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
                    const int spanCells { std::max (1, static_cast<int> (key.span)) };
                    const float effCellW { static_cast<float> (cellWidth * spanCells) };
                    const float effCellH { static_cast<float> (cellHeight) };

                    const float glyphNaturalW { static_cast<float> (face->glyph->metrics.width) / ftFixed };
                    const float glyphNaturalH { static_cast<float> (face->glyph->metrics.height) / ftFixed };

                    const auto xform { computeConstraintTransform (constraint, glyphNaturalW, glyphNaturalH, effCellW, effCellH) };

                    FT_Outline& outline { face->glyph->outline };
                    const float naturalX { static_cast<float> (face->glyph->metrics.horiBearingX) / ftFixed };
                    const float naturalY { static_cast<float> (face->glyph->metrics.horiBearingY) / ftFixed };

                    for (int p { 0 }; p < outline.n_points; ++p)
                    {
                        float px { static_cast<float> (outline.points[p].x) / ftFixed };
                        float py { static_cast<float> (outline.points[p].y) / ftFixed };

                        px -= naturalX;
                        py -= (naturalY - glyphNaturalH);

                        px *= xform.scaleX;
                        py *= xform.scaleY;

                        px += xform.posX;
                        py += xform.posY;

                        outline.points[p].x = static_cast<FT_Pos> (px * ftFixed);
                        outline.points[p].y = static_cast<FT_Pos> (py * ftFixed);
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
                        FT_Outline_Embolden (&face->glyph->outline, jreng::Typeface::ftFixedScale);
                    }

                    FT_Render_Glyph (face->glyph, FT_RENDER_MODE_LIGHT);

                    const int originalBitmapTop { face->glyph->bitmap_top };
                    const int originalBitmapRows { static_cast<int> (face->glyph->bitmap.rows) };

                    // Kitty-style oversize rescale: if the rasterized bitmap is wider
                    // than the cell, shrink the face proportionally and re-render.
                    // Prevents proportional fallback glyphs (e.g. Segoe UI Symbol
                    // arrows) from bleeding into adjacent cells.
                    // Reference: kitty/freetype.c:624-631.
                    const int effectiveWidth { cellWidth * std::max (1, static_cast<int> (key.span)) };

                    if (face->glyph->bitmap.width > static_cast<unsigned int> (effectiveWidth)
                        and FT_IS_SCALABLE (face))
                    {
                        const FT_UInt savedXppem { face->size->metrics.x_ppem };
                        const FT_UInt savedYppem { face->size->metrics.y_ppem };
                        const float scale { static_cast<float> (effectiveWidth)
                                            / static_cast<float> (face->glyph->bitmap.width) };
                        const FT_UInt scaledX { std::max (1u, static_cast<FT_UInt> (static_cast<float> (savedXppem) * scale)) };
                        const FT_UInt scaledY { std::max (1u, static_cast<FT_UInt> (static_cast<float> (savedYppem) * scale)) };

                        FT_Set_Pixel_Sizes (face, scaledX, scaledY);
                        FT_Load_Glyph (face, key.glyphIndex, FT_LOAD_TARGET_LIGHT);

                        if (embolden)
                        {
                            FT_Outline_Embolden (&face->glyph->outline, jreng::Typeface::ftFixedScale);
                        }

                        FT_Render_Glyph (face->glyph, FT_RENDER_MODE_LIGHT);
                        FT_Set_Pixel_Sizes (face, savedXppem, savedYppem);
                    }

                    auto& bitmap { face->glyph->bitmap };

                    if (bitmap.width > 0 and bitmap.rows > 0)
                    {
                        auto region { monoPacker.allocate (static_cast<int> (bitmap.width), static_cast<int> (bitmap.rows)) };

                        if (not region.isEmpty())
                        {
                            glyph.widthPixels = static_cast<int> (bitmap.width);
                            glyph.heightPixels = static_cast<int> (bitmap.rows);
                            glyph.bearingX = face->glyph->bitmap_left;
                            const int verticalCenter { originalBitmapTop - originalBitmapRows / 2 };
                            glyph.bearingY = verticalCenter + static_cast<int> (bitmap.rows) / 2;

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

} // namespace jreng::Glyph
