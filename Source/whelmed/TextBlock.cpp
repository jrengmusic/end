#include "TextBlock.h"

namespace Whelmed
{ /*____________________________________________________________________________*/

TextBlock::TextBlock (juce::AttributedString attributedText,
                      juce::Colour background)
    : source (std::move (attributedText))
    , backgroundColour (background)
{
}

int TextBlock::getPreferredHeight (int /*width*/) const noexcept
{
    return cachedHeight;
}

void TextBlock::paint (juce::Graphics& g, juce::Rectangle<int> area) const
{
    if (backgroundColour != juce::Colours::transparentBlack)
    {
        g.setColour (backgroundColour);
        g.fillRect (area);
    }

    textLayout.draw (g, area.toFloat());
}

void TextBlock::layout (int width)
{
    if (width != cachedWidth and width > 0)
    {
        textLayout = juce::TextLayout();
        textLayout.createLayout (source, static_cast<float> (width));
        cachedHeight = juce::jmax (1, static_cast<int> (std::ceil (textLayout.getHeight())));
        cachedWidth = width;
    }
}

juce::String TextBlock::getText() const
{
    return source.getText();
}

int TextBlock::getTextLength() const noexcept
{
    return source.getText().length();
}

juce::Rectangle<float> TextBlock::getGlyphBounds (int charIndex) const
{
    int currentChar { 0 };

    for (int lineIdx { 0 }; lineIdx < textLayout.getNumLines(); ++lineIdx)
    {
        const auto& line { textLayout.getLine (lineIdx) };
        const float lineTop    { line.lineOrigin.y - line.ascent };
        const float lineHeight { line.ascent + line.descent };

        for (int runIdx { 0 }; runIdx < line.runs.size(); ++runIdx)
        {
            const auto* run { line.runs.getUnchecked (runIdx) };

            for (int glyphIdx { 0 }; glyphIdx < run->glyphs.size(); ++glyphIdx)
            {
                if (currentChar == charIndex)
                {
                    const float glyphX { run->glyphs.getReference (glyphIdx).anchor.x };
                    const float glyphW { run->glyphs.getReference (glyphIdx).width };
                    return { glyphX, lineTop, glyphW, lineHeight };
                }

                ++currentChar;
            }
        }
    }

    return {};
}

int TextBlock::getLineCount() const noexcept
{
    const int count { textLayout.getNumLines() };
    return juce::jmax (1, count);
}

int TextBlock::getLineForChar (int charIndex) const noexcept
{
    int currentChar { 0 };

    for (int lineIdx { 0 }; lineIdx < textLayout.getNumLines(); ++lineIdx)
    {
        const auto& line { textLayout.getLine (lineIdx) };
        int lineChars { 0 };

        for (int runIdx { 0 }; runIdx < line.runs.size(); ++runIdx)
            lineChars += line.runs.getUnchecked (runIdx)->glyphs.size();

        if (charIndex < currentChar + lineChars)
            return lineIdx;

        currentChar += lineChars;
    }

    return juce::jmax (0, textLayout.getNumLines() - 1);
}

juce::Range<int> TextBlock::getLineCharRange (int lineIndex) const noexcept
{
    int currentChar { 0 };

    for (int lineIdx { 0 }; lineIdx < textLayout.getNumLines(); ++lineIdx)
    {
        const auto& line { textLayout.getLine (lineIdx) };
        int lineChars { 0 };

        for (int runIdx { 0 }; runIdx < line.runs.size(); ++runIdx)
            lineChars += line.runs.getUnchecked (runIdx)->glyphs.size();

        if (lineIdx == lineIndex)
            return { currentChar, currentChar + lineChars };

        currentChar += lineChars;
    }

    return { 0, 0 };
}

int TextBlock::getCharForLine (int lineIndex, float targetX) const noexcept
{
    int currentChar { 0 };

    for (int lineIdx { 0 }; lineIdx < textLayout.getNumLines(); ++lineIdx)
    {
        const auto& line { textLayout.getLine (lineIdx) };

        if (lineIdx == lineIndex)
        {
            int bestChar { currentChar };
            float bestDist { std::numeric_limits<float>::max() };

            for (int runIdx { 0 }; runIdx < line.runs.size(); ++runIdx)
            {
                const auto* run { line.runs.getUnchecked (runIdx) };

                for (int glyphIdx { 0 }; glyphIdx < run->glyphs.size(); ++glyphIdx)
                {
                    const float glyphX { run->glyphs.getReference (glyphIdx).anchor.x };
                    const float dist { std::abs (glyphX - targetX) };

                    if (dist < bestDist)
                    {
                        bestDist = dist;
                        bestChar = currentChar;
                    }

                    ++currentChar;
                }
            }

            return bestChar;
        }

        for (int runIdx { 0 }; runIdx < line.runs.size(); ++runIdx)
            currentChar += line.runs.getUnchecked (runIdx)->glyphs.size();
    }

    return 0;
}

float TextBlock::getCharX (int charIndex) const noexcept
{
    int currentChar { 0 };

    for (int lineIdx { 0 }; lineIdx < textLayout.getNumLines(); ++lineIdx)
    {
        const auto& line { textLayout.getLine (lineIdx) };

        for (int runIdx { 0 }; runIdx < line.runs.size(); ++runIdx)
        {
            const auto* run { line.runs.getUnchecked (runIdx) };

            for (int glyphIdx { 0 }; glyphIdx < run->glyphs.size(); ++glyphIdx)
            {
                if (currentChar == charIndex)
                    return run->glyphs.getReference (glyphIdx).anchor.x;

                ++currentChar;
            }
        }
    }

    return 0.0f;
}

int TextBlock::hitTest (float localX, float localY) const noexcept
{
    int currentChar { 0 };

    for (int lineIdx { 0 }; lineIdx < textLayout.getNumLines(); ++lineIdx)
    {
        const auto& line { textLayout.getLine (lineIdx) };
        const float lineTop    { line.lineOrigin.y - line.ascent };
        const float lineBottom { line.lineOrigin.y + line.descent };

        if (localY >= lineTop and localY < lineBottom)
        {
            int bestChar { currentChar };
            float bestDist { std::numeric_limits<float>::max() };

            for (int runIdx { 0 }; runIdx < line.runs.size(); ++runIdx)
            {
                const auto* run { line.runs.getUnchecked (runIdx) };

                for (int glyphIdx { 0 }; glyphIdx < run->glyphs.size(); ++glyphIdx)
                {
                    const float glyphX { run->glyphs.getReference (glyphIdx).anchor.x };
                    const float dist { std::abs (glyphX - localX) };

                    if (dist < bestDist)
                    {
                        bestDist = dist;
                        bestChar = currentChar;
                    }

                    ++currentChar;
                }
            }

            return bestChar;
        }

        // Skip this line's chars
        for (int runIdx { 0 }; runIdx < line.runs.size(); ++runIdx)
            currentChar += line.runs.getUnchecked (runIdx)->glyphs.size();
    }

    // Below all lines — return last char
    return juce::jmax (0, currentChar - 1);
}

juce::RectangleList<float> TextBlock::getSelectionRects (int startChar, int endChar) const
{
    juce::RectangleList<float> rects;
    const int textLen { source.getText().length() };
    const int clampedStart { juce::jlimit (0, textLen, startChar) };
    const int clampedEnd { juce::jlimit (0, textLen, endChar) };

    if (clampedStart < clampedEnd)
    {
        int charIndex { 0 };

        for (int lineIdx { 0 }; lineIdx < textLayout.getNumLines(); ++lineIdx)
        {
            const auto& line { textLayout.getLine (lineIdx) };
            const float lineTop    { line.lineOrigin.y - line.ascent };
            const float lineBottom { line.lineOrigin.y + line.descent };
            const float lineHeight { lineBottom - lineTop };

            float lineStartX { -1.0f };
            float lineEndX   { -1.0f };

            for (int runIdx { 0 }; runIdx < line.runs.size(); ++runIdx)
            {
                const auto* run { line.runs.getUnchecked (runIdx) };

                for (int glyphIdx { 0 }; glyphIdx < run->glyphs.size(); ++glyphIdx)
                {
                    if (charIndex >= clampedStart and charIndex < clampedEnd)
                    {
                        const float glyphX { run->glyphs.getReference (glyphIdx).anchor.x };
                        const float glyphW { run->glyphs.getReference (glyphIdx).width };

                        if (lineStartX < 0.0f)
                            lineStartX = glyphX;

                        lineEndX = glyphX + glyphW;
                    }

                    ++charIndex;
                }
            }

            if (lineStartX >= 0.0f and lineEndX > lineStartX)
            {
                rects.addWithoutMerging (juce::Rectangle<float> (lineStartX, lineTop, lineEndX - lineStartX, lineHeight));
            }
        }
    }

    return rects;
}

/**_____________________________END OF NAMESPACE______________________________*/
} // namespace Whelmed
