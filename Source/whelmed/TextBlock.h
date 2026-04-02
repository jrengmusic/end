/**
 * @file TextBlock.h
 * @brief Block subtype for flowing styled text — headings, paragraphs, list items.
 */

#pragma once
#include <JuceHeader.h>
#include "Block.h"

namespace Whelmed
{ /*____________________________________________________________________________*/

class TextBlock : public Block
{
public:
    explicit TextBlock (juce::AttributedString attributedText,
                        juce::Colour background = juce::Colours::transparentBlack);

    int getPreferredHeight (int width) const noexcept override;
    void paint (juce::Graphics& g, juce::Rectangle<int> area) const override;

    /** Re-creates the internal layout for a new width. Call when viewport width changes. */
    void layout (int width);

    juce::RectangleList<float> getSelectionRects (int startChar, int endChar) const override;
    juce::String getText() const override;
    int getTextLength() const noexcept override;
    juce::Rectangle<float> getGlyphBounds (int charIndex) const override;

    /** Returns the number of visual lines in this block's layout. */
    int getLineCount() const noexcept override;

    /** Returns which visual line a character index falls on. */
    int getLineForChar (int charIndex) const noexcept override;

    /** Returns the start and end character indices for a visual line (half-open range). */
    juce::Range<int> getLineCharRange (int lineIndex) const noexcept override;

    /** Returns the character index closest to targetX on the given visual line. */
    int getCharForLine (int lineIndex, float targetX) const noexcept override;

    /** Returns the x-position of a character (for sticky column tracking). */
    float getCharX (int charIndex) const noexcept override;

    int hitTest (float localX, float localY) const noexcept override;

private:
    juce::AttributedString source;
    juce::TextLayout textLayout;
    juce::Colour backgroundColour;
    int cachedHeight { 0 };
    int cachedWidth { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TextBlock)
};

/**_____________________________END OF NAMESPACE______________________________*/
} // namespace Whelmed
