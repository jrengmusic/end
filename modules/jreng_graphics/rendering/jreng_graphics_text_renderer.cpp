/**
 * @file jreng_graphics_text_renderer.cpp
 * @brief Implementation of GraphicsTextRenderer — direct BitmapData compositing.
 */

#include "jreng_simd_blend.h"

namespace jreng::Glyph
{

juce::Image GraphicsTextRenderer::sharedMonoAtlas;
juce::Image GraphicsTextRenderer::sharedEmojiAtlas;
int GraphicsTextRenderer::sharedAtlasRefCount { 0 };

// =========================================================================
// Lifecycle
// =========================================================================

void GraphicsTextRenderer::createContext() noexcept
{
    if (sharedAtlasRefCount == 0)
    {
        const int dim { getAtlasDimension() };

        sharedMonoAtlas  = juce::Image (juce::Image::SingleChannel, dim, dim, true,
                                         juce::SoftwareImageType());
        sharedEmojiAtlas = juce::Image (juce::Image::ARGB, dim, dim, true,
                                         juce::SoftwareImageType());
    }

    ++sharedAtlasRefCount;
    contextInitialised = true;
}

void GraphicsTextRenderer::closeContext() noexcept
{
    if (contextInitialised)
    {
        contextInitialised = false;
        --sharedAtlasRefCount;

        if (sharedAtlasRefCount == 0)
        {
            sharedMonoAtlas  = juce::Image();
            sharedEmojiAtlas = juce::Image();
        }
    }

    renderTarget = juce::Image();
}

// =========================================================================
// Per-frame operations
// =========================================================================

void GraphicsTextRenderer::uploadStagedBitmaps (jreng::Typeface& typeface) noexcept
{
    typeface.consumeStagedBitmaps (stagedBuffer, stagedCount);

    if (stagedCount > 0)
    {
        for (int i { 0 }; i < stagedCount; ++i)
        {
            auto& staged { stagedBuffer[i] };
            const auto region { staged.region };
            const bool isMono { staged.type == Type::mono };
            auto& atlas { isMono ? sharedMonoAtlas : sharedEmojiAtlas };
            const int pixelStride { isMono ? 1 : 4 };

            juce::Image::BitmapData destData (atlas, region.getX(), region.getY(),
                                               region.getWidth(), region.getHeight(),
                                               juce::Image::BitmapData::writeOnly);

            const uint8_t* src { staged.pixelData.get() };
            const int srcStride { region.getWidth() * pixelStride };

            for (int row { 0 }; row < region.getHeight(); ++row)
            {
                std::memcpy (destData.getLinePointer (row),
                             src + row * srcStride,
                             static_cast<size_t> (srcStride));
            }
        }

        stagedCount = 0;
    }
}

void GraphicsTextRenderer::setViewportSize (int width, int height) noexcept
{
    viewportWidth  = width;
    viewportHeight = height;
}

void GraphicsTextRenderer::push (int x, int y, int w, int h, int fullHeight) noexcept
{
    juce::ignoreUnused (fullHeight);
    viewportWidth  = w;
    viewportHeight = h;
    frameOffsetX   = x;
    frameOffsetY   = y;

    if (not renderTarget.isValid()
        or renderTarget.getWidth() != w
        or renderTarget.getHeight() != h)
    {
        renderTarget = juce::Image (juce::Image::ARGB, w, h, true,
                                     juce::NativeImageType());
    }
    // No clear — render target persists between frames.
    // Dirty rows are composited selectively in prepareFrame().
}

void GraphicsTextRenderer::pop() noexcept
{
    if (graphics != nullptr and renderTarget.isValid())
    {
        const float scale { jreng::Typeface::getDisplayScale() };
        const float inverseScale { 1.0f / scale };

        graphics->saveState();
        graphics->addTransform (juce::AffineTransform::scale (inverseScale));
        graphics->drawImageAt (renderTarget, frameOffsetX, frameOffsetY);
        graphics->restoreState();
    }
}

void GraphicsTextRenderer::prepareFrame (const uint64_t* dirtyRows, int scrollDelta,
                                          int cellHeight, int totalRows, int scrollOffset) noexcept
{
    if (renderTarget.isValid() and cellHeight > 0)
    {
        const bool viewportChanged { scrollOffset != previousScrollOffset };
        previousScrollOffset = scrollOffset;

        const bool hasScroll { scrollDelta > 0 };
        const bool needsFullClear { viewportChanged or hasScroll };

        if (needsFullClear)
        {
            renderTarget.clear (juce::Rectangle<int> (0, 0, viewportWidth, viewportHeight));
        }
        else
        {
            // Clear only accumulated dirty rows.
            for (int r { 0 }; r < totalRows; ++r)
            {
                const int word { r >> 6 };
                const uint64_t bit { uint64_t (1) << (r & 63) };

                if ((dirtyRows[word] & bit) != 0)
                {
                    const int rowY { r * cellHeight };
                    const int rowH { std::min (cellHeight, viewportHeight - rowY) };

                    if (rowH > 0)
                    {
                        renderTarget.clear (juce::Rectangle<int> (0, rowY, viewportWidth, rowH));
                    }
                }
            }

            // Clear below grid area — prevents stale pixels after zoom/resize.
            const int gridBottom { totalRows * cellHeight };

            if (gridBottom < viewportHeight)
            {
                renderTarget.clear (juce::Rectangle<int> (0, gridBottom,
                                                           viewportWidth,
                                                           viewportHeight - gridBottom));
            }
        }
    }
}

// =========================================================================
// Drawing
// =========================================================================

void GraphicsTextRenderer::drawQuads (const Render::Quad* data, int count, bool isEmoji) noexcept
{
    if (count > 0 and renderTarget.isValid())
    {
        if (isEmoji)
        {
            juce::Image::BitmapData targetData (renderTarget, juce::Image::BitmapData::readWrite);
            juce::Image::BitmapData atlasData  (sharedEmojiAtlas, juce::Image::BitmapData::readOnly);

            for (int i { 0 }; i < count; ++i)
            {
                compositeEmojiGlyph (targetData, atlasData, data[i]);
            }
        }
        else
        {
            juce::Image::BitmapData targetData (renderTarget, juce::Image::BitmapData::readWrite);
            juce::Image::BitmapData atlasData  (sharedMonoAtlas, juce::Image::BitmapData::readOnly);

            for (int i { 0 }; i < count; ++i)
            {
                compositeMonoGlyph (targetData, atlasData, data[i]);
            }
        }
    }
}

void GraphicsTextRenderer::drawBackgrounds (const Render::Background* data, int count) noexcept
{
    if (count > 0 and renderTarget.isValid())
    {
        juce::Image::BitmapData targetData (renderTarget, juce::Image::BitmapData::readWrite);

        for (int i { 0 }; i < count; ++i)
        {
            const auto& bg { data[i] };
            const auto& bounds { bg.screenBounds };

            const int x0 { std::max (0, static_cast<int> (bounds.getX())) };
            const int y0 { std::max (0, static_cast<int> (bounds.getY())) };
            const int x1 { std::min (viewportWidth,  static_cast<int> (std::ceil (bounds.getRight()))) };
            const int y1 { std::min (viewportHeight, static_cast<int> (std::ceil (bounds.getBottom()))) };

            if (x0 < x1 and y0 < y1)
            {
                const uint8_t a { static_cast<uint8_t> (juce::roundToInt (bg.backgroundColorA * 255.0f)) };
                const uint8_t r { static_cast<uint8_t> (juce::roundToInt (bg.backgroundColorR * a)) };
                const uint8_t g { static_cast<uint8_t> (juce::roundToInt (bg.backgroundColorG * a)) };
                const uint8_t b { static_cast<uint8_t> (juce::roundToInt (bg.backgroundColorB * a)) };

                if (a == 255)
                {
                    // Opaque: overwrite directly, no blend needed.
                    const uint32_t packedColor { (static_cast<uint32_t> (a) << 24u)
                                               | (static_cast<uint32_t> (r) << 16u)
                                               | (static_cast<uint32_t> (g) << 8u)
                                               |  static_cast<uint32_t> (b) };

                    for (int row { y0 }; row < y1; ++row)
                    {
                        auto* pixelU32 { reinterpret_cast<uint32_t*> (targetData.getLinePointer (row)) + x0 };
                        int col { x0 };

                        // SIMD: 4 pixels at a time.
                        for (; col + 3 < x1; col += 4)
                        {
                            jreng::simd::fillOpaque4 (pixelU32, packedColor);
                            pixelU32 += 4;
                        }

                        // Scalar tail.
                        for (; col < x1; ++col)
                        {
                            *pixelU32 = packedColor;
                            ++pixelU32;
                        }
                    }
                }
                else
                {
                    // Semi-transparent: src-over blend.
                    const uint32_t srcPixel { (static_cast<uint32_t> (a) << 24u)
                                            | (static_cast<uint32_t> (r) << 16u)
                                            | (static_cast<uint32_t> (g) << 8u)
                                            |  static_cast<uint32_t> (b) };

                    for (int row { y0 }; row < y1; ++row)
                    {
                        auto* pixelU32 { reinterpret_cast<uint32_t*> (targetData.getLinePointer (row)) + x0 };
                        int col { x0 };

                        // SIMD: 4 pixels at a time.
                        const uint32_t srcBatch[4] { srcPixel, srcPixel, srcPixel, srcPixel };
                        for (; col + 3 < x1; col += 4)
                        {
                            jreng::simd::blendSrcOver4 (pixelU32, srcBatch);
                            pixelU32 += 4;
                        }

                        // Scalar tail.
                        for (; col < x1; ++col)
                        {
                            const uint32_t d    { *pixelU32 };
                            const uint32_t invA { 255u - a };
                            const uint32_t rOut { r + ((((d >> 16u) & 0xFFu) * invA + 128u) >> 8u) };
                            const uint32_t gOut { g + ((((d >> 8u)  & 0xFFu) * invA + 128u) >> 8u) };
                            const uint32_t bOut { b + ((( d         & 0xFFu) * invA + 128u) >> 8u) };
                            const uint32_t aOut { a + ((((d >> 24u) & 0xFFu) * invA + 128u) >> 8u) };
                            *pixelU32 = (aOut << 24u) | (rOut << 16u) | (gOut << 8u) | bOut;
                            ++pixelU32;
                        }
                    }
                }
            }
        }
    }
}

void GraphicsTextRenderer::compositeMonoGlyph (juce::Image::BitmapData& targetData,
                                                const juce::Image::BitmapData& atlasData,
                                                const Render::Quad& quad) noexcept
{
    const int dim { getAtlasDimension() };
    const auto& uv { quad.textureCoordinates };

    const int srcX { juce::roundToInt (uv.getX() * dim) };
    const int srcY { juce::roundToInt (uv.getY() * dim) };
    const int srcW { juce::roundToInt (uv.getWidth() * dim) };
    const int srcH { juce::roundToInt (uv.getHeight() * dim) };

    const int destX { juce::roundToInt (quad.screenPosition.x) };
    const int destY { juce::roundToInt (quad.screenPosition.y) };

    // Clip to render target bounds.
    const int clipX0 { std::max (0, -destX) };
    const int clipY0 { std::max (0, -destY) };
    const int clipX1 { std::min (srcW, viewportWidth - destX) };
    const int clipY1 { std::min (srcH, viewportHeight - destY) };

    if (clipX0 < clipX1 and clipY0 < clipY1)
    {
        // Premultiply foreground colour once.
        const uint8_t fgR { static_cast<uint8_t> (juce::roundToInt (quad.foregroundColorR * 255.0f)) };
        const uint8_t fgG { static_cast<uint8_t> (juce::roundToInt (quad.foregroundColorG * 255.0f)) };
        const uint8_t fgB { static_cast<uint8_t> (juce::roundToInt (quad.foregroundColorB * 255.0f)) };

        const uint32_t fgPacked { (static_cast<uint32_t> (255u) << 24u)
                                 | (static_cast<uint32_t> (fgR) << 16u)
                                 | (static_cast<uint32_t> (fgG) << 8u)
                                 |  static_cast<uint32_t> (fgB) };

        for (int py { clipY0 }; py < clipY1; ++py)
        {
            const auto* srcRow { atlasData.getLinePointer (srcY + py) + srcX + clipX0 };
            auto* destU32 { reinterpret_cast<uint32_t*> (targetData.getLinePointer (destY + py)) + destX + clipX0 };
            int px { clipX0 };

            // SIMD: 4 pixels at a time.
            for (; px + 3 < clipX1; px += 4)
            {
                jreng::simd::blendMonoTinted4 (destU32, srcRow, fgPacked);
                srcRow  += 4;
                destU32 += 4;
            }

            // Scalar tail.
            for (; px < clipX1; ++px)
            {
                const uint8_t alpha { *srcRow };
                const uint8_t srcR { static_cast<uint8_t> ((fgR * alpha + 127) >> 8) };
                const uint8_t srcG { static_cast<uint8_t> ((fgG * alpha + 127) >> 8) };
                const uint8_t srcB { static_cast<uint8_t> ((fgB * alpha + 127) >> 8) };
                const uint8_t invA { static_cast<uint8_t> (255 - alpha) };

                auto* destPixel { reinterpret_cast<juce::PixelARGB*> (destU32) };
                destPixel->setARGB (
                    static_cast<uint8_t> (alpha + ((destPixel->getAlpha() * invA + 127) >> 8)),
                    static_cast<uint8_t> (srcR  + ((destPixel->getRed()   * invA + 127) >> 8)),
                    static_cast<uint8_t> (srcG  + ((destPixel->getGreen() * invA + 127) >> 8)),
                    static_cast<uint8_t> (srcB  + ((destPixel->getBlue()  * invA + 127) >> 8)));
                ++srcRow;
                ++destU32;
            }
        }
    }
}

void GraphicsTextRenderer::compositeEmojiGlyph (juce::Image::BitmapData& targetData,
                                                  const juce::Image::BitmapData& atlasData,
                                                  const Render::Quad& quad) noexcept
{
    const int dim { getAtlasDimension() };
    const auto& uv { quad.textureCoordinates };

    const int srcX { juce::roundToInt (uv.getX() * dim) };
    const int srcY { juce::roundToInt (uv.getY() * dim) };
    const int srcW { juce::roundToInt (uv.getWidth() * dim) };
    const int srcH { juce::roundToInt (uv.getHeight() * dim) };

    const int destX { juce::roundToInt (quad.screenPosition.x) };
    const int destY { juce::roundToInt (quad.screenPosition.y) };

    // Clip to render target bounds.
    const int clipX0 { std::max (0, -destX) };
    const int clipY0 { std::max (0, -destY) };
    const int clipX1 { std::min (srcW, viewportWidth - destX) };
    const int clipY1 { std::min (srcH, viewportHeight - destY) };

    if (clipX0 < clipX1 and clipY0 < clipY1)
    {
        for (int py { clipY0 }; py < clipY1; ++py)
        {
            auto* srcU32  { reinterpret_cast<const uint32_t*> (atlasData.getLinePointer (srcY + py)) + srcX + clipX0 };
            auto* destU32 { reinterpret_cast<uint32_t*> (targetData.getLinePointer (destY + py)) + destX + clipX0 };
            int px { clipX0 };

            // SIMD: 4 pixels at a time.
            for (; px + 3 < clipX1; px += 4)
            {
                jreng::simd::blendSrcOver4 (destU32, srcU32);
                srcU32  += 4;
                destU32 += 4;
            }

            // Scalar tail.
            for (; px < clipX1; ++px)
            {
                const uint8_t srcA { static_cast<uint8_t> (*srcU32 >> 24u) };

                if (srcA == 255)
                {
                    *destU32 = *srcU32;
                }
                else if (srcA > 0)
                {
                    const uint32_t s    { *srcU32 };
                    const uint32_t d    { *destU32 };
                    const uint32_t invA { 255u - srcA };
                    const uint32_t rOut { ((s >> 16u) & 0xFFu) + ((((d >> 16u) & 0xFFu) * invA + 128u) >> 8u) };
                    const uint32_t gOut { ((s >> 8u)  & 0xFFu) + ((((d >> 8u)  & 0xFFu) * invA + 128u) >> 8u) };
                    const uint32_t bOut { ( s         & 0xFFu) + ((( d         & 0xFFu) * invA + 128u) >> 8u) };
                    const uint32_t aOut { ((s >> 24u) & 0xFFu) + ((((d >> 24u) & 0xFFu) * invA + 128u) >> 8u) };
                    *destU32 = (aOut << 24u) | (rOut << 16u) | (gOut << 8u) | bOut;
                }

                ++srcU32;
                ++destU32;
            }
        }
    }
}

// =========================================================================
// TextLayout duck-type
// =========================================================================

void GraphicsTextRenderer::setFont (jreng::Font& font) noexcept
{
    currentFont = &font;
}

void GraphicsTextRenderer::drawGlyphs (const uint16_t* glyphCodes,
                                        const juce::Point<float>* positions,
                                        int count) noexcept
{
    jassert (currentFont != nullptr);

    if (count > 0 and renderTarget.isValid() and currentFont != nullptr)
    {
        const bool isMono { not currentFont->isEmoji() };
        auto& atlas { isMono ? sharedMonoAtlas : sharedEmojiAtlas };

        juce::Image::BitmapData targetData (renderTarget, juce::Image::BitmapData::readWrite);
        juce::Image::BitmapData atlasData  (atlas, juce::Image::BitmapData::readOnly);

        for (int i { 0 }; i < count; ++i)
        {
            auto* region { currentFont->getGlyph (glyphCodes[i]) };

            if (region != nullptr)
            {
                const Render::Quad quad
                {
                    { positions[i].x + static_cast<float> (region->bearingX),
                      positions[i].y - static_cast<float> (region->bearingY) },
                    { static_cast<float> (region->widthPixels),
                      static_cast<float> (region->heightPixels) },
                    region->textureCoordinates,
                    1.0f, 1.0f, 1.0f, 1.0f
                };

                if (isMono)
                    compositeMonoGlyph (targetData, atlasData, quad);
                else
                    compositeEmojiGlyph (targetData, atlasData, quad);
            }
        }
    }
}

// =========================================================================
// State queries
// =========================================================================

bool GraphicsTextRenderer::isReady() const noexcept
{
    return sharedMonoAtlas.isValid();
}

// =========================================================================
// Graphics context binding
// =========================================================================

void GraphicsTextRenderer::setGraphicsContext (juce::Graphics& g) noexcept
{
    graphics = &g;
}

} // namespace jreng::Glyph
