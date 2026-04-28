/**
 * @file GridAccess.cpp
 * @brief Grid member functions for row access, scrollback, text extraction,
 *        decoded image storage, and serialization.
 *
 * This translation unit implements the read-side and accessor surface of the
 * Grid class.  Functions here are consumed primarily by the MESSAGE THREAD
 * (renderer, selection, text extraction) and by the READER THREAD for direct
 * pointer access on bulk write paths.
 *
 * @see Grid.h for the full class documentation.
 */

#include "Grid.h"

namespace Terminal
{ /*____________________________________________________________________________*/

// ============================================================================
// Row Access
// ============================================================================

/**
 * @brief Returns a raw pointer to the start of visible row `row` for bulk writes.
 *
 * Inlines the ring-buffer index calculation without bounds checking.  The
 * caller must stay within [0, cols) and call `markRowDirty (row)` after
 * writing.
 *
 * @param visibleRow  Zero-based visible row index.
 * @return Pointer to the first Cell in the row.
 * @note READER THREAD — lock-free, noexcept.
 *       Caller must markRowDirty after writing.
 */
Cell* Grid::directRowPtr (int visibleRow) noexcept
{
    Buffer& buffer { bufferForScreen() };
    return buffer.cells.get() + ((buffer.head - getVisibleRows() + 1 + visibleRow) & buffer.rowMask) * getCols();
}

/**
 * @brief Returns a raw pointer to the start of the grapheme row for `visibleRow`.
 *
 * Bypasses bounds checking.  The caller must stay within [0, cols) and must
 * call `markRowDirty (visibleRow)` after writing.
 *
 * @param visibleRow  Zero-based visible row index.
 * @return Pointer to the first Grapheme in the row.
 * @note READER THREAD — lock-free, noexcept.
 */
Grapheme* Grid::directGraphemeRowPtr (int visibleRow) noexcept
{
    Buffer& buffer { bufferForScreen() };
    return buffer.graphemes.get() + ((buffer.head - getVisibleRows() + 1 + visibleRow) & buffer.rowMask) * getCols();
}

/**
 * @brief Returns a raw pointer to the start of the linkId row for `visibleRow`.
 *
 * Bypasses bounds checking.  The caller must stay within [0, cols) and must
 * call `markRowDirty (visibleRow)` after writing.
 *
 * @param visibleRow  Zero-based visible row index.
 * @return Pointer to the first uint16_t in the linkIds row.
 * @note READER THREAD — lock-free, noexcept.
 */
uint16_t* Grid::directLinkIdRowPtr (int visibleRow) noexcept
{
    Buffer& buffer { bufferForScreen() };
    return buffer.linkIds.get() + ((buffer.head - getVisibleRows() + 1 + visibleRow) & buffer.rowMask) * getCols();
}

/**
 * @brief Returns a mutable pointer to the start of visible row `row`.
 *
 * Bounds-checks `row` against [0, visibleRows) before computing the physical
 * index.  Returns `nullptr` if out of range.
 *
 * @param row  Zero-based visible row index.
 * @return Pointer to the first Cell in the row, or `nullptr` if out of range.
 * @note READER THREAD — lock-free, noexcept.
 */
Cell* Grid::activeVisibleRow (int row) noexcept
{
    if (row >= 0 and row < getVisibleRows())
    {
        Buffer& buffer { bufferForScreen() };
        return rowPtr (buffer, row);
    }
    return nullptr;
}

/**
 * @brief Returns a const pointer to the start of visible row `row`.
 *
 * @param row  Zero-based visible row index.
 * @return Const pointer to the first Cell in the row, or `nullptr` if out of range.
 * @note MESSAGE THREAD — lock-free, noexcept.
 */
const Cell* Grid::activeVisibleRow (int row) const noexcept
{
    if (row >= 0 and row < getVisibleRows())
    {
        const Buffer& buffer { bufferForScreen() };
        return rowPtr (buffer, row);
    }
    return nullptr;
}

/**
 * @brief Returns a const pointer to the grapheme row for visible row `row`.
 *
 * The returned pointer addresses `cols` Grapheme entries co-indexed with the
 * cell row.  Returns `nullptr` if `row` is out of range.
 *
 * @param row  Zero-based visible row index.
 * @return Pointer to the first Grapheme in the row, or `nullptr` if out of range.
 * @note MESSAGE THREAD — lock-free, noexcept.
 */
const Grapheme* Grid::activeVisibleGraphemeRow (int row) const noexcept
{
    if (row >= 0 and row < getVisibleRows())
    {
        const Buffer& buffer { bufferForScreen() };
        return buffer.graphemes.get() + physicalRow (buffer, row) * getCols();
    }
    return nullptr;
}

/**
 * @brief Returns the number of scrollback rows currently stored.
 *
 * Reads `scrollbackUsed` from the active buffer.  Only meaningful for the
 * normal screen; always 0 for the alternate screen.
 *
 * @return Number of scrollback rows available (0 … scrollbackCapacity).
 * @note MESSAGE THREAD — lock-free, noexcept.
 */
int Grid::getScrollbackUsed() const noexcept
{
    const Buffer& buffer { bufferForScreen() };
    return buffer.scrollbackUsed;
}

/**
 * @brief Returns a const pointer to a scrolled-back cell row.
 *
 * Applies `scrollOffset` to the ring-buffer index:
 * @code
 * phys = (head - visibleRows + 1 + visibleRow - scrollOffset) & rowMask
 * @endcode
 * A `scrollOffset` of 1 returns the row one line above the live view.
 * Returns `nullptr` if `visibleRow` is out of range.
 *
 * @param visibleRow   Zero-based visible row index (0 … visibleRows-1).
 * @param scrollOffset Lines scrolled back (0 = live view).
 * @return Const pointer to the first Cell in the scrolled row, or `nullptr`.
 * @note MESSAGE THREAD — lock-free, noexcept.
 */
const Cell* Grid::scrollbackRow (int visibleRow, int scrollOffset) const noexcept
{
    if (visibleRow >= 0 and visibleRow < getVisibleRows())
    {
        const Buffer& buffer { bufferForScreen() };
        const int phys { (buffer.head - getVisibleRows() + 1 + visibleRow - scrollOffset) & buffer.rowMask };
        return buffer.cells.get() + phys * getCols();
    }
    return nullptr;
}

/**
 * @brief Returns a const pointer to a scrolled-back grapheme row.
 *
 * Parallel to `scrollbackRow()` but for the grapheme sidecar array.
 * Returns `nullptr` if `visibleRow` is out of range.
 *
 * @param visibleRow   Zero-based visible row index (0 … visibleRows-1).
 * @param scrollOffset Lines scrolled back (0 = live view).
 * @return Const pointer to the first Grapheme in the scrolled row, or `nullptr`.
 * @note MESSAGE THREAD — lock-free, noexcept.
 */
const Grapheme* Grid::scrollbackGraphemeRow (int visibleRow, int scrollOffset) const noexcept
{
    if (visibleRow >= 0 and visibleRow < getVisibleRows())
    {
        const Buffer& buffer { bufferForScreen() };
        const int phys { (buffer.head - getVisibleRows() + 1 + visibleRow - scrollOffset) & buffer.rowMask };
        return buffer.graphemes.get() + phys * getCols();
    }
    return nullptr;
}

/**
 * @brief Returns a const pointer to the hyperlink ID row for visible row `row`.
 *
 * The returned pointer addresses `cols` uint16_t entries co-indexed with
 * the cell row.  Returns `nullptr` if `row` is out of range.
 *
 * @param row  Zero-based visible row index.
 * @return Pointer to the first uint16_t in the row, or `nullptr` if out of range.
 * @note MESSAGE THREAD — lock-free, noexcept.
 */
const uint16_t* Grid::activeVisibleLinkIdRow (int row) const noexcept
{
    if (row >= 0 and row < getVisibleRows())
    {
        const Buffer& buffer { bufferForScreen() };
        return buffer.linkIds.get() + physicalRow (buffer, row) * getCols();
    }
    return nullptr;
}

/**
 * @brief Returns a const pointer to a scrolled-back hyperlink ID row.
 *
 * Parallel to `scrollbackRow()` but for the linkIds sidecar array.
 * Returns `nullptr` if `visibleRow` is out of range.
 *
 * @param visibleRow   Zero-based visible row index (0 … visibleRows-1).
 * @param scrollOffset Lines scrolled back (0 = live view).
 * @return Const pointer to the first uint16_t in the scrolled row, or `nullptr`.
 * @note MESSAGE THREAD — lock-free, noexcept.
 */
const uint16_t* Grid::scrollbackLinkIdRow (int visibleRow, int scrollOffset) const noexcept
{
    if (visibleRow >= 0 and visibleRow < getVisibleRows())
    {
        const Buffer& buffer { bufferForScreen() };
        const int phys { (buffer.head - getVisibleRows() + 1 + visibleRow - scrollOffset) & buffer.rowMask };
        return buffer.linkIds.get() + phys * getCols();
    }
    return nullptr;
}

/**
 * @brief Returns a mutable reference to the RowState for visible row `row`.
 *
 * @param row  Zero-based visible row index.
 * @return Reference to the RowState entry for the physical row.
 * @note READER THREAD — lock-free, noexcept.
 */
RowState& Grid::activeVisibleRowState (int row) noexcept
{
    Buffer& buffer { bufferForScreen() };
    const int phys { physicalRow (buffer, row) };
    return buffer.rowStates[phys];
}

/**
 * @brief Returns a const reference to the RowState for visible row `row`.
 *
 * @param row  Zero-based visible row index.
 * @return Const reference to the RowState entry for the physical row.
 * @note MESSAGE THREAD — lock-free, noexcept.
 */
const RowState& Grid::activeVisibleRowState (int row) const noexcept
{
    const Buffer& buffer { bufferForScreen() };
    const int phys { physicalRow (buffer, row) };
    return buffer.rowStates[phys];
}

// ============================================================================
// Decoded Image Storage
// ============================================================================

/**
 * @brief Reserves the next image ID via atomic increment.
 *
 * @return A unique image ID in [1, UINT32_MAX].
 * @note READER THREAD — lock-free, noexcept.
 */
uint32_t Grid::reserveImageId() noexcept
{
    return nextImageId.fetch_add (1u, std::memory_order_relaxed);
}

/**
 * @brief Stores decoded image data for later atlas consumption.
 *
 * @param img  PendingImage to store; moved into the internal map.
 * @note READER THREAD — called after image decode completes.
 */
void Grid::storeDecodedImage (PendingImage&& img) noexcept
{
    decodedImages[img.imageId] = std::move (img);
}

/**
 * @brief Retrieves stored decoded image data without removing it.
 *
 * @param imageId  Image ID to look up.
 * @return Pointer to PendingImage, or nullptr if not found.
 * @note MESSAGE THREAD — called by renderer on first atlas encounter.
 */
PendingImage* Grid::getDecodedImage (uint32_t imageId) noexcept
{
    auto it { decodedImages.find (imageId) };

    if (it != decodedImages.end())
    {
        return &it->second;
    }

    return nullptr;
}

/**
 * @brief Removes decoded image data after atlas has consumed it.
 *
 * @param imageId  Image ID to remove.
 * @note MESSAGE THREAD.
 */
void Grid::releaseDecodedImage (uint32_t imageId) noexcept
{
    decodedImages.erase (imageId);
}

// =============================================================================

/**
 * @brief Append a single cell's text contribution to a row string.
 *
 * Skips wide-continuation cells entirely.  Appends a space for empty cells
 * (`codepoint == 0`).  For all other cells, appends the base codepoint
 * followed by any extra codepoints from the grapheme sidecar.
 *
 * @param cell      Cell to convert.
 * @param grapheme  Grapheme sidecar entry co-indexed with the cell (may be nullptr).
 * @param rowText   String to append to.
 */
static void appendCellText (const Cell& cell, const Grapheme* grapheme, juce::String& rowText)
{
    if (not cell.isWideContinuation())
    {
        if (cell.codepoint == 0)
        {
            rowText += " ";
        }
        else
        {
            rowText += juce::String::charToString (static_cast<juce::juce_wchar> (cell.codepoint));

            if (cell.hasGrapheme() and grapheme != nullptr)
            {
                const uint8_t safeCount { std::min (grapheme->count,
                    static_cast<uint8_t> (grapheme->extraCodepoints.size())) };

                for (uint8_t g { 0 }; g < safeCount; ++g)
                {
                    rowText += juce::String::charToString (
                        static_cast<juce::juce_wchar> (grapheme->extraCodepoints.at (g)));
                }
            }
        }
    }
}

// =============================================================================

/**
 * @brief Extracts a rectangular region of text as a UTF-32 string.
 *
 * Normalises the selection so that `start` is always before `end`, clamps
 * both endpoints to the visible area, then iterates rows collecting
 * codepoints:
 *
 * - Empty cells (`codepoint == 0`) are emitted as spaces.
 * - Wide-continuation cells (`LAYOUT_WIDE_CONT`) are skipped.
 * - Grapheme clusters append their extra codepoints after the base.
 * - Trailing whitespace is trimmed from each row.
 * - Rows are separated by `'\n'` except the last.
 *
 * @param start        Top-left corner of the selection (x = col, y = row).
 * @param end          Bottom-right corner of the selection (inclusive).
 * @param scrollOffset Number of rows the viewport is scrolled back (0 = live view).
 * @return A `juce::String` containing the selected text.
 * @note MESSAGE THREAD — caller must hold `resizeLock`.
 */
juce::String Grid::extractText (juce::Point<int> start, juce::Point<int> end, int scrollOffset) const
{
    int startRow { start.y };
    int startCol { start.x };
    int endRow { end.y };
    int endCol { end.x };

    if (startRow > endRow or (startRow == endRow and startCol > endCol))
    {
        std::swap (startRow, endRow);
        std::swap (startCol, endCol);
    }

    startRow = juce::jlimit (0, getVisibleRows() - 1, startRow);
    endRow = juce::jlimit (0, getVisibleRows() - 1, endRow);
    startCol = juce::jlimit (0, getCols() - 1, startCol);
    endCol = juce::jlimit (0, getCols() - 1, endCol);

    juce::String result;

    for (int row { startRow }; row <= endRow; ++row)
    {
        const Cell* cells { scrollbackRow (row, scrollOffset) };

        if (cells != nullptr)
        {
            int firstCol { (row == startRow) ? startCol : 0 };
            const int lastCol { (row == endRow) ? endCol : getCols() - 1 };

            if (firstCol > 0 and (*(cells + firstCol)).isWideContinuation())
                --firstCol;

            juce::String rowText;

            for (int col { firstCol }; col <= lastCol; ++col)
            {
                const Cell& cell { *(cells + col) };
                const Grapheme* gRow { cell.hasGrapheme() ? scrollbackGraphemeRow (row, scrollOffset) : nullptr };
                const Grapheme* grapheme { gRow != nullptr ? gRow + col : nullptr };
                appendCellText (cell, grapheme, rowText);
            }

            rowText = rowText.trimEnd();

            if (row < endRow)
                rowText += "\n";

            result += rowText;
        }
    }

    return result;
}

/**
 * @brief Extracts a box (rectangle) selection of text as a UTF-32 string.
 *
 * Applies the same column range `[topLeft.x, bottomRight.x]` to every row in
 * `[topLeft.y, bottomRight.y]`.  This produces a strict rectangular region —
 * the column range does not change between rows, unlike `extractText()` which
 * uses row-wrapped semantics.
 *
 * Empty cells are emitted as spaces; wide-continuation cells are skipped.
 * Trailing whitespace is trimmed from each row.  Rows are separated by `'\n'`
 * except the last.
 *
 * @param topLeft      Top-left corner of the rectangle (x = col, y = row).
 *                     Must already be normalised (min col/row of the selection).
 * @param bottomRight  Bottom-right corner of the rectangle (inclusive).
 *                     Must already be normalised (max col/row of the selection).
 * @param scrollOffset Number of rows the viewport is scrolled back (0 = live view).
 * @return A `juce::String` containing the selected text.
 * @note MESSAGE THREAD — caller must hold `resizeLock`.
 */
juce::String Grid::extractBoxText (juce::Point<int> topLeft, juce::Point<int> bottomRight, int scrollOffset) const
{
    const int startRow { juce::jlimit (0, getVisibleRows() - 1, topLeft.y) };
    const int endRow   { juce::jlimit (0, getVisibleRows() - 1, bottomRight.y) };
    const int startCol { juce::jlimit (0, getCols() - 1, topLeft.x) };
    const int endCol   { juce::jlimit (0, getCols() - 1, bottomRight.x) };

    juce::String result;

    for (int row { startRow }; row <= endRow; ++row)
    {
        const Cell* cells { scrollbackRow (row, scrollOffset) };

        if (cells != nullptr)
        {
            juce::String rowText;

            for (int col { startCol }; col <= endCol; ++col)
            {
                const Cell& cell { *(cells + col) };
                const Grapheme* gRow { cell.hasGrapheme() ? scrollbackGraphemeRow (row, scrollOffset) : nullptr };
                const Grapheme* grapheme { gRow != nullptr ? gRow + col : nullptr };
                appendCellText (cell, grapheme, rowText);
            }

            rowText = rowText.trimEnd();

            if (row < endRow)
                rowText += "\n";

            result += rowText;
        }
    }

    return result;
}

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Terminal
