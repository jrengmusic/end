/**
 * @file Cursor.h
 * @brief Shared cursor descriptor for all pane types.
 *
 * Describes what the cursor looks like and where it goes.
 * Both Terminal and Whelmed build a Cursor from their own coordinate
 * systems and draw it through their respective renderers.
 *
 * @see lua::Engine::display.cursor
 * @see lua::Engine::display.colours.selectionCursor
 */

#pragma once

#include <JuceHeader.h>

/**
 * @struct Cursor
 * @brief Describes a cursor to draw: position, size, colour, shape, codepoint.
 *
 * A pure data descriptor.  No state, no history, no behaviour.
 * Built per-frame by the owning screen, consumed by the renderer.
 *
 * Also serves as the SSOT for cursor position computation via its static
 * functions.  Callers query their own data structures and pass raw values;
 * Cursor computes and returns a Position without touching any screen state.
 */
struct Cursor
{
    juce::Rectangle<float> bounds;   ///< Pixel rect — position and size of the cursor cell/glyph.
    juce::Colour colour;             ///< Draw colour.
    uint32_t codepoint { 0x2588 };   ///< Unicode codepoint to draw (default: full block U+2588).
    int shape { 0 };                 ///< DECSCUSR shape: 0 = user glyph, 1/2 = block, 3/4 = underline, 5/6 = bar.
    bool visible { false };          ///< Whether the cursor should be drawn at all.
    bool blinkOn { true };           ///< Current blink phase (true = visible).

    // =========================================================================
    // Position computation — stateless, no Screen/Block dependency.
    // =========================================================================

    /** Result of a cursor position computation. */
    struct Position
    {
        int block { 0 };
        int character { 0 };
    };

    /** Moves cursor left one character, crossing block boundary if at start.
     *  @param cursorBlock     Current block index.
     *  @param cursorChar      Current char index within block.
     *  @param prevBlockLength Text length of the previous block (blockIndex - 1). 0 if no previous block.
     *  @return New position. */
    static Position moveLeft (int cursorBlock, int cursorChar,
                              int prevBlockLength) noexcept;

    /** Moves cursor right one character, crossing block boundary if at end.
     *  @param cursorBlock     Current block index.
     *  @param cursorChar      Current char index within block.
     *  @param blockLength     Text length of the current block.
     *  @param maxBlock        Maximum valid block index.
     *  @return New position. */
    static Position moveRight (int cursorBlock, int cursorChar,
                               int blockLength, int maxBlock) noexcept;

    /** Moves cursor up one visual line, preserving column via targetX.
     *  @param cursorBlock               Current block index.
     *  @param currentLine               Current visual line within block.
     *  @param charForPrevLine           Char index at targetX on the previous line (caller computed).
     *  @param prevBlock                 Block index of previous block (cursorBlock - 1).
     *  @param charForPrevBlockLastLine  Char index at targetX on last line of previous block. Used when currentLine == 0.
     *  @return New position. */
    static Position moveUp (int cursorBlock, int currentLine,
                            int charForPrevLine,
                            int prevBlock, int charForPrevBlockLastLine) noexcept;

    /** Moves cursor down one visual line, preserving column via targetX.
     *  @param cursorBlock                   Current block index.
     *  @param currentLine                   Current visual line within block.
     *  @param lineCount                     Total visual lines in current block.
     *  @param charForNextLine               Char index at targetX on the next line (caller computed).
     *  @param nextBlock                     Block index of next block (cursorBlock + 1).
     *  @param charForNextBlockFirstLine     Char index at targetX on first line of next block.
     *  @param maxBlock                      Maximum valid block index.
     *  @return New position. */
    static Position moveDown (int cursorBlock, int currentLine, int lineCount,
                              int charForNextLine,
                              int nextBlock, int charForNextBlockFirstLine,
                              int maxBlock) noexcept;

    /** Moves cursor to the start of the current visual line.
     *  @param cursorBlock    Current block index (returned unchanged).
     *  @param lineStartChar  First char index of the current visual line.
     *  @return New position. */
    static Position moveToLineStart (int cursorBlock, int lineStartChar) noexcept;

    /** Moves cursor to the end of the current visual line.
     *  @param cursorBlock   Current block index (returned unchanged).
     *  @param lineEndChar   Last char index of the current visual line.
     *  @return New position. */
    static Position moveToLineEnd (int cursorBlock, int lineEndChar) noexcept;

    /** Moves cursor to top of document.
     *  @return Position {0, 0}. */
    static Position moveToTop() noexcept;

    /** Moves cursor to bottom of document.
     *  @param maxBlock         Last valid block index.
     *  @param lastBlockLength  Text length of the last block.
     *  @return New position. */
    static Position moveToBottom (int maxBlock, int lastBlockLength) noexcept;
};
