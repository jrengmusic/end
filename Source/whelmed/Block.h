/**
 * @file Block.h
 * @brief Pure virtual base for renderable block types in the Whelmed document stack.
 */

#pragma once
#include <JuceHeader.h>

namespace Whelmed
{ /*____________________________________________________________________________*/

/**
    Base class for all renderable block types in the Whelmed document stack.

    Blocks are NOT juce::Components. They are data objects that know how to
    measure their height and paint themselves into a Graphics context.
    Whelmed::Screen owns blocks and renders them in a single unified paint() call.
*/
class Block
{
public:
    Block() = default;
    virtual ~Block() = default;

    /** Returns the preferred height for a given width. */
    virtual int getPreferredHeight (int width) const noexcept = 0;

    /** Paints this block into the given area. */
    virtual void paint (juce::Graphics& g, juce::Rectangle<int> area) const = 0;

    /** Returns selection highlight rectangles for a character range within this block.
     *  Coordinates are relative to the block's own origin (0,0). Default: empty. */
    virtual juce::RectangleList<float> getSelectionRects (int startChar, int endChar) const
    {
        juce::ignoreUnused (startChar, endChar);
        return {};
    }

    /** Returns the plain text content. Default: empty string. */
    virtual juce::String getText() const { return {}; }

    /** Returns the number of characters. Default: 0. */
    virtual int getTextLength() const noexcept { return 0; }

    /** Returns the bounding rect for a single character at the given index.
     *  Returns empty rect if index is out of range. Coordinates relative to block origin. */
    virtual juce::Rectangle<float> getGlyphBounds (int charIndex) const
    {
        juce::ignoreUnused (charIndex);
        return {};
    }

    virtual int getLineCount() const noexcept { return 1; }
    virtual int getLineForChar (int charIndex) const noexcept { juce::ignoreUnused (charIndex); return 0; }
    virtual juce::Range<int> getLineCharRange (int lineIndex) const noexcept { juce::ignoreUnused (lineIndex); return { 0, 0 }; }
    virtual int getCharForLine (int lineIndex, float targetX) const noexcept { juce::ignoreUnused (lineIndex, targetX); return 0; }
    virtual float getCharX (int charIndex) const noexcept { juce::ignoreUnused (charIndex); return 0.0f; }

    /** Converts a local pixel position to a character index. Returns -1 if out of range. */
    virtual int hitTest (float localX, float localY) const noexcept
    {
        juce::ignoreUnused (localX, localY);
        return -1;
    }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Block)
};

/**_____________________________END OF NAMESPACE______________________________*/
} // namespace Whelmed
