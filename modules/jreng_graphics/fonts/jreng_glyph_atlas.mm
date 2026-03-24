/**
 * @file jreng_glyph_atlas.mm
 * @brief CoreText rasterization backend for Atlas (macOS only).
 *
 * This translation unit is compiled **only on macOS** (`JUCE_MAC`).  It
 * provides the macOS-specific implementations of:
 * - `Atlas::flipBitmapVertically()` — in-place vertical flip for
 *   CoreText bitmaps (CGBitmapContext uses bottom-left origin).
 * - `Atlas::rasterizeGlyph()` — CoreText + CGBitmapContext rasterization.
 *
 * On Linux and Windows, `jreng_glyph_atlas.cpp` provides the FreeType equivalents.
 *
 * ### CoreText rasterization paths
 *
 * | Condition                              | Context type  | Embolden mode        |
 * |----------------------------------------|---------------|----------------------|
 * | Constrained NF icon (non-emoji)        | DeviceGray    | fill only            |
 * | Normal monochrome glyph, no embolden   | DeviceGray    | fill only            |
 * | Normal monochrome glyph, embolden      | DeviceGray    | fill + stroke (1 pt) |
 * | Color emoji                            | DeviceRGB     | n/a                  |
 *
 * ### Anti-aliasing padding
 * `CTFontGetBoundingRectsForGlyphs` returns outline bounds, not rasterized
 * pixel bounds.  Anti-aliased rendering extends beyond the outline by up to
 * one screen pixel on each side.  The font is created at `fontSize ×
 * displayScale`, so bounds are in physical pixels.  The code expands by
 * `displayScale` (one screen pixel in physical coords) before calling
 * `CGRectIntegral` to capture the full AA fringe.
 *
 * @see jreng_glyph_atlas.h
 * @see jreng_glyph_atlas.cpp
 */

// Included via unity build (jreng_glyph.mm → jreng_glyph.cpp) — jreng_glyph.h already in scope

#if JUCE_MAC

#include <CoreText/CoreText.h>
#include <CoreGraphics/CoreGraphics.h>

namespace jreng::Glyph
{

static void ensurePooledContext (int neededW, int neededH,
                                 juce::HeapBlock<uint8_t>& buffer,
                                 CGContextRef& ctx,
                                 CGColorSpaceRef colorSpace,
                                 int& poolW, int& poolH,
                                 size_t bpp, uint32_t bitmapInfo) noexcept
{
    if (ctx != nullptr)
    {
        CGContextRelease (ctx);
        ctx = nullptr;
    }

    if (neededW > poolW or neededH > poolH)
    {
        poolW = std::max (poolW, neededW);
        poolH = std::max (poolH, neededH);
        buffer.allocate (static_cast<size_t> (poolW) * static_cast<size_t> (poolH) * bpp, true);
    }
    else
    {
        std::memset (buffer.get(), 0,
                     static_cast<size_t> (neededW) * static_cast<size_t> (neededH) * bpp);
    }

    ctx = CGBitmapContextCreate (buffer.get(),
                                 static_cast<size_t> (neededW),
                                 static_cast<size_t> (neededH),
                                 8,
                                 static_cast<size_t> (neededW) * bpp,
                                 colorSpace,
                                 bitmapInfo);
}


/**
 * @brief Flip a pixel buffer vertically in-place (macOS only).
 *
 * CoreText renders into a `CGBitmapContext` with a bottom-left coordinate
 * origin, producing a bitmap that is upside-down relative to the top-left
 * convention used by OpenGL's `glTexSubImage2D`.  This function corrects the
 * orientation by swapping rows symmetrically around the vertical midpoint.
 *
 * @param data          Pointer to the pixel buffer to flip.
 * @param width         Image width in pixels.
 * @param height        Image height in pixels.
 * @param bytesPerPixel Bytes per pixel: 1 for DeviceGray (mono), 4 for
 *                      DeviceRGB (emoji, premultiplied BGRA).
 *
 * @note MESSAGE THREAD only.
 */
void Atlas::flipBitmapVertically (uint8_t* data, int width, int height, int bytesPerPixel) noexcept
{
    const size_t rowBytes { static_cast<size_t> (width * bytesPerPixel) };
    juce::HeapBlock<uint8_t> tempRow (rowBytes);

    for (int y { 0 }; y < height / 2; ++y)
    {
        uint8_t* topRow { data + static_cast<ptrdiff_t> (y) * static_cast<ptrdiff_t> (rowBytes) };
        uint8_t* bottomRow { data + static_cast<ptrdiff_t> (height - 1 - y) * static_cast<ptrdiff_t> (rowBytes) };

        std::memcpy (tempRow.get(), topRow, rowBytes);
        std::memcpy (topRow, bottomRow, rowBytes);
        std::memcpy (bottomRow, tempRow.get(), rowBytes);
    }
}

Atlas::Atlas (AtlasSize size) noexcept
    : atlasWidth (static_cast<int> (size))
    , atlasHeight (static_cast<int> (size))
    , monoPacker (static_cast<int> (size), static_cast<int> (size))
    , emojiPacker (static_cast<int> (size), static_cast<int> (size))
    , monoLRU (monoLruCapacity)
    , emojiLRU (emojiLruCapacity)
{
    monoColorSpace = CGColorSpaceCreateDeviceGray();
    emojiColorSpace = CGColorSpaceCreateDeviceRGB();
}

Atlas::~Atlas()
{
    if (monoPoolContext != nullptr)
    {
        CGContextRelease (monoPoolContext);
    }

    if (emojiPoolContext != nullptr)
    {
        CGContextRelease (emojiPoolContext);
    }

    if (monoColorSpace != nullptr)
    {
        CGColorSpaceRelease (monoColorSpace);
    }

    if (emojiColorSpace != nullptr)
    {
        CGColorSpaceRelease (emojiColorSpace);
    }
}

/**
 * @brief Rasterize a single glyph using CoreText (macOS backend).
 *
 * Dispatches to one of two rasterization paths based on whether a
 * `Constraint` is active:
 *
 * @par Path 1 — Constrained NF icon (non-emoji, active constraint)
 * Computes scale factors and position offsets from the constraint's scale
 * mode, alignment, and padding, then applies them to the CGContext via
 * `CGContextTranslateCTM` / `CGContextScaleCTM` before drawing.  The bitmap
 * is always `cellWidth × cellHeight` and stored in the mono atlas with
 * `bearingX = 0`, `bearingY = baseline`.
 *
 * @par Path 2 — Normal glyph (mono or emoji)
 * Queries `CTFontGetBoundingRectsForGlyphs` for the outline bounds, expands
 * by one display-scale pixel to capture the AA fringe, then calls
 * `CGRectIntegral` to snap to pixel boundaries.  The draw origin is offset
 * by `-integralBounds.origin` so the glyph is positioned at (0, 0) within
 * the bitmap.
 *
 * - **Mono**: DeviceGray context, 8 bpp.  Embolden via fill+stroke (1 pt
 *   stroke) when `embolden` is `true`.
 * - **Emoji**: DeviceRGB context, 32 bpp, `kCGImageAlphaPremultipliedFirst |
 *   kCGBitmapByteOrder32Host` (BGRA on little-endian).
 *
 * @param key        Glyph identity (index, face, size, span).
 * @param fontHandle Opaque `CTFontRef` handle.
 * @param isEmoji    `true` for RGBA8 color rendering.
 * @param constraint Nerd Font layout descriptor.
 * @param cellWidth  Cell width in physical pixels.
 * @param cellHeight Cell height in physical pixels.
 * @param baseline   Pixels from cell top to baseline.
 * @return Populated `Region` on success; zero-dimension glyph on failure
 *         (null font handle, zero-size bounds, atlas full, null CGContext).
 *
 * @note MESSAGE THREAD only.
 * @see Constraint
 * @see flipBitmapVertically()
 */
Region Atlas::rasterizeGlyph (const Key& key, void* fontHandle, bool isEmoji,
                              const Constraint& constraint,
                              int cellWidth, int cellHeight, int baseline) noexcept
{
    Region glyph;

    if (fontHandle != nullptr)
    {
        CTFontRef font { static_cast<CTFontRef> (fontHandle) };
        CGGlyph cgGlyph { static_cast<CGGlyph> (key.glyphIndex) };

        CGRect bounds {};
        CTFontGetBoundingRectsForGlyphs (font, kCTFontOrientationHorizontal, &cgGlyph, &bounds, 1);

        if (constraint.isActive() and not isEmoji)
        {
            const int spanCells { std::max (1, static_cast<int> (key.span)) };
            const float effCellW { static_cast<float> (cellWidth * spanCells) };
            const float effCellH { static_cast<float> (cellHeight) };

            const float glyphNaturalW { static_cast<float> (bounds.size.width) };
            const float glyphNaturalH { static_cast<float> (bounds.size.height) };

            const auto xform { computeConstraintTransform (constraint, glyphNaturalW, glyphNaturalH, effCellW, effCellH) };

            const int cellBitmapW { cellWidth * spanCells };
            const int cellBitmapH { cellHeight };
            auto region { monoPacker.allocate (cellBitmapW, cellBitmapH) };

            if (not region.isEmpty())
            {
                glyph.bearingX = 0;
                glyph.bearingY = baseline;
                glyph.widthPixels = cellBitmapW;
                glyph.heightPixels = cellBitmapH;

                const float aw { static_cast<float> (atlasWidth) };
                const float ah { static_cast<float> (atlasHeight) };
                glyph.textureCoordinates = juce::Rectangle<float> {
                    static_cast<float> (region.getX()) / aw,
                    static_cast<float> (region.getY()) / ah,
                    static_cast<float> (cellBitmapW) / aw,
                    static_cast<float> (cellBitmapH) / ah };

                ensurePooledContext (cellBitmapW, cellBitmapH,
                                     monoPoolBuffer, monoPoolContext, monoColorSpace,
                                     monoPoolWidth, monoPoolHeight,
                                     1, kCGImageAlphaNone);

                if (monoPoolContext != nullptr)
                {
                    CGContextSaveGState (monoPoolContext);

                    CGContextSetShouldAntialias (monoPoolContext, true);
                    CGContextSetShouldSmoothFonts (monoPoolContext, true);
                    CGContextSetGrayFillColor (monoPoolContext, 1.0, 1.0);
                    CGContextSetTextDrawingMode (monoPoolContext, kCGTextFill);

                    CGContextTranslateCTM (monoPoolContext, static_cast<CGFloat> (xform.posX), static_cast<CGFloat> (xform.posY));
                    CGContextScaleCTM (monoPoolContext, static_cast<CGFloat> (xform.scaleX), static_cast<CGFloat> (xform.scaleY));

                    const CGPoint position { -bounds.origin.x, -bounds.origin.y };
                    CTFontDrawGlyphs (font, &cgGlyph, &position, 1, monoPoolContext);

                    CGContextRestoreGState (monoPoolContext);

                    stageForUpload (monoPoolBuffer.get(), cellBitmapW, cellBitmapH, region, false);
                }
            }
        }
        else
        {
            // CTFontGetBoundingRectsForGlyphs returns outline bounds, not rasterized pixel bounds.
            // Antialiased rendering extends beyond the outline by up to 1 screen pixel on each side.
            // The font is created at fontSize * displayScale, so bounds are in physical pixels.
            // Expand by displayScale (1 screen pixel in physical coords) to capture the full AA fringe.
            const CGFloat aaPad { static_cast<CGFloat> (displayScale) };
            const CGRect aaPadded { CGRectInset (bounds, -aaPad, -aaPad) };
            const CGRect integralBounds { CGRectIntegral (aaPadded) };

            const int pixelWidth { static_cast<int> (integralBounds.size.width) };
            const int pixelHeight { static_cast<int> (integralBounds.size.height) };

            if (pixelWidth > 0 and pixelHeight > 0)
            {
                auto& packer { isEmoji ? emojiPacker : monoPacker };
                auto region { packer.allocate (pixelWidth, pixelHeight) };

                if (not region.isEmpty())
                {
                    glyph.bearingX = static_cast<int> (integralBounds.origin.x);
                    glyph.bearingY = static_cast<int> (integralBounds.origin.y + integralBounds.size.height);
                    glyph.widthPixels = pixelWidth;
                    glyph.heightPixels = pixelHeight;

                    const float aw { static_cast<float> (atlasWidth) };
                    const float ah { static_cast<float> (atlasHeight) };
                    glyph.textureCoordinates = juce::Rectangle<float> {
                        static_cast<float> (region.getX()) / aw,
                        static_cast<float> (region.getY()) / ah,
                        static_cast<float> (pixelWidth) / aw,
                        static_cast<float> (pixelHeight) / ah };

                    const CGFloat drawX { -integralBounds.origin.x };
                    const CGFloat drawY { -integralBounds.origin.y };

                    if (isEmoji)
                    {
                        ensurePooledContext (pixelWidth, pixelHeight,
                                             emojiPoolBuffer, emojiPoolContext, emojiColorSpace,
                                             emojiPoolWidth, emojiPoolHeight,
                                             4, kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host);

                        if (emojiPoolContext != nullptr)
                        {
                            CGContextSaveGState (emojiPoolContext);

                            CGContextSetShouldAntialias (emojiPoolContext, true);
                            CGContextSetShouldSmoothFonts (emojiPoolContext, true);

                            const CGPoint position { drawX, drawY };
                            CTFontDrawGlyphs (font, &cgGlyph, &position, 1, emojiPoolContext);

                            CGContextRestoreGState (emojiPoolContext);

                            stageForUpload (emojiPoolBuffer.get(), pixelWidth, pixelHeight, region, true);
                        }
                    }
                    else
                    {
                        ensurePooledContext (pixelWidth, pixelHeight,
                                             monoPoolBuffer, monoPoolContext, monoColorSpace,
                                             monoPoolWidth, monoPoolHeight,
                                             1, kCGImageAlphaNone);

                        if (monoPoolContext != nullptr)
                        {
                            CGContextSaveGState (monoPoolContext);

                            CGContextSetShouldAntialias (monoPoolContext, true);
                            CGContextSetShouldSmoothFonts (monoPoolContext, true);
                            CGContextSetGrayFillColor (monoPoolContext, 1.0, 1.0);

                            if (embolden)
                            {
                                CGContextSetGrayStrokeColor (monoPoolContext, 1.0, 1.0);
                                CGContextSetLineWidth (monoPoolContext, 1.0);
                                CGContextSetTextDrawingMode (monoPoolContext, kCGTextFillStroke);
                            }
                            else
                            {
                                CGContextSetTextDrawingMode (monoPoolContext, kCGTextFill);
                            }

                            const CGPoint position { drawX, drawY };
                            CTFontDrawGlyphs (font, &cgGlyph, &position, 1, monoPoolContext);

                            CGContextRestoreGState (monoPoolContext);

                            stageForUpload (monoPoolBuffer.get(), pixelWidth, pixelHeight, region, false);
                        }
                    }
                }
            }
        }
    }

    return glyph;
}

} // namespace jreng::Glyph

#endif // JUCE_MAC
