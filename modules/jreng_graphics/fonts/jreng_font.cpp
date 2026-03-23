/**
 * @file jreng_font.cpp
 * @brief Implementation of jreng::Font glyph access and delegation methods.
 */

namespace jreng
{

// =============================================================================
// Glyph access
// =============================================================================

jreng::Glyph::Region* Font::getGlyph (uint16_t glyphCode) noexcept
{
    const jreng::Glyph::Constraint defaultConstraint;
    return getGlyph (glyphCode, defaultConstraint, 1);
}

jreng::Glyph::Region* Font::getGlyph (uint16_t glyphCode,
                                       const jreng::Glyph::Constraint& constraint,
                                       uint8_t span) noexcept
{
    void* resolvedHandle { faceHandle };

    if (resolvedHandle == nullptr)
    {
        resolvedHandle = emoji ? typeface.getEmojiFontHandle()
                               : typeface.getFontHandle (style);
    }

    const Typeface::Metrics m { typeface.calcMetrics (size) };

    const jreng::Glyph::Key key { static_cast<uint32_t> (glyphCode),
                                   resolvedHandle,
                                   size,
                                   span };

    return typeface.getOrRasterize (key, resolvedHandle, emoji, constraint,
                                    m.physCellW, m.physCellH, m.physBaseline);
}

// =============================================================================
// Atlas delegation
// =============================================================================

void Font::consumeStagedBitmaps (juce::HeapBlock<jreng::Glyph::StagedBitmap>& out,
                                  int& outCount) noexcept
{
    typeface.consumeStagedBitmaps (out, outCount);
}

bool Font::hasStagedBitmaps() const noexcept
{
    return typeface.hasStagedBitmaps();
}

} // namespace jreng
