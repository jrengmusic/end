/**
 * @file GlyphAtlas.mm
 * @brief CoreText rasterization backend for GlyphAtlas (macOS only).
 *
 * This translation unit is compiled **only on macOS** (`JUCE_MAC`).  It
 * provides the macOS-specific implementations of:
 * - `GlyphAtlas::flipBitmapVertically()` — in-place vertical flip for
 *   CoreText bitmaps (CGBitmapContext uses bottom-left origin).
 * - `GlyphAtlas::rasterizeGlyph()` — CoreText + CGBitmapContext rasterization.
 *
 * On Linux and Windows, `GlyphAtlas.cpp` provides the FreeType equivalents.
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
 * `Fonts::getDisplayScale()` (one screen pixel in physical coords) before
 * calling `CGRectIntegral` to capture the full AA fringe.
 *
 * @see GlyphAtlas.h
 * @see GlyphAtlas.cpp
 * @see Fonts::getDisplayScale()
 */

#include "GlyphAtlas.h"
#include "Fonts.h"

#if JUCE_MAC

#include <CoreText/CoreText.h>
#include <CoreGraphics/CoreGraphics.h>

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
void GlyphAtlas::flipBitmapVertically (uint8_t* data, int width, int height, int bytesPerPixel) noexcept
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

/**
 * @brief Rasterize a single glyph using CoreText (macOS backend).
 *
 * Dispatches to one of two rasterization paths based on whether a
 * `GlyphConstraint` is active:
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
 * @return Populated `AtlasGlyph` on success; zero-dimension glyph on failure
 *         (null font handle, zero-size bounds, atlas full, null CGContext).
 *
 * @note MESSAGE THREAD only.
 * @see GlyphConstraint
 * @see Fonts::getDisplayScale()
 * @see flipBitmapVertically()
 */
AtlasGlyph GlyphAtlas::rasterizeGlyph (const GlyphKey& key, void* fontHandle, bool isEmoji,
                                         const GlyphConstraint& constraint,
                                         int cellWidth, int cellHeight, int baseline) noexcept
{
    AtlasGlyph glyph;

    if (fontHandle != nullptr)
    {
        CTFontRef font { static_cast<CTFontRef> (fontHandle) };
        CGGlyph cgGlyph { static_cast<CGGlyph> (key.glyphIndex) };

        CGRect bounds {};
        CTFontGetBoundingRectsForGlyphs (font, kCTFontOrientationHorizontal, &cgGlyph, &bounds, 1);

        if (constraint.isActive() and not isEmoji)
        {
            const int spanCells { std::max (1, static_cast<int> (key.cellSpan)) };
            const float effCellW { static_cast<float> (cellWidth * spanCells) };
            const float effCellH { static_cast<float> (cellHeight) };

            const float inset { GlyphConstraint::iconInset };
            const float targetW { effCellW * (1.0f - constraint.padLeft - constraint.padRight - inset * 2.0f) };
            const float targetH { effCellH * (1.0f - constraint.padTop - constraint.padBottom - inset * 2.0f) };

            const float glyphNaturalW { static_cast<float> (bounds.size.width) };
            const float glyphNaturalH { static_cast<float> (bounds.size.height) };

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

                const size_t bufSize { static_cast<size_t> (cellBitmapW) * static_cast<size_t> (cellBitmapH) };
                juce::HeapBlock<uint8_t> buf (bufSize, true);

                CGColorSpaceRef colorSpace { CGColorSpaceCreateDeviceGray() };
                CGContextRef ctx { CGBitmapContextCreate (buf.get(), static_cast<size_t> (cellBitmapW),
                                                          static_cast<size_t> (cellBitmapH), 8,
                                                          static_cast<size_t> (cellBitmapW),
                                                          colorSpace,
                                                          kCGImageAlphaNone) };
                CGColorSpaceRelease (colorSpace);

                if (ctx != nullptr)
                {
                    CGContextSetShouldAntialias (ctx, true);
                    CGContextSetShouldSmoothFonts (ctx, true);
                    CGContextSetGrayFillColor (ctx, 1.0, 1.0);
                    CGContextSetTextDrawingMode (ctx, kCGTextFill);

                    CGContextTranslateCTM (ctx, static_cast<CGFloat> (posX), static_cast<CGFloat> (posY));
                    CGContextScaleCTM (ctx, static_cast<CGFloat> (scaleX), static_cast<CGFloat> (scaleY));

                    const CGPoint position { -bounds.origin.x, -bounds.origin.y };
                    CTFontDrawGlyphs (font, &cgGlyph, &position, 1, ctx);
                    CGContextRelease (ctx);

                    stageForUpload (buf.get(), cellBitmapW, cellBitmapH, region, false);
                }
            }
        }
        else
        {
            // CTFontGetBoundingRectsForGlyphs returns outline bounds, not rasterized pixel bounds.
            // Antialiased rendering extends beyond the outline by up to 1 screen pixel on each side.
            // The font is created at fontSize * displayScale, so bounds are in physical pixels.
            // Expand by displayScale (1 screen pixel in physical coords) to capture the full AA fringe.
            const CGFloat aaPad { static_cast<CGFloat> (Fonts::getDisplayScale()) };
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
                        const size_t bufSize { static_cast<size_t> (pixelWidth) * static_cast<size_t> (pixelHeight) * 4 };
                        juce::HeapBlock<uint8_t> buf (bufSize, true);

                        CGColorSpaceRef colorSpace { CGColorSpaceCreateDeviceRGB() };
                        CGContextRef ctx { CGBitmapContextCreate (buf.get(), static_cast<size_t> (pixelWidth),
                                                                  static_cast<size_t> (pixelHeight), 8,
                                                                  static_cast<size_t> (pixelWidth) * 4,
                                                                  colorSpace,
                                                                  kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host) };
                        CGColorSpaceRelease (colorSpace);

                        if (ctx != nullptr)
                        {
                            CGContextSetShouldAntialias (ctx, true);
                            CGContextSetShouldSmoothFonts (ctx, true);

                            const CGPoint position { drawX, drawY };
                            CTFontDrawGlyphs (font, &cgGlyph, &position, 1, ctx);
                            CGContextRelease (ctx);

                            stageForUpload (buf.get(), pixelWidth, pixelHeight, region, true);
                        }
                    }
                    else
                    {
                        const size_t bufSize { static_cast<size_t> (pixelWidth) * static_cast<size_t> (pixelHeight) };
                        juce::HeapBlock<uint8_t> buf (bufSize, true);

                        CGColorSpaceRef colorSpace { CGColorSpaceCreateDeviceGray() };
                        CGContextRef ctx { CGBitmapContextCreate (buf.get(), static_cast<size_t> (pixelWidth),
                                                                  static_cast<size_t> (pixelHeight), 8,
                                                                  static_cast<size_t> (pixelWidth),
                                                                  colorSpace,
                                                                  kCGImageAlphaNone) };
                        CGColorSpaceRelease (colorSpace);

                        if (ctx != nullptr)
                        {
                            CGContextSetShouldAntialias (ctx, true);
                            CGContextSetShouldSmoothFonts (ctx, true);
                            CGContextSetGrayFillColor (ctx, 1.0, 1.0);

                            if (embolden)
                            {
                                CGContextSetGrayStrokeColor (ctx, 1.0, 1.0);
                                CGContextSetLineWidth (ctx, 1.0);
                                CGContextSetTextDrawingMode (ctx, kCGTextFillStroke);
                            }
                            else
                            {
                                CGContextSetTextDrawingMode (ctx, kCGTextFill);
                            }

                            const CGPoint position { drawX, drawY };
                            CTFontDrawGlyphs (font, &cgGlyph, &position, 1, ctx);
                            CGContextRelease (ctx);

                            stageForUpload (buf.get(), pixelWidth, pixelHeight, region, false);
                        }
                    }
                }
            }
        }
    }

    return glyph;
}

#endif // JUCE_MAC
