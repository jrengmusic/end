/**
 * @file ParserVT.cpp
 * @brief Ground-state VT handler: print, execute (C0 control codes), and reset.
 *
 * This translation unit implements the three foundational Parser actions that
 * operate in the VT ground state:
 *
 * - **print** — writes a Unicode codepoint to the active screen buffer,
 *   handling grapheme cluster extension, wide characters, emoji variation
 *   selectors, line-drawing charset substitution, and cursor advancement.
 *
 * - **execute** — dispatches C0 control characters (BEL, BS, HT, LF/VT/FF,
 *   CR, SO, SI) to their respective terminal actions.
 *
 * - **reset** — performs a full terminal reset (RIS), clearing the grid,
 *   restoring all mode flags to defaults, and homing the cursor.
 *
 * @par Fast path: processGroundChunk()
 * When the parser is in the `ground` state and the input stream contains a
 * contiguous run of printable ASCII bytes (0x20–0x7E) mixed with the most
 * common C0 codes (LF, CR, BS), `processGroundChunk()` processes the entire
 * run without per-byte dispatch table overhead.  File-local helpers
 * (`handleLineFeed`, `handleCarriageReturn`, `handleBackspace`,
 * `flushPrintRun`) operate on a `GroundCursor` snapshot to avoid repeated
 * atomic reads from State.
 *
 * @par Thread model
 * All functions in this file run exclusively on the **READER THREAD**.
 * Cross-thread communication is performed through State's atomic setters and
 * the `writeToHost` / `onBell` callbacks (which must be thread-safe on the
 * caller's side).
 *
 * @see Parser.h   — class declaration and full API documentation
 * @see ParserCSI.cpp — CSI sequence dispatch
 * @see ParserESC.cpp — ESC sequence dispatch
 * @see Grid        — screen buffer written by print() and erase operations
 * @see State       — atomic terminal parameter store
 * @see CharProps   — Unicode character property queries used by print()
 */

#include "Parser.h"
#include "Grid.h"
#include "../data/CharProps.h"

namespace Terminal
{ /*____________________________________________________________________________*/

// ============================================================================
// VT Handler: print
// ============================================================================

namespace
{
    /**
     * @brief Lightweight cursor snapshot used by the ground-state fast path.
     *
     * `processGroundChunk()` copies the cursor position out of State once at
     * the start of the run and operates on this plain struct throughout the
     * loop, avoiding repeated atomic loads.  The final values are written back
     * to State via `setCursorRow()` / `setCursorCol()` / `setWrapPending()`
     * only if at least one byte was consumed.
     *
     * @see processGroundChunk()
     * @see flushPrintRun()
     */
    struct GroundCursor
    {
        int row;           ///< Current cursor row (zero-based).
        int col;           ///< Current cursor column (zero-based).
        bool wrapPending;  ///< Whether a wrap is pending at the right margin.
        Cell* cellRow;     ///< Direct pointer to the current row's cell array.
    };

    /**
     * @brief Advances the cursor to the next line, scrolling the region if needed.
     *
     * Clears `wrapPending`, then:
     * - If the cursor is exactly at `scrollBottom`, scrolls the region up one line.
     * - If the cursor is below `scrollBottom` (outside the scroll region), advances
     *   the row clamped to `visibleRows - 1`.
     * - Otherwise increments the row.
     * Updates `c.cellRow` to point at the new row.
     *
     * @param c             Cursor snapshot to update in-place.
     * @param grid          Terminal screen buffer (for scroll and row pointer).
     * @param scrollTop     Zero-based index of the first row of the scroll region.
     * @param scrollBottom  Zero-based index of the last row of the scroll region.
     * @param visibleRows   Total number of visible rows in the terminal.
     *
     * @note READER THREAD only.
     */
    inline void handleLineFeed (GroundCursor& c, Grid& grid, int scrollTop, int scrollBottom, int visibleRows, const Cell& fill) noexcept
    {
        c.wrapPending = false;

        if (c.row >= visibleRows - 2)
            DBG ("LF-AT row=" + juce::String (c.row) + " scrollBot=" + juce::String (scrollBottom) + " visRows=" + juce::String (visibleRows));

        if (c.row == scrollBottom)
        {
            grid.scrollRegionUp (scrollTop, scrollBottom, 1, fill);
        }
        else if (c.row > scrollBottom)
        {
            c.row = juce::jmin (c.row + 1, visibleRows - 1);
        }
        else
        {
            ++c.row;
        }

        c.cellRow = grid.directRowPtr (c.row);
    }

    /**
     * @brief Moves the cursor to column 0 and clears wrap-pending.
     *
     * Implements the Carriage Return (CR, 0x0D) action in the fast path.
     *
     * @param c  Cursor snapshot to update in-place.
     *
     * @note READER THREAD only.
     */
    inline void handleCarriageReturn (GroundCursor& c) noexcept
    {
        c.col = 0;
        c.wrapPending = false;
    }

    /**
     * @brief Moves the cursor one column to the left, clamped to column 0.
     *
     * Implements the Backspace (BS, 0x08) action in the fast path.
     * Clears wrap-pending regardless of whether the cursor actually moved.
     *
     * @param c  Cursor snapshot to update in-place.
     *
     * @note READER THREAD only.
     */
    inline void handleBackspace (GroundCursor& c) noexcept
    {
        if (c.col > 0)
        {
            --c.col;
        }

        c.wrapPending = false;
    }

    /**
     * @brief Writes a single printable ASCII byte to the grid at the cursor position.
     *
     * This is the innermost loop body of the ground-state fast path.  It:
     * 1. Resolves any pending wrap (scrolling if at `scrollBottom`, otherwise
     *    incrementing the row and resetting the column to 0).
     * 2. Optionally translates the byte through the line-drawing charset via
     *    `translateCharset()`.
     * 3. Writes the cell directly into `c.cellRow[c.col]` using the pre-built
     *    `cellTemplate`.
     * 4. Either sets `wrapPending` (if the cursor is now at the right margin)
     *    or advances `c.col`.
     *
     * The dirty-bit for the current row is OR'd into `localDirty` so that a
     * single `grid.batchMarkDirty()` call can flush all dirty rows at the end
     * of the chunk.
     *
     * @param c              Cursor snapshot (modified in-place).
     * @param grid           Terminal screen buffer.
     * @param scrollTop      Zero-based index of the first row of the scroll region.
     * @param scrollBottom   Zero-based index of the last row of the scroll region.
     * @param cols           Terminal column count (right margin = cols - 1).
     * @param autoWrap       Whether auto-wrap mode (DECAWM) is active.
     * @param localDirty     Bit-array of dirty rows (4 × 64-bit words = 256 rows).
     * @param cellTemplate   Pre-populated Cell with the current pen attributes.
     * @param byte           The printable ASCII byte to write (0x20–0x7E).
     * @param useLineDrawing Whether the DEC Special Graphics charset is active.
     *
     * @note READER THREAD only.
     *
     * @see processGroundChunk()
     * @see translateCharset()
     */
    inline void flushPrintRun (GroundCursor& c, Grid& grid, int scrollTop, int scrollBottom,
                                int visibleRows, int cols, bool autoWrap, uint64_t* localDirty,
                                Cell& cellTemplate, uint8_t byte, bool useLineDrawing, const Cell& fill) noexcept
    {
        if (c.wrapPending and autoWrap)
        {
            grid.activeVisibleRowState (c.row).setWrapped (true);

            if (c.row == scrollBottom)
            {
                grid.scrollRegionUp (scrollTop, scrollBottom, 1, fill);
            }
            else if (c.row > scrollBottom)
            {
                c.row = juce::jmin (c.row + 1, visibleRows - 1);
            }
            else
            {
                ++c.row;
            }

            c.col = 0;
            c.cellRow = grid.directRowPtr (c.row);
        }

        c.wrapPending = false;
        cellTemplate.codepoint = translateCharset (static_cast<uint32_t> (byte), useLineDrawing);
        c.cellRow[c.col] = cellTemplate;

        if (c.row == visibleRows - 1)
            DBG ("BOTTOM-FAST col=" + juce::String (c.col) + " cp=U+" + juce::String::toHexString ((int) cellTemplate.codepoint).paddedLeft ('0', 4) + " '" + juce::String::charToString (static_cast<juce::juce_wchar> (byte)) + "'");

        if (c.col + 1 >= cols)
        {
            localDirty[c.row >> 6] |= uint64_t { 1 } << (c.row & 63);
            c.wrapPending = true;
        }
        else
        {
            ++c.col;
        }
    }
}

/**
 * @brief Fast-path processor for ground-state printable ASCII and common C0 codes.
 *
 * Called by `process()` when the parser is in the `ground` state and the
 * input buffer begins with a byte that can be handled without the full
 * dispatch table.  Processes bytes in a tight loop until it encounters a byte
 * that requires the state machine (e.g. ESC, CSI introducer, or any byte
 * outside the handled set).
 *
 * @par Handled byte ranges
 * | Byte(s)         | Action                                      |
 * |-----------------|---------------------------------------------|
 * | 0x20–0x7E       | `flushPrintRun()` — write cell to grid      |
 * | 0x0A / 0x0B / 0x0C | `handleLineFeed()` — LF / VT / FF      |
 * | 0x0D            | `handleCarriageReturn()` — CR               |
 * | 0x08            | `handleBackspace()` — BS                    |
 * | Any other byte  | Loop exits; byte is left for the state machine |
 *
 * @par Dirty tracking
 * Rather than calling `grid.markRowDirty()` on every cell write, the function
 * accumulates dirty bits in a local 256-bit array (`localDirty[4]`) and
 * flushes them with a single `grid.batchMarkDirty()` call at the end.
 *
 * @par State writeback
 * The cursor position is read from State once at the start and written back
 * only if `consumed > 0`.  The grapheme segmentation state is reset to its
 * initial value after the run (the fast path only handles single-codepoint
 * ASCII characters, so no cluster state carries over).
 *
 * @param data    Pointer to the first byte of the input buffer.
 * @param length  Maximum number of bytes to examine.
 *
 * @return Number of bytes consumed from `data` (0 if the first byte is not
 *         in the handled set).
 *
 * @note READER THREAD only.
 *
 * @see process()
 * @see flushPrintRun()
 * @see handleLineFeed()
 */
// READER THREAD — bulk ground-state: printable ASCII + LF/CR/BS
size_t Parser::processGroundChunk (const uint8_t* data, size_t length) noexcept
{
    const auto scr { state.getScreen() };
    const int cols { grid.getCols() };
    const int visibleRows { grid.getVisibleRows() };
    const bool autoWrap { state.getMode (ID::autoWrap) };
    const int scrollTop { state.getScrollTop (scr) };

    Cell cellTemplate {};
    cellTemplate.style = stamp.style;
    cellTemplate.width = 1;
    cellTemplate.fg = stamp.fg;
    cellTemplate.bg = stamp.bg;

    Cell fill {};
    fill.bg = stamp.bg;

    GroundCursor c { state.getCursorRow (scr), state.getCursorCol (scr),
                     state.isWrapPending (scr), grid.directRowPtr (state.getCursorRow (scr)) };

    uint64_t localDirty[4] { 0, 0, 0, 0 };
    size_t consumed { 0 };

    for (size_t i { 0 }; i < length; ++i)
    {
        const uint8_t byte { data[i] };

        if (byte >= 0x20 and byte <= 0x7E)
        {
            flushPrintRun (c, grid, scrollTop, scrollBottom, visibleRows, cols, autoWrap, localDirty, cellTemplate, byte, useLineDrawing, fill);
            lastGraphicChar = cellTemplate.codepoint;
            consumed = i + 1;
            continue;
        }

        if (byte == 0x0A or byte == 0x0B or byte == 0x0C)
        {
            localDirty[c.row >> 6] |= uint64_t { 1 } << (c.row & 63);
            handleLineFeed (c, grid, scrollTop, scrollBottom, visibleRows, fill);
            consumed = i + 1;
            continue;
        }

        if (byte == 0x0D)
        {
            localDirty[c.row >> 6] |= uint64_t { 1 } << (c.row & 63);
            handleCarriageReturn (c);
            consumed = i + 1;
            continue;
        }

        if (byte == 0x08)
        {
            handleBackspace (c);
            consumed = i + 1;
            continue;
        }

        break;
    }

    if (consumed > 0)
    {
        localDirty[c.row >> 6] |= uint64_t { 1 } << (c.row & 63);
        grid.batchMarkDirty (localDirty);
        state.setCursorCol (scr, c.col);
        state.setCursorRow (scr, c.row);
        state.setWrapPending (scr, c.wrapPending);
        graphemeState = graphemeSegmentationInit();
    }

    return consumed;
}

/**
 * @brief Resolves a pending line wrap before writing a new character.
 *
 * When `State::isWrapPending()` is true and a new printable codepoint arrives,
 * this method is called before the cell write to commit the deferred wrap:
 *
 * 1. If auto-wrap mode (DECAWM) is active, marks the current row as wrapped
 *    via `Grid::activeVisibleRowState().setWrapped(true)`.
 * 2. Scrolls the scroll region up by one line if the cursor is at
 *    `scrollBottom`, otherwise increments the cursor row.
 * 3. Resets the cursor column to 0.
 * 4. Clears the wrap-pending flag unconditionally.
 *
 * If auto-wrap is disabled, only the wrap-pending flag is cleared (step 4).
 *
 * @param scr  Target screen buffer (normal or alternate).
 *
 * @note READER THREAD only.
 *
 * @see print()
 * @see cursorGoToNextLine()
 * @see State::isWrapPending()
 */
void Parser::resolveWrapPending (ActiveScreen scr) noexcept
{
    if (state.getMode (ID::autoWrap))
    {
        grid.activeVisibleRowState (state.getCursorRow (scr)).setWrapped (true);

        const int row { state.getCursorRow (scr) };

        if (row == scrollBottom)
        {
            Cell fill {};
            fill.bg = stamp.bg;
            grid.scrollRegionUp (state.getScrollTop (scr), scrollBottom, 1, fill);
        }
        else if (row > scrollBottom)
        {
            state.setCursorRow (scr, juce::jmin (row + 1, grid.getVisibleRows() - 1));
        }
        else
        {
            state.setCursorRow (scr, row + 1);
        }
        state.setCursorCol (scr, 0);
    }

    state.setWrapPending (scr, false);
}

/**
 * @brief Writes a Unicode codepoint to the active screen at the cursor position.
 *
 * This is the primary character output function.  It handles two distinct
 * cases based on the Unicode grapheme segmentation result:
 *
 * @par Case 1 — Grapheme cluster extension (`segResult.addToCurrentCell()`)
 * The codepoint is a combining character, variation selector, or other
 * non-spacing mark that extends the previous grapheme cluster.  The codepoint
 * is appended to the `Grapheme` record of the previous cell rather than
 * creating a new cell.  Special handling is applied for:
 * - **U+FE0F** (Variation Selector-16 / emoji presentation): if the base
 *   character is an emoji variation base with width 1, it is promoted to
 *   width 2 and a wide-continuation cell is inserted to its right.
 * - **U+FE0E** (Variation Selector-15 / text presentation): if the base
 *   character is currently width 2, it is demoted to width 1 and the
 *   continuation cell is erased.
 *
 * @par Case 2 — New grapheme cluster (normal codepoint)
 * 1. Any pending wrap is resolved via `resolveWrapPending()`.
 * 2. If the character is wide (width 2) and would overflow the right margin,
 *    the cursor wraps to the next line (if auto-wrap is enabled).
 * 3. A `Cell` is built from the codepoint, current pen attributes, and
 *    character width.  The codepoint is passed through `translateCharset()`
 *    to apply any active line-drawing substitution.
 * 4. For wide characters, a `LAYOUT_WIDE_CONT` continuation cell is written
 *    to the column immediately to the right.
 * 5. The cursor is advanced by `cellWidth` columns, or `wrapPending` is set
 *    if the cursor has reached the right margin.
 *
 * @par Grapheme segmentation
 * `graphemeSegmentationStep()` is called on every codepoint to maintain the
 * Unicode grapheme cluster break algorithm state across successive calls.
 * The result determines whether the codepoint starts a new cluster or extends
 * the previous one.
 *
 * @param codepoint  Unicode scalar value to print (U+0000–U+10FFFF).
 *
 * @note READER THREAD only.
 *
 * @see resolveWrapPending()
 * @see handlePrintByte()
 * @see translateCharset()
 * @see charPropsFor()
 * @see graphemeSegmentationStep()
 */
void Parser::print (uint32_t codepoint) noexcept
{
    const auto scr { state.getScreen() };
    const auto props { charPropsFor (codepoint) };
    const auto segResult { graphemeSegmentationStep (graphemeState, props) };

    graphemeState = segResult;

    if (segResult.addToCurrentCell())
    {
        const int curCol { state.getCursorCol (scr) };
        int prevCol { curCol > 0 ? curCol - 1 : 0 };
        const int row { state.getCursorRow (scr) };

        const Cell* rowCells { grid.activeVisibleRow (row) };

        if (rowCells != nullptr and prevCol > 0 and rowCells[prevCol].isWideContinuation())
        {
            --prevCol;
        }

        if (state.isWrapPending (scr) and curCol == 0)
        {
            // TODO: handle wrap-pending grapheme append to previous line's last cell
        }

        const Grapheme* existing { grid.activeReadGrapheme (row, prevCol) };
        Grapheme g {};

        if (existing != nullptr)
        {
            g = *existing;
        }

        if (g.count < static_cast<uint8_t> (g.extraCodepoints.size()))
        {
            g.extraCodepoints.at (g.count) = codepoint;
            ++g.count;
        }

        grid.activeWriteGrapheme (row, prevCol, g);

        if (props.isEmojiPresentation())
        {
            Cell* rowPtr { grid.activeVisibleRow (row) };

            if (rowPtr != nullptr)
            {
                rowPtr[prevCol].layout |= Cell::LAYOUT_EMOJI;
            }
        }

        if (codepoint == 0xFE0F)
        {
            Cell* rowPtr { grid.activeVisibleRow (row) };

            if (rowPtr != nullptr)
            {
                Cell& base { rowPtr[prevCol] };
                const auto baseProps { charPropsFor (base.codepoint) };

                if (baseProps.isEmojiVariationBase() and base.width == 1)
                {
                    const int cols { grid.getCols() };
                    base.width = 2;
                    base.layout |= Cell::LAYOUT_EMOJI;

                    if (prevCol + 1 < cols)
                    {
                        Cell cont {};
                        cont.codepoint = 0;
                        cont.style = base.style;
                        cont.width = 1;
                        cont.fg = base.fg;
                        cont.bg = base.bg;
                        cont.layout = Cell::LAYOUT_WIDE_CONT;
                        grid.activeWriteCell (row, prevCol + 1, cont);
                        grid.activeEraseGrapheme (row, prevCol + 1);
                    }

                    const int newCursorCol { prevCol + 2 < cols ? prevCol + 2 : cols };
                    state.setCursorCol (scr, newCursorCol);

                    if (newCursorCol >= cols)
                    {
                        state.setWrapPending (scr, true);
                    }

                    grid.markRowDirty (row);
                }
            }
        }

        if (codepoint == 0xFE0E)
        {
            Cell* rowPtr { grid.activeVisibleRow (row) };

            if (rowPtr != nullptr)
            {
                Cell& base { rowPtr[prevCol] };
                const auto baseProps { charPropsFor (base.codepoint) };

                if (baseProps.isEmojiVariationBase() and base.width == 2)
                {
                    base.width = 1;
                    base.layout &= static_cast<uint8_t> (~Cell::LAYOUT_EMOJI);

                    const int cols { grid.getCols() };

                    if (prevCol + 1 < cols)
                    {
                        Cell empty {};
                        grid.activeWriteCell (row, prevCol + 1, empty);
                        grid.activeEraseGrapheme (row, prevCol + 1);
                    }

                    state.setCursorCol (scr, prevCol + 1);
                    state.setWrapPending (scr, false);

                    grid.markRowDirty (row);
                }
            }
        }
    }
    else
    {
        const int rawWidth { props.width() };
        const int cellWidth { rawWidth < 1 ? 1 : rawWidth };
        const int cols { grid.getCols() };

        if (state.isWrapPending (scr))
        {
            resolveWrapPending (scr);
        }

        const int row { state.getCursorRow (scr) };
        const int col { state.getCursorCol (scr) };

        if (cellWidth == 2 and col + 2 > cols)
        {
            if (state.getMode (ID::autoWrap))
            {
                grid.activeVisibleRowState (row).setWrapped (true);

                if (row == scrollBottom)
                {
                    Cell fill {};
                    fill.bg = stamp.bg;
                    grid.scrollRegionUp (state.getScrollTop (scr), scrollBottom, 1, fill);
                }
                else if (row > scrollBottom)
                {
                    state.setCursorRow (scr, juce::jmin (row + 1, grid.getVisibleRows() - 1));
                }
                else
                {
                    state.setCursorRow (scr, row + 1);
                }

                state.setCursorCol (scr, 0);
                state.setWrapPending (scr, false);
            }
        }

        const int writeRow { state.getCursorRow (scr) };
        const int writeCol { state.getCursorCol (scr) };

        if (writeRow == grid.getVisibleRows() - 1)
            DBG ("BOTTOM-ROW col=" + juce::String (writeCol) + " cp=U+" + juce::String::toHexString ((int) codepoint).paddedLeft ('0', 4) + (codepoint >= 0x20 && codepoint <= 0x7E ? " ch=" + juce::String::charToString ((juce::juce_wchar) codepoint) : ""));

        Cell cell {};
        cell.codepoint = translateCharset (codepoint, useLineDrawing);
        cell.style = stamp.style;
        cell.width = static_cast<uint8_t> (cellWidth);
        cell.fg = stamp.fg;
        cell.bg = stamp.bg;

        if (props.isEmojiPresentation())
        {
            cell.layout |= Cell::LAYOUT_EMOJI;
        }

        grid.activeWriteCell (writeRow, writeCol, cell);
        grid.activeEraseGrapheme (writeRow, writeCol);
        lastGraphicChar = codepoint;

        if (cellWidth == 2 and writeCol + 1 < cols)
        {
            Cell cont {};
            cont.codepoint = 0;
            cont.style = stamp.style;
            cont.width = 1;
            cont.fg = stamp.fg;
            cont.bg = stamp.bg;
            cont.layout = Cell::LAYOUT_WIDE_CONT;
            grid.activeWriteCell (writeRow, writeCol + 1, cont);
            grid.activeEraseGrapheme (writeRow, writeCol + 1);
        }

        if (writeCol + cellWidth >= cols)
        {
            state.setWrapPending (scr, true);
        }
        else
        {
            state.setCursorCol (scr, writeCol + cellWidth);
        }
    }
}

// ============================================================================
// VT Handler: execute (C0 control codes)
// ============================================================================

/**
 * @brief Performs a line feed, advancing the cursor or scrolling the region.
 *
 * Delegates to `cursorGoToNextLine()`.  If the cursor is already at
 * `scrollBottom`, the scroll region is scrolled up by one line instead of
 * moving the cursor.
 *
 * @par Sequence
 * Invoked for LF (0x0A), VT (0x0B), and FF (0x0C) via `execute()`, and
 * also directly from `escDispatchNoIntermediate()` for IND (ESC D) and
 * NEL (ESC E).
 *
 * @param scr  Target screen buffer (normal or alternate).
 *
 * @note READER THREAD only.
 *
 * @see execute()
 * @see cursorGoToNextLine()
 */
void Parser::executeLineFeed (ActiveScreen scr) noexcept
{
    const int elfRow { state.getCursorRow (scr) };
    if (elfRow >= grid.getVisibleRows() - 2)
        DBG ("ELF row=" + juce::String (elfRow) + " scrollBot=" + juce::String (scrollBottom) + " visRows=" + juce::String (grid.getVisibleRows()));

    if (not cursorGoToNextLine (scr, scrollBottom, grid.getVisibleRows()))
    {
        Cell fill {};
        fill.bg = stamp.bg;
        grid.scrollRegionUp (state.getScrollTop (scr), scrollBottom, 1, fill);
    }
}

/**
 * @brief Executes a C0 control character.
 *
 * Dispatches the control byte to the appropriate terminal action.  Only the
 * subset of C0 codes that have defined VT100/VT520 behaviour are handled;
 * all others are silently ignored.
 *
 * @par Handled control codes
 * | Byte | Name | Action                                                    |
 * |------|------|-----------------------------------------------------------|
 * | 0x07 | BEL  | Fires `onBell` callback asynchronously on the message thread |
 * | 0x08 | BS   | Moves cursor one column left (clamped to column 0); clears wrap-pending |
 * | 0x09 | HT   | Advances cursor to the next tab stop via `nextTabStop()`  |
 * | 0x0A | LF   | Line feed — `executeLineFeed()`                           |
 * | 0x0B | VT   | Vertical tab — treated as LF                              |
 * | 0x0C | FF   | Form feed — treated as LF                                 |
 * | 0x0D | CR   | Carriage return — cursor to column 0; clears wrap-pending |
 * | 0x0E | SO   | Shift-Out — (reserved; line-drawing handled via escDispatchCharset) |
 * | 0x0F | SI   | Shift-In  — (reserved; line-drawing handled via escDispatchCharset) |
 *
 * @param controlByte  The C0 control character byte (0x00–0x1F).
 *
 * @note READER THREAD only.  The BEL callback is dispatched to the message
 *       thread via `juce::MessageManager::callAsync`.
 *
 * @see executeLineFeed()
 * @see nextTabStop()
 * @see onBell
 */
void Parser::execute (uint8_t controlByte) noexcept
{
    const auto scr { state.getScreen() };
    const int cols { grid.getCols() };

    switch (controlByte)
    {
        case 0x07:
            if (onBell)
                juce::MessageManager::callAsync ([this] { /* MESSAGE THREAD */ onBell(); });
            break;

        case 0x08:
            if (state.getCursorCol (scr) > 0)
            {
                state.setCursorCol (scr, state.getCursorCol (scr) - 1);
                state.setWrapPending (scr, false);
            }
            break;

        case 0x09:
        {
            const int nextTab { nextTabStop (scr, cols) };
            state.setCursorCol (scr, nextTab);
            state.setWrapPending (scr, false);
            break;
        }

        case 0x0A:
        case 0x0B:
        case 0x0C:
            executeLineFeed (scr);
            break;

        case 0x0D:
            state.setCursorCol (scr, 0);
            state.setWrapPending (scr, false);
            break;

        case 0x0E:
            useLineDrawing = g1LineDrawing;
            break;

        case 0x0F:
            useLineDrawing = g0LineDrawing;
            break;

        default:
            break;
    }
}

// ============================================================================
// VT Handler: Send Response
// ============================================================================

/**
 * @brief Appends a null-terminated response string to the internal response buffer.
 *
 * Responses (device attribute replies, cursor position reports, etc.) are not
 * sent immediately during `process()`.  Instead they are accumulated here and
 * flushed after `process()` returns via `flushResponses()`.  This avoids
 * re-entrant writes to the PTY during parsing.
 *
 * If the response would overflow `responseBuf`, it is silently discarded.
 *
 * @param resp  Null-terminated C string to append.  Must not be null.
 *
 * @note READER THREAD only.
 *
 * @see flushResponses()
 * @see writeToHost
 */
void Parser::sendResponse (const char* resp) noexcept
{
    const int len { static_cast<int> (std::strlen (resp)) };
    const int available { static_cast<int> (sizeof (responseBuf)) - responseLen };

    if (len <= available)
    {
        std::memcpy (responseBuf + responseLen, resp, static_cast<size_t> (len));
        responseLen += len;
    }
}

/**
 * @brief Delivers all queued response bytes to the host via `writeToHost`.
 *
 * Called by the owner (Session) after each `process()` invocation.  If
 * `responseLen > 0` and `writeToHost` is set, the entire `responseBuf` is
 * passed to the callback and `responseLen` is reset to zero.
 *
 * @note READER THREAD only.
 *
 * @see sendResponse()
 * @see writeToHost
 */
void Parser::flushResponses() noexcept
{
    if (responseLen > 0 and writeToHost)
    {
        writeToHost (responseBuf, responseLen);
        responseLen = 0;
    }
}

// ============================================================================
// VT Handler: Full Reset
// ============================================================================

/**
 * @brief Resets all terminal mode flags to their power-on defaults.
 *
 * Writes the default value for every mode flag tracked in State.  Called by
 * `reset()` and by the RIS (Reset to Initial State, ESC c) handler.
 *
 * @par Default mode values
 * | Mode               | Default |
 * |--------------------|---------|
 * | originMode         | false   |
 * | autoWrap           | true    |
 * | applicationCursor  | false   |
 * | bracketedPaste     | false   |
 * | insertMode         | false   |
 * | mouseTracking      | false   |
 * | mouseMotionTracking| false   |
 * | mouseAllTracking   | false   |
 * | mouseSgr           | false   |
 * | focusEvents        | false   |
 * | applicationKeypad  | false   |
 * | cursorVisible      | true    |
 * | reverseVideo       | false   |
 *
 * Also clears the kitty keyboard mode stacks for both screens via
 * `resetKeyboardMode()`, restoring flags to 0 (legacy mode).
 *
 * @note READER THREAD only.
 *
 * @see reset()
 */
void Parser::resetModes() noexcept
{
    state.setMode (ID::originMode, false);
    state.setMode (ID::autoWrap, true);
    state.setMode (ID::applicationCursor, false);
    state.setMode (ID::bracketedPaste, false);
    state.setMode (ID::insertMode, false);
    state.setMode (ID::mouseTracking, false);
    state.setMode (ID::mouseMotionTracking, false);
    state.setMode (ID::mouseAllTracking, false);
    state.setMode (ID::mouseSgr, false);
    state.setMode (ID::focusEvents, false);
    state.setMode (ID::applicationKeypad, false);
    state.setMode (ID::cursorVisible, true);
    state.setMode (ID::reverseVideo, false);

    state.resetKeyboardMode (normal);
    state.resetKeyboardMode (alternate);
}

/**
 * @brief Performs a full terminal reset (RIS — Reset to Initial State).
 *
 * Equivalent to the ESC c sequence.  Restores the terminal to a clean
 * power-on state:
 * 1. Switches to the normal screen buffer.
 * 2. Homes the cursor via `resetCursor()`.
 * 3. Resets all mode flags via `resetModes()`.
 * 4. Resets the active pen via `resetPen()`.
 * 5. Disables the line-drawing charset.
 * 6. Erases the entire visible grid.
 * 7. Calls `calc()` to synchronise internal cached geometry.
 *
 * @note READER THREAD only.
 *
 * @see resetModes()
 * @see resetPen()
 * @see resetCursor()
 * @see calc()
 */
void Parser::reset() noexcept
{
    state.setScreen (normal);
    resetCursor (grid.getCols());
    resetModes();
    resetPen();
    useLineDrawing = false;
    g0LineDrawing = false;
    g1LineDrawing = false;
    grid.eraseRowRange (0, grid.getVisibleRows() - 1);
    calc();
}

// ============================================================================
// New Ops: Pen
// ============================================================================

/**
 * @brief Resets the active pen to default attributes.
 *
 * Replaces `pen` with a default-constructed `Pen`, which has no style flags
 * set and uses the terminal's default foreground and background colours.
 * Called by `reset()` and by SGR 0 (reset all attributes) in `applySGR()`.
 *
 * @note READER THREAD only.
 *
 * @see pen
 * @see applySGR()
 */
void Parser::resetPen() noexcept
{
    pen = Pen {};
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Terminal
