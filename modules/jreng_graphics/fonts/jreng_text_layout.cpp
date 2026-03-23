/**
 * @file jreng_text_layout.cpp
 * @brief Implementation of jreng::TextLayout.
 *
 * Shapes attributed text via jreng::Typeface::shapeText() (HarfBuzz), finds line
 * break opportunities via libunibreak's set_linebreaks_utf32(), and builds
 * Line/Run/Glyph structures for GL instanced rendering.
 */

#include "linebreak/linebreak.h"

namespace jreng
{

// =============================================================================
// Constants
// =============================================================================

/** @brief Very large float used as an unbounded maxWidth/maxHeight sentinel. */
static constexpr float unlimitedDimension { 1.0e6f };

/** @brief Default span for non-wide glyphs. */
static constexpr uint8_t defaultGlyphSpan { 1 };

// =============================================================================
// Run
// =============================================================================

juce::Range<float> TextLayout::Run::getRunBoundsX() const noexcept
{
    float minX { 0.0f };
    float maxX { 0.0f };

    if (glyphs.size() > 0)
    {
        minX = glyphs.getFirst().anchor.x;
        maxX = glyphs.getLast().anchor.x + glyphs.getLast().width;
    }

    return { minX, maxX };
}

// =============================================================================
// Line
// =============================================================================

juce::Rectangle<float> TextLayout::Line::getLineBounds() const noexcept
{
    float left  { 0.0f };
    float right { 0.0f };

    for (const auto* run : runs)
    {
        auto bounds { run->getRunBoundsX() };
        left  = std::min (left,  bounds.getStart());
        right = std::max (right, bounds.getEnd());
    }

    return { lineOrigin.x + left,
             lineOrigin.y - ascent,
             right - left,
             ascent + descent + leading };
}

// =============================================================================
// TextLayout — private helpers
// =============================================================================

Typeface::Style TextLayout::resolveStyle (const juce::Font& juceFont) noexcept
{
    const bool isBold   { juceFont.isBold() };
    const bool isItalic { juceFont.isItalic() };

    Typeface::Style result { Typeface::Style::regular };

    if (isBold and isItalic)
    {
        result = Typeface::Style::boldItalic;
    }
    else if (isBold)
    {
        result = Typeface::Style::bold;
    }
    else if (isItalic)
    {
        result = Typeface::Style::italic;
    }

    return result;
}

void TextLayout::recalculateSize() noexcept
{
    width  = 0.0f;
    height = 0.0f;

    for (const auto* line : lines)
    {
        auto bounds { line->getLineBounds() };
        width  = std::max (width,  bounds.getRight());
        height = std::max (height, bounds.getBottom());
    }
}

// =============================================================================
// TextLayout — public createLayout API
// =============================================================================

void TextLayout::createLayout (const juce::AttributedString& text,
                               jreng::Typeface& typeface,
                               float maxWidth) noexcept
{
    createLayoutInternal (text, typeface, maxWidth, unlimitedDimension);
}

void TextLayout::createLayout (const juce::AttributedString& text,
                               jreng::Typeface& typeface,
                               float maxWidth,
                               float maxHeight) noexcept
{
    createLayoutInternal (text, typeface, maxWidth, maxHeight);
}

// =============================================================================
// TextLayout — core layout algorithm
// =============================================================================

void TextLayout::createLayoutInternal (const juce::AttributedString& text,
                                       jreng::Typeface& typeface,
                                       float maxWidth,
                                       float maxHeight) noexcept
{
    layoutTypeface = &typeface;
    lines.clear();

    const juce::String& fullText { text.getText() };

    if (not fullText.isEmpty())
    {
        // -------------------------------------------------------------------------
        // Step 1: Convert to UTF-32 codepoints
        // -------------------------------------------------------------------------

        juce::Array<uint32_t> codepoints;
        codepoints.ensureStorageAllocated (fullText.length());

        juce::String::CharPointerType charPtr { fullText.getCharPointer() };
        while (not charPtr.isEmpty())
            codepoints.add (static_cast<uint32_t> (charPtr.getAndAdvance()));

        const int totalCodepoints { codepoints.size() };

        if (totalCodepoints > 0)
        {
            // -------------------------------------------------------------------------
            // Step 2: Query libunibreak for line-break opportunities
            // -------------------------------------------------------------------------

            juce::HeapBlock<char> breakOps (static_cast<size_t> (totalCodepoints));
            set_linebreaks_utf32 (reinterpret_cast<const utf32_t*> (codepoints.data()),
                                  static_cast<size_t> (totalCodepoints),
                                  nullptr,
                                  breakOps.getData());

            // -------------------------------------------------------------------------
            // Step 3: Shape all attribute runs and record per-glyph advance widths
            //         so the line-wrapper can operate on the shaped output.
            //
            // We accumulate:
            //   shapedGlyphs[i]   — Typeface::Glyph for codepoint i
            //   shapedAdvances[i] — advance width of that glyph in logical pixels
            //   attrIndex[i]      — which attribute owns codepoint i
            // -------------------------------------------------------------------------

            const int numAttributes { text.getNumAttributes() };

            // Per-codepoint data from shaping
            juce::Array<Typeface::Glyph> shapedGlyphs;
            juce::Array<float>       shapedAdvances;
            juce::Array<int>         attrIndexForCp;

            shapedGlyphs.resize (totalCodepoints);
            shapedAdvances.resize (totalCodepoints);
            attrIndexForCp.resize (totalCodepoints);

            // Zero-initialise
            for (int i { 0 }; i < totalCodepoints; ++i)
            {
                shapedGlyphs.getReference (i)   = {};
                shapedAdvances.getReference (i)  = 0.0f;
                attrIndexForCp.getReference (i)  = 0;
            }

            for (int attrIdx { 0 }; attrIdx < numAttributes; ++attrIdx)
            {
                const juce::AttributedString::Attribute& attr { text.getAttribute (attrIdx) };
                const juce::Range<int> range { attr.range };
                const Typeface::Style style { resolveStyle (attr.font) };

                const int rangeStart { juce::jmax (0, range.getStart()) };
                const int rangeEnd   { juce::jmin (totalCodepoints, range.getEnd()) };
                const int rangeLen   { rangeEnd - rangeStart };

                if (rangeLen > 0)
                {
                    Typeface::GlyphRun shaped { typeface.shapeText (style,
                                                               codepoints.data() + rangeStart,
                                                               static_cast<size_t> (rangeLen)) };

                    // Map shaped glyphs back to codepoint indices.  HarfBuzz produces one
                    // Glyph per input codepoint for most scripts (clusters may merge but
                    // we record the advance on the first codepoint of each cluster).
                    const int numShaped { shaped.count };
                    const int writeLen  { juce::jmin (numShaped, rangeLen) };

                    for (int i { 0 }; i < writeLen; ++i)
                    {
                        const int cpIdx { rangeStart + i };
                        shapedGlyphs.getReference (cpIdx)  = shaped.glyphs[i];
                        shapedAdvances.getReference (cpIdx) = shaped.glyphs[i].xAdvance;
                        attrIndexForCp.getReference (cpIdx) = attrIdx;
                    }
                }
            }

            // -------------------------------------------------------------------------
            // Step 4: Line-wrap using break opportunities and shaped advances
            // -------------------------------------------------------------------------

            // Use the first attribute's font height for global metrics.
            // Each line's ascent/descent will be set from the dominant attribute.
            const juce::Font& firstJuceFont { numAttributes > 0
                                              ? text.getAttribute (0).font
                                              : juce::Font (juce::FontOptions{}) };
            const float fontHeight   { firstJuceFont.getHeight() };
            Typeface::Metrics metrics    { typeface.calcMetrics (fontHeight) };
            const float lineAscent   { static_cast<float> (metrics.logicalBaseline) };
            const float lineDescent  { static_cast<float> (metrics.logicalCellH - metrics.logicalBaseline) };
            const float lineLeading  { 0.0f };
            const float lineAdvance  { lineAscent + lineDescent + lineLeading };

            // We build lines by walking codepoints and tracking the current line
            // start index and current x pen position.
            int   lineStart        { 0 };
            float lineY            { lineAscent };   // baseline Y of the current line
            float currentLineW     { 0.0f };
            int   lastBreakAt      { -1 };           // last valid break opportunity codepoint index
            float widthAtLastBreak { 0.0f };

            // Lambda: commit codepoints [lineStart, endExcl) as a new Line.
            // endExcl is exclusive; the line holds all runs from lineStart to endExcl.
            auto commitLine = [&] (int endExcl)
            {
                if (lineY <= maxHeight)
                {
                    auto* line { new Line() };
                    line->lineOrigin  = { 0.0f, lineY };
                    line->ascent      = lineAscent;
                    line->descent     = lineDescent;
                    line->leading     = lineLeading;
                    line->stringRange = { lineStart, endExcl };

                    // Build runs for this line
                    float penX    { 0.0f };
                    int   runStart { lineStart };

                    while (runStart < endExcl)
                    {
                        const int attrIdx { attrIndexForCp[runStart] };
                        const juce::AttributedString::Attribute& attr { text.getAttribute (attrIdx) };

                        // Find the extent of this attribute within [runStart, endExcl)
                        int runEnd { runStart };
                        while (runEnd < endExcl and attrIndexForCp[runEnd] == attrIdx)
                            ++runEnd;

                        auto* run { new Run() };
                        run->font        = attr.font;
                        run->colour      = attr.colour;
                        run->style       = resolveStyle (attr.font);
                        run->stringRange = { runStart, runEnd };

                        for (int ci { runStart }; ci < runEnd; ++ci)
                        {
                            Glyph g;
                            g.glyphCode = static_cast<int> (shapedGlyphs[ci].glyphIndex);
                            g.anchor    = { penX + shapedGlyphs[ci].xOffset,
                                            shapedGlyphs[ci].yOffset };
                            g.width     = shapedAdvances[ci];
                            run->glyphs.add (g);
                            penX += shapedAdvances[ci];
                        }

                        line->runs.add (run);
                        runStart = runEnd;
                    }

                    lines.add (line);
                }
            };

            for (int i { 0 }; i < totalCodepoints and lineY <= maxHeight; ++i)
            {
                const char breakOp  { breakOps[i] };
                const float advance { shapedAdvances[i] };

                if (breakOp == LINEBREAK_MUSTBREAK)
                {
                    // Mandatory break: commit immediately at this codepoint
                    commitLine (i + 1);
                    lineStart        = i + 1;
                    lineY           += lineAdvance;
                    currentLineW     = 0.0f;
                    lastBreakAt      = -1;
                    widthAtLastBreak = 0.0f;
                }
                else
                {
                    // Track the most recent allowed break position
                    if (breakOp == LINEBREAK_ALLOWBREAK)
                    {
                        lastBreakAt      = i;
                        widthAtLastBreak = currentLineW;
                    }

                    // Check if adding this glyph would exceed maxWidth
                    if (currentLineW + advance > maxWidth and currentLineW > 0.0f)
                    {
                        // Break at the last allowed break point if we have one,
                        // otherwise force-break before this glyph.
                        if (lastBreakAt >= lineStart)
                        {
                            commitLine (lastBreakAt + 1);
                            lineStart        = lastBreakAt + 1;
                            lineY           += lineAdvance;
                            currentLineW     = currentLineW - widthAtLastBreak;
                            lastBreakAt      = -1;
                            widthAtLastBreak = 0.0f;
                        }
                        else
                        {
                            // No break opportunity — force break before this glyph
                            commitLine (i);
                            lineStart        = i;
                            lineY           += lineAdvance;
                            currentLineW     = 0.0f;
                            lastBreakAt      = -1;
                            widthAtLastBreak = 0.0f;
                        }
                    }

                    currentLineW += advance;
                }
            }

            // Commit any remaining codepoints as the final line
            if (lineStart < totalCodepoints)
                commitLine (totalCodepoints);

            recalculateSize();
        }
    }
}

// =============================================================================
// TextLayout — static convenience
// =============================================================================

juce::Rectangle<float> TextLayout::getStringBounds (const juce::AttributedString& text,
                                                     jreng::Typeface& font)
{
    TextLayout layout;
    layout.createLayout (text, font, unlimitedDimension);
    return { 0.0f, 0.0f, layout.getWidth(), layout.getHeight() };
}

float TextLayout::getStringWidth (const juce::AttributedString& text,
                                  jreng::Typeface& font)
{
    return getStringBounds (text, font).getWidth();
}

} // namespace jreng
