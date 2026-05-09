/**
 * @file ParserVT.cpp
 * @brief Ground-state VT handler: print, execute (C0 control codes), and reset.
 *
 * This translation unit implements the three foundational Parser actions that
 * operate in the VT ground state:
 *
 * - **print** — writes a Unicode codepoint to the active screen buffer via
 *   Grid::getWritePointer() — writes Cell in-place, handling grapheme cluster
 *   extension, wide characters, emoji variation selectors, line-drawing charset
 *   substitution, and cursor advancement.
 *
 * - **execute** — dispatches C0 control characters (BEL, BS, HT, LF/VT/FF,
 *   CR, SO, SI) to their respective terminal actions.
 *
 * - **reset** — performs a full terminal reset (RIS), clearing the grid,
 *   restoring all mode flags to defaults, and homing the cursor.
 *
 * @par Fast path: processGroundBlock()
 * When the parser is in the `ground` state and the input stream contains a
 * contiguous run of printable ASCII bytes (0x20–0x7E), width-1 UTF-8
 * multi-byte codepoints (lead byte 0xC2–0xF4), or the most common C0 codes
 * (LF, CR, BS), `processGroundBlock()` processes the entire run without
 * per-byte dispatch table overhead.  File-local helpers
 * (`GroundOps::handleLineFeed`, `GroundOps::handleCarriageReturn`,
 * `GroundOps::handleBackspace`, `GroundOps::flushPrintCodepoint`) operate on a
 * `GroundOps::Cursor` snapshot to avoid repeated atomic reads from State.
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
 * @see Grid        — ring-buffer cell storage
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

/**
 * @brief Ground-state fast-path operations.
 *
 * Static helper functions operating on a lightweight cursor snapshot.
 * Extracted from the anonymous namespace to comply with naming standards.
 */
struct GroundOps
{
    /**
     * @brief Lightweight cursor snapshot used by the ground-state fast path.
     *
     * `processGroundBlock()` copies the cursor position out of State once at
     * the start of the run and operates on this plain struct throughout the
     * loop, avoiding repeated atomic loads.  The final values are written back
     * to State via `setCursorRow()` / `setCursorCol()` / `setWrapPending()`
     * only if at least one byte was consumed.
     *
     * @see processGroundBlock()
     * @see GroundOps::flushPrintCodepoint()
     */
    struct Cursor
    {
        int row;          ///< Current cursor row (zero-based).
        int col;          ///< Current cursor column (zero-based).
        bool wrapPending; ///< Whether a wrap is pending at the right margin.
    };

    /**
     * @brief Advances the cursor to the next line, calling `grid.scrollUp()` if at scroll bottom.
     *
     * Clears `wrapPending`, then:
     * - If the cursor is exactly at `scrollBottom`, calls `grid.scrollUp()` —
     *   Grid handles the scroll via ring-buffer head advance.
     * - If the cursor is below `scrollBottom` (outside the scroll region),
     *   advances the row clamped to `visibleRows - 1`.
     * - Otherwise increments the row.
     *
     * @param c             Cursor snapshot to update in-place.
     * @param grid          Grid cell buffer to scroll.
     * @param scrollTop     Zero-based index of the first row of the scroll region.
     * @param scrollBottom  Zero-based index of the last row of the scroll region.
     * @param visibleRows   Total number of visible rows in the terminal.
     * @param fill          Fill colour for the new blank line.
     *
     * @note READER THREAD only.
     */
    static inline void handleLineFeed (Cursor& c, Grid& grid,
                                       int scrollTop, int scrollBottom,
                                       int visibleRows, const juce::Colour& fillBg) noexcept
    {
        c.wrapPending = false;

        if (c.row == scrollBottom)
        {
            grid.scrollUp (scrollTop, scrollBottom);

            if (fillBg.getAlpha() > 0)
            {
                jam::Cell* bottom { grid.getWritePointer (scrollBottom) };
                const jam::Cell fill { jam::Cell::erase (fillBg) };

                for (int col { 0 }; col < grid.getNumCols(); ++col)
                    bottom[col] = fill;
            }
        }
        else if (c.row > scrollBottom)
        {
            c.row = juce::jmin (c.row + 1, visibleRows - 1);
        }
        else
        {
            ++c.row;
        }
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
    static inline void handleCarriageReturn (Cursor& c) noexcept
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
    static inline void handleBackspace (Cursor& c) noexcept
    {
        if (c.col > 0)
        {
            --c.col;
        }

        c.wrapPending = false;
    }

    /**
     * @brief Writes the cell directly to Grid via getWritePointer, with wrap handling.
     *
     * This is the innermost loop body of the ground-state fast path.  It:
     * 1. Resolves any pending wrap by calling `grid.scrollUp()` (if at
     *    `scrollBottom`) or incrementing the row, then resets column to 0.
     * 2. Stores the supplied codepoint directly into the cell template.
     * 3. Writes the jam::Cell to the Grid via getWritePointer.
     * 4. Either sets `wrapPending` (if the cursor is now at the right margin)
     *    or advances `c.col`.
     *
     * @param c              Cursor snapshot (modified in-place).
     * @param grid           Grid cell buffer.
     * @param scrollTop      Zero-based index of the first row of the scroll region.
     * @param scrollBottom   Zero-based index of the last row of the scroll region.
     * @param visibleRows    Terminal visible row count.
     * @param cols           Terminal column count (right margin = cols - 1).
     * @param autoWrap       Whether auto-wrap mode (DECAWM) is active.
     * @param cellTemplate   Pre-populated jam::Cell with the current pen attributes.
     * @param codepoint      The Unicode codepoint to write (post-charset-translation
     *                       for ASCII, raw for UTF-8).
     * @param fillBg         Background colour for blank lines created on wrap.
     * @param activeLinkId   Current hyperlink stamp ID, or 0 when no link is active.
     *
     * @note READER THREAD only.
     *
     * @see processGroundBlock()
     */
    static inline void flushPrintCodepoint (Cursor& c, Grid& grid,
                                            int scrollTop, int scrollBottom,
                                            int visibleRows, int cols, bool autoWrap,
                                            jam::Cell& cellTemplate, uint32_t codepoint,
                                            const juce::Colour& fillBg,
                                            uint16_t activeLinkId) noexcept
    {
        if (c.wrapPending and autoWrap)
        {
            if (c.row == scrollBottom)
            {
                grid.scrollUp (scrollTop, scrollBottom);

                if (fillBg.getAlpha() > 0)
                {
                    jam::Cell* bottom { grid.getWritePointer (scrollBottom) };
                    const jam::Cell fill { jam::Cell::erase (fillBg) };

                    for (int col { 0 }; col < grid.getNumCols(); ++col)
                        bottom[col] = fill;
                }
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
        }

        c.wrapPending = false;
        cellTemplate.codepoint = codepoint;

        if (activeLinkId != 0)
        {
            cellTemplate.style |= jam::Cell::UNDERLINE;
        }

        *grid.getWritePointer (c.row, c.col) = cellTemplate;

        // TODO: link ID stamping
        // activeLinkId should be written to a per-row sidecar on Grid.

        if (c.col + 1 >= cols)
        {
            c.wrapPending = true;
        }
        else
        {
            ++c.col;
        }
    }
};

/**
 * @brief Fast-path processor for ground-state printable bytes and common C0 codes.
 *
 * Called by `process()` when the parser is in the `ground` state and the
 * input buffer begins with a byte that can be handled without the full
 * dispatch table.  Processes bytes in a tight loop until it encounters a byte
 * that requires the state machine (e.g. ESC, CSI introducer, a UTF-8 sequence
 * whose continuation bytes are not yet in the buffer, or a wide/combining
 * codepoint that must go through the DFA slow path).
 *
 * @par Handled byte ranges
 * | Byte(s)            | Action                                                              |
 * |--------------------|---------------------------------------------------------------------|
 * | 0x20–0x7E          | `GroundOps::flushPrintCodepoint()` — ASCII print (charset-translated)|
 * | 0x0A / 0x0B / 0x0C | `GroundOps::handleLineFeed()` — LF / VT / FF                       |
 * | 0x0D               | `GroundOps::handleCarriageReturn()` — CR                            |
 * | 0x08               | `GroundOps::handleBackspace()` — BS                                 |
 * | 0xC2–0xF4 (lead)   | UTF-8 multi-byte decode → `flushPrintCodepoint()` (width-1 only)   |
 * | Any other byte     | Loop exits; byte is left for the state machine                      |
 *
 * @par UTF-8 fast path
 * When a lead byte in the range 0xC2–0xF4 is encountered and all continuation
 * bytes are present in the buffer, the sequence is decoded inline.  Only
 * width-1, non-combining codepoints are handled here; wide characters and
 * combining marks break to the DFA slow path so that their full grapheme
 * cluster and wide-cell logic is applied.
 *
 * @par State writeback
 * The cursor position is read from State once at the start and written back
 * only if `consumed > 0`.  The grapheme segmentation state is reset to its
 * initial value after the run (the fast path only handles single-codepoint,
 * non-combining characters, so no cluster state carries over).
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
 * @see GroundOps::flushPrintCodepoint()
 * @see GroundOps::handleLineFeed()
 */
/** @brief UTF-8 sequence length by lead byte (indexed by byte - 0xC2).
 *  0xC2-0xDF = 2, 0xE0-0xEF = 3, 0xF0-0xF4 = 4. */
static constexpr int utf8LeadLength[51] {
    2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2, // 0xC2-0xDF (30 entries)
    3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,                               // 0xE0-0xEF (16 entries)
    4,4,4,4,4                                                        // 0xF0-0xF4 (5 entries)
};

// READER THREAD — bulk ground-state: printable ASCII + UTF-8 width-1 + LF/CR/BS
size_t Parser::processGroundBlock (const uint8_t* data, size_t length) noexcept
{
    const auto scr { state.getRawValue<ActiveScreen> (ID::activeScreen) };
    const int cols { state.getRawValue<int> (ID::cols) };
    const int visibleRows { state.getRawValue<int> (ID::visibleRows) };
    const bool autoWrap { state.getRawValue<bool> (state.modeKey (ID::autoWrap)) };
    const int scrollTop { state.getRawValue<int> (state.screenKey (scr, ID::scrollTop)) };
    const int scrollBottomVal { activeScrollBottom() };

    jam::Cell cellTemplate {};
    cellTemplate.style = stamp.style;
    cellTemplate.width = 1;
    cellTemplate.fg = stamp.fg;
    cellTemplate.bg = stamp.bg;

    const int initCursorRow { state.getRawValue<int> (state.screenKey (scr, ID::cursorRow)) };

    GroundOps::Cursor c { initCursorRow,
                          state.getRawValue<int> (state.screenKey (scr, ID::cursorCol)),
                          state.getRawValue<bool> (state.screenKey (scr, ID::wrapPending)) };

    size_t consumed { 0 };

    for (size_t i { 0 }; i < length; ++i)
    {
        const uint8_t byte { data[i] };

        if (byte >= 0x20 and byte <= 0x7E)
        {
            const uint32_t cp { translateCharset (static_cast<uint32_t> (byte), useLineDrawing) };
            GroundOps::flushPrintCodepoint (c, grid, scrollTop, scrollBottomVal,
                                            visibleRows, cols, autoWrap, cellTemplate,
                                            cp, stamp.bg, activeLinkId);
            lastGraphicChar = cp;
            consumed = i + 1;
            continue;
        }

        if (byte == 0x0A or byte == 0x0B or byte == 0x0C)
        {
            GroundOps::handleLineFeed (c, grid, scrollTop, scrollBottomVal, visibleRows, stamp.bg);
            state.extendOutputBlock (state.getRawValue<int> (ID::scrollbackUsed) + c.row);
            consumed = i + 1;
            continue;
        }

        if (byte == 0x0D)
        {
            GroundOps::handleCarriageReturn (c);
            consumed = i + 1;
            continue;
        }

        if (byte == 0x08)
        {
            GroundOps::handleBackspace (c);
            consumed = i + 1;
            continue;
        }

        // UTF-8 lead byte — decode multi-byte sequence in fast path
        if (byte >= 0xC2 and byte <= 0xF4)
        {
            const int expectedLen { utf8LeadLength[byte - 0xC2] };
            const size_t remaining { length - i };

            if (static_cast<size_t> (expectedLen) <= remaining)
            {
                bool validSequence { true };

                for (int j { 1 }; j < expectedLen and validSequence; ++j)
                    validSequence = (data[i + static_cast<size_t> (j)] >= 0x80
                                 and data[i + static_cast<size_t> (j)] < 0xC0);

                if (validSequence)
                {
                    char utf8Buf[5];

                    for (int j { 0 }; j < expectedLen; ++j)
                        utf8Buf[j] = static_cast<char> (data[i + static_cast<size_t> (j)]);

                    utf8Buf[expectedLen] = '\0';

                    juce::CharPointer_UTF8 decoder (utf8Buf);
                    const uint32_t codepoint { static_cast<uint32_t> (*decoder) };
                    const auto props { charPropsFor (codepoint) };

                    if (props.width() < 2 and not props.isCombiningChar())
                    {
                        cellTemplate.layout = props.isEmojiPresentation()
                                                ? jam::Cell::LAYOUT_EMOJI : 0;

                        GroundOps::flushPrintCodepoint (c, grid, scrollTop, scrollBottomVal,
                                                        visibleRows, cols, autoWrap,
                                                        cellTemplate, codepoint,
                                                        stamp.bg, activeLinkId);
                        lastGraphicChar = codepoint;
                        cellTemplate.layout = 0;

                        i += static_cast<size_t> (expectedLen) - 1;
                        consumed = i + 1;
                        continue;
                    }
                }
            }
        }

        break;
    }

    if (consumed > 0)
    {
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
 * 1. If auto-wrap mode (DECAWM) is active, pushes a LineFeed command and
 *    advances the cursor row.
 * 2. Resets the cursor column to 0.
 * 3. Clears the wrap-pending flag unconditionally.
 *
 * If auto-wrap is disabled, only the wrap-pending flag is cleared (step 3).
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
    if (state.getRawValue<bool> (state.modeKey (ID::autoWrap)))
    {
        const int row { state.getRawValue<int> (state.screenKey (scr, ID::cursorRow)) };
        const int scrollBot { activeScrollBottom() };
        const int visibleRows { state.getRawValue<int> (ID::visibleRows) };
        const int scrollTop { state.getRawValue<int> (state.screenKey (scr, ID::scrollTop)) };

        if (row == scrollBot)
        {
            grid.scrollUp (scrollTop, scrollBot);

            if (stamp.bg.getAlpha() > 0)
            {
                jam::Cell* bottom { grid.getWritePointer (scrollBot) };
                const jam::Cell fill { jam::Cell::erase (stamp.bg) };
                const int cols { state.getRawValue<int> (ID::cols) };

                for (int col { 0 }; col < cols; ++col)
                    bottom[col] = fill;
            }
        }
        else if (row > scrollBot)
        {
            state.setCursorRow (scr, juce::jmin (row + 1, visibleRows - 1));
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
 * non-spacing mark that extends the previous grapheme cluster.  Grapheme
 * cluster extension is deferred — a `// TODO: grapheme sidecar on Grid`
 * marker is left; the base Print command was already pushed on the previous
 * call.
 *
 * @par Case 2 — New grapheme cluster (normal codepoint)
 * 1. Any pending wrap is resolved via `resolveWrapPending()`.
 * 2. If the character is wide (width 2) and would overflow the right margin,
 *    the cursor wraps to the next line (if auto-wrap is enabled).
 * 3. A `jam::Cell` is built from the codepoint, current pen attributes, and
 *    character width.  The cell is written to Grid via getWritePointer().
 * 4. For wide characters, a second cell with LAYOUT_WIDE_CONT is written
 *    to the adjacent column.
 * 5. The cursor is advanced by `cellWidth` columns, or `wrapPending` is set
 *    if the cursor has reached the right margin.
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
    const auto scr { state.getRawValue<ActiveScreen> (ID::activeScreen) };
    const auto props { charPropsFor (codepoint) };
    const auto segResult { graphemeSegmentationStep (graphemeState, props) };

    graphemeState = segResult;

    if (segResult.addToCurrentCell())
    {
        // TODO: grapheme sidecar on Grid
        // Previously: read rowCells(row), rowGraphemes(row), update grapheme cluster,
        // handle VS-15/VS-16 width promotion/demotion.  Deferred — Grid needs a
        // grapheme sidecar similar to jam::Cells.
    }
    else
    {
        const int rawWidth { props.width() };
        const int cellWidth { rawWidth < 1 ? 1 : rawWidth };
        const int cols { state.getRawValue<int> (ID::cols) };

        if (state.getRawValue<bool> (state.screenKey (scr, ID::wrapPending)))
        {
            resolveWrapPending (scr);
        }

        const int row { state.getRawValue<int> (state.screenKey (scr, ID::cursorRow)) };
        const int col { state.getRawValue<int> (state.screenKey (scr, ID::cursorCol)) };

        if (cellWidth == 2 and col + 2 > cols)
        {
            if (state.getRawValue<bool> (state.modeKey (ID::autoWrap)))
            {
                const int scrollBot { activeScrollBottom() };
                const int visibleRows { state.getRawValue<int> (ID::visibleRows) };
                const int scrollTop { state.getRawValue<int> (state.screenKey (scr, ID::scrollTop)) };

                if (row == scrollBot)
                {
                    grid.scrollUp (scrollTop, scrollBot);

                    if (stamp.bg.getAlpha() > 0)
                    {
                        jam::Cell* bottom { grid.getWritePointer (scrollBot) };
                        const jam::Cell fill { jam::Cell::erase (stamp.bg) };

                        for (int c { 0 }; c < cols; ++c)
                            bottom[c] = fill;
                    }
                }
                else if (row > scrollBot)
                {
                    state.setCursorRow (scr, juce::jmin (row + 1, visibleRows - 1));
                }
                else
                {
                    state.setCursorRow (scr, row + 1);
                }

                state.setCursorCol (scr, 0);
                state.setWrapPending (scr, false);
            }
        }

        const int writeRow { state.getRawValue<int> (state.screenKey (scr, ID::cursorRow)) };
        const int writeCol { state.getRawValue<int> (state.screenKey (scr, ID::cursorCol)) };

        jam::Cell cell {};
        cell.codepoint = translateCharset (codepoint, useLineDrawing);
        cell.style = stamp.style;
        cell.width = static_cast<uint8_t> (cellWidth);
        cell.fg = stamp.fg;
        cell.bg = stamp.bg;

        if (props.isEmojiPresentation())
        {
            cell.layout |= jam::Cell::LAYOUT_EMOJI;
        }

        if (activeLinkId != 0)
        {
            cell.style |= jam::Cell::UNDERLINE;
        }

        // TODO: link ID stamping on Grid
        // Previously: linkIds[writeCol] = activeLinkId stamped to sidecar.
        // Grid needs a per-row link ID sidecar.

        *grid.getWritePointer (writeRow, writeCol) = cell;

        lastGraphicChar = codepoint;

        if (cellWidth == 2 and writeCol + 1 < cols)
        {
            jam::Cell cont {};
            cont.codepoint = 0;
            cont.style = stamp.style;
            cont.width = 1;
            cont.fg = stamp.fg;
            cont.bg = stamp.bg;
            cont.layout = jam::Cell::LAYOUT_WIDE_CONT;

            *grid.getWritePointer (writeRow, writeCol + 1) = cont;
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
 * Calls `grid.scrollUp()` if at scroll bottom, delegates cursor movement to
 * `cursorGoToNextLine()`.  If the cursor is already at `scrollBottom`,
 * the row stays in place — Grid handles the scroll via ring-buffer head advance.
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
    const int scrollBot { activeScrollBottom() };
    const int visibleRows { state.getRawValue<int> (ID::visibleRows) };
    const int cursorRow { state.getRawValue<int> (state.screenKey (scr, ID::cursorRow)) };
    const int scrollTop { state.getRawValue<int> (state.screenKey (scr, ID::scrollTop)) };

    if (cursorRow == scrollBot)
    {
        grid.scrollUp (scrollTop, scrollBot);

        if (stamp.bg.getAlpha() > 0)
        {
            jam::Cell* bottom { grid.getWritePointer (scrollBot) };
            const jam::Cell fill { jam::Cell::erase (stamp.bg) };
            const int cols { state.getRawValue<int> (ID::cols) };

            for (int col { 0 }; col < cols; ++col)
                bottom[col] = fill;
        }
    }

    cursorGoToNextLine (scr, scrollBot, visibleRows);

    state.extendOutputBlock (state.getRawValue<int> (ID::scrollbackUsed) + state.getRawValue<int> (state.screenKey (scr, ID::cursorRow)));
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
    const auto scr { state.getRawValue<ActiveScreen> (ID::activeScreen) };
    const int cols { state.getRawValue<int> (ID::cols) };

    switch (controlByte)
    {
        case 0x07:
            if (onBell)
                juce::MessageManager::callAsync ([this] { /* MESSAGE THREAD */ onBell(); });
            break;

        case 0x08:
            if (state.getRawValue<int> (state.screenKey (scr, ID::cursorCol)) > 0)
            {
                state.setCursorCol (scr, state.getRawValue<int> (state.screenKey (scr, ID::cursorCol)) - 1);
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
    state.setCursorVisible (normal, true);
    state.setMode (ID::reverseVideo, false);

    state.resetKeyboardMode (normal);
    state.resetKeyboardMode (alternate);
}

/**
 * @brief Performs a full terminal reset (RIS — Reset to Initial State).
 *
 * Equivalent to the ESC c sequence.  Restores the terminal to a clean
 * power-on state:
 * 1. Switches to the normal screen buffer via `grid.setScreen()`.
 * 2. Homes the cursor via `resetCursor()`.
 * 3. Resets all mode flags via `resetModes()`.
 * 4. Resets the active pen via `resetPen()`.
 * 5. Disables the line-drawing charset.
 * 6. Calls `grid.clear()` to clear the visible grid.
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
    grid.setScreen (false);
    resetCursor (state.getRawValue<int> (ID::cols));
    resetModes();
    resetPen();
    useLineDrawing = false;
    g0LineDrawing  = false;
    g1LineDrawing  = false;
    activeLinkId   = 0;

    grid.clear();

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
} // namespace Terminal
