#pragma once
#include <JuceHeader.h>
#include "Block.h"
#include "TextBlock.h"
#include "MermaidBlock.h"
#include "TableBlock.h"
#include "MermaidSVGParser.h"
#include "../Cursor.h"

namespace Whelmed
{ /*____________________________________________________________________________*/

class Screen : public juce::Component,
               private juce::Timer
{
public:
    Screen();

    /** Loads a document. Builds blocks until viewportHeight is filled, returns batch count. */
    int load (const jreng::Markdown::ParsedDocument& doc, int viewportHeight);

    /** Builds a single block by index. Idempotent — skips if already built. */
    void build (int blockIndex, const jreng::Markdown::ParsedDocument& doc);

    /** Recomputes layout for all entries. */
    void updateLayout();

    void updateMermaidBlock (int blockIndex, MermaidParseResult&& result);
    void clear();

    int getBlockCount() const noexcept;
    int getBuiltCount() const noexcept;
    juce::Array<int> getMermaidBlockIndices() const;

    void paint (juce::Graphics& g) override;
    void resized() override;

    /** Sets the current selection coordinates for rendering. Called from Component. */
    void setSelection (int anchorBlock, int anchorChar, int cursorBlock, int cursorChar) noexcept;

    /** Sets the cursor descriptor for selection mode rendering. */
    void setCursor (const Cursor& c) noexcept;

    /** Builds and stores the cursor for the current selection cursor position. */
    void updateCursor (int blockIndex, int charIndex) noexcept;

    /** Extracts plain text from a block/character range for clipboard copy. */
    juce::String extractText (int startBlock, int startChar, int endBlock, int endChar) const;

    int getBlockTextLength (int blockIndex) const noexcept;
    int getBlockLineCount (int blockIndex) const noexcept;
    int getBlockLineForChar (int blockIndex, int charIndex) const noexcept;
    juce::Range<int> getBlockLineCharRange (int blockIndex, int lineIndex) const noexcept;
    int getBlockCharForLine (int blockIndex, int lineIndex, float targetX) const noexcept;
    float getBlockCharX (int blockIndex, int charIndex) const noexcept;

    /** Returns the pixel Y of the cursor for viewport scrolling. Returns -1 if invalid. */
    float getCursorPixelY() const noexcept;

    /** Returns the cursor bounds for viewport auto-scroll. */
    juce::Rectangle<float> getCursorBounds() const noexcept;

    void hideCursor() noexcept;

    /** Returns the block index of the first block visible at the given scroll offset. */
    int getFirstVisibleBlock (int viewportY) const noexcept;

    struct HitResult
    {
        int block { -1 };
        int character { -1 };
    };

    /** Converts a pixel position (in Screen coordinates) to block/char. */
    HitResult hitTestAt (float x, float y) const noexcept;

    void setStateTree (juce::ValueTree tree) noexcept;

    void mouseDown (const juce::MouseEvent& event) override;
    void mouseDrag (const juce::MouseEvent& event) override;
    void mouseUp   (const juce::MouseEvent& event) override;

private:
    void timerCallback() override;
    bool hasPendingMermaids() const noexcept;
    void paintMermaidSpinner (juce::Graphics& g, juce::Rectangle<int> area) const;

    struct BlockEntry
    {
        std::unique_ptr<Block> block;
        jreng::Markdown::BlockType type;
        int y { 0 };
        int height { 0 };
    };

    std::vector<BlockEntry> entries;
    int builtCount { 0 };
    int spinnerFrame { 0 };
    int contentHeight { 0 };
    int layoutWidth { 0 };
    float bodyFontSize { 16.0f };
    float lineHeight { 1.5f };

    int selAnchorBlock { 0 };
    int selAnchorChar  { 0 };
    int selCursorBlock { 0 };
    int selCursorChar  { 0 };

    Cursor cursor;

    juce::ValueTree stateTree;
    HitResult dragAnchor { -1, -1 };
    bool dragActive { false };
    int clickCount { 0 };

    juce::AttributedString buildAttributedString (const jreng::Markdown::ParsedDocument& doc, int blockIndex) const;

    std::unique_ptr<Block> createBlock (const jreng::Markdown::ParsedDocument& doc, int blockIndex);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Screen)
};

/**_____________________________END OF NAMESPACE______________________________*/
} // namespace Whelmed
