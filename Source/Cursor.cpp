/**
 * @file Cursor.cpp
 * @brief Shared cursor position computation.
 *
 * @see Cursor.h
 */

#include "Cursor.h"

Cursor::Position Cursor::moveLeft (int cursorBlock, int cursorChar,
                                   int prevBlockLength) noexcept
{
    Position result { cursorBlock, cursorChar };

    if (cursorChar > 0)
    {
        result.character = cursorChar - 1;
    }
    else if (cursorBlock > 0)
    {
        result.block = cursorBlock - 1;
        result.character = juce::jmax (0, prevBlockLength - 1);
    }

    return result;
}

Cursor::Position Cursor::moveRight (int cursorBlock, int cursorChar,
                                    int blockLength, int maxBlock) noexcept
{
    Position result { cursorBlock, cursorChar };

    if (cursorChar < blockLength - 1)
    {
        result.character = cursorChar + 1;
    }
    else if (cursorBlock < maxBlock)
    {
        result.block = cursorBlock + 1;
        result.character = 0;
    }

    return result;
}

Cursor::Position Cursor::moveUp (int cursorBlock, int currentLine,
                                 int charForPrevLine,
                                 int prevBlock, int charForPrevBlockLastLine) noexcept
{
    Position result { cursorBlock, 0 };

    if (currentLine > 0)
    {
        result.character = charForPrevLine;
    }
    else if (cursorBlock > 0)
    {
        result.block = prevBlock;
        result.character = charForPrevBlockLastLine;
    }

    return result;
}

Cursor::Position Cursor::moveDown (int cursorBlock, int currentLine, int lineCount,
                                   int charForNextLine,
                                   int nextBlock, int charForNextBlockFirstLine,
                                   int maxBlock) noexcept
{
    Position result { cursorBlock, 0 };

    if (currentLine < lineCount - 1)
    {
        result.character = charForNextLine;
    }
    else if (cursorBlock < maxBlock)
    {
        result.block = nextBlock;
        result.character = charForNextBlockFirstLine;
    }

    return result;
}

Cursor::Position Cursor::moveToLineStart (int cursorBlock, int lineStartChar) noexcept
{
    return { cursorBlock, lineStartChar };
}

Cursor::Position Cursor::moveToLineEnd (int cursorBlock, int lineEndChar) noexcept
{
    return { cursorBlock, lineEndChar };
}

Cursor::Position Cursor::moveToTop() noexcept
{
    return { 0, 0 };
}

Cursor::Position Cursor::moveToBottom (int maxBlock, int lastBlockLength) noexcept
{
    return { maxBlock, juce::jmax (0, lastBlockLength - 1) };
}
