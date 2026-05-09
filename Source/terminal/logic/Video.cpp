/**
 * @file Video.cpp
 * @brief Video constructor, resize, calc, cached geometry helpers, and ground-state VT handlers.
 *
 * This translation unit implements:
 *
 * - Construction, resize, and cached geometry (`calc()`).
 * - `setDimensions()` and `setCellSize()` — cross-thread dimension setters.
 * - Mode flag SSOT (`modePtr()`, `getMode()`, `setMode()`).
 * - `activeScrollBottom()` — effective scroll region bottom.
 * - `scrollUpAndFill()` / `scrollDownAndFill()` — DRY single-row scroll + fill helpers.
 * - Ground-state VT handlers: `print()`, `resolveWrapPending()`, `executeLineFeed()`.
 * - C0 control character dispatch: `applyControlCode()`.
 * - Device response buffering: `sendResponse()`, `flushResponses()`.
 * - Full terminal reset: `reset()`, `resetModes()`, `resetPen()`.
 *
 * @par Thread model
 * All functions in this file run exclusively on the **READER THREAD** unless
 * explicitly noted as MESSAGE THREAD in the function documentation.
 * Cross-thread communication is performed through atomic setters and
 * the `"writeToHost"` / `"bell"` events in the Processor events map.
 *
 * @see Video.h   — class declaration and full API documentation
 * @see VideoCSI.cpp — CSI sequence dispatch
 * @see VideoESC.cpp — ESC sequence dispatch
 * @see VideoMode.cpp — DEC private mode and ANSI mode handlers
 * @see Grid         — ring-buffer cell storage
 * @see CharProps    — Unicode character property queries used by print()
 */

#include "Video.h"
#include "Grid.h"
#include "../data/CharProps.h"

namespace Terminal
{ /*____________________________________________________________________________*/


// ============================================================================
// Public API
// ============================================================================

/**
 * @brief Constructs the Video, binding it to the Grid and events map.
 *
 * Stores references to `grid` and `events` for use throughout the parsing
 * lifetime.  Internal terminal state is initialised to VT power-on defaults
 * (80×24, cursor at home, autoWrap on, cursor visible).
 *
 * The constructor does **not** call `calc()`.  The owner must call `calc()`
 * after construction (and after every `resize()`) to synchronise internal
 * cached geometry.
 *
 * @param grid    Live cell buffer. Video writes in-place; Display reads dirty rows.
 * @param events  Events map owned by Processor.  Video fires events through this map.
 *
 * @note MESSAGE THREAD — called before the reader thread starts.
 *
 * @see calc()
 * @see Video.h
 */
Video::Video (Grid& grid, jam::Function::Map<juce::Identifier, void>& events) noexcept
    : grid (grid)
    , events (events)
{
}

/**
 * @brief Updates internal geometry after terminal dimensions change.
 *
 * Re-clamps the cursor and scroll region to the new bounds on both screen
 * buffers, reinitialises tab stops, and calls `calc()` to update internal
 * cached values.
 *
 * @param newCols         New terminal width in character columns.
 * @param newVisibleRows  New terminal height in visible rows.
 *
 * @note MESSAGE THREAD — called from Processor::resized().
 *
 * @see calc()
 * @see initializeTabStops()
 * @see cursorResetScrollRegion()
 */
void Video::resize (int newCols, int newVisibleRows) noexcept
{
    wrapPending[0] = false;
    wrapPending[1] = false;
    cursorClamp (normal, newCols, newVisibleRows);
    cursorClamp (alternate, newCols, newVisibleRows);
    cursorResetScrollRegion (normal);
    cursorResetScrollRegion (alternate);
    initializeTabStops (newCols);
    calc();
}

/**
 * @brief Synchronises internal cached geometry from State.
 *
 * Copies the current pen style and colour attributes from `pen` into `stamp`
 * (so DECSC saves the up-to-date pen), then recomputes `scrollBottom` from
 * the current visible row count and the active screen's scroll region.
 *
 * Must be called after construction and after every `resize()`.  Also called
 * internally by `cursorSetScrollRegion()` and `cursorResetScrollRegion()`.
 *
 * @note READER THREAD only.
 *
 * @see resize()
 * @see effectiveScrollBottom()
 */
void Video::calc() noexcept
{
    stamp.style = pen.style;
    stamp.fg = pen.fg;
    stamp.bg = pen.bg;
}

/**
 * @brief Sets terminal dimensions from Processor (message thread → reader thread).
 *
 * Stores new column and row counts atomically.  Called by Processor before
 * delegating to `resize()`, so that internal geometry is consistent on
 * the reader thread without requiring a State read.
 *
 * @param newCols  New terminal width in character columns.
 * @param newRows  New terminal height in visible rows.
 *
 * @note MESSAGE THREAD — relaxed atomic stores; eventual consistency is
 *       acceptable because `resize()` is always called immediately after.
 */
void Video::setDimensions (int newCols, int newRows) noexcept
{
    cols.store (newCols, std::memory_order_relaxed);
    visibleRows.store (newRows, std::memory_order_relaxed);
}

/**
 * @brief Sets physical cell dimensions for image decode.
 *
 * Stores `widthPx` and `heightPx` as relaxed atomics so that Sixel, Kitty,
 * and iTerm2 decode paths on the reader thread can load them without a lock.
 *
 * @param widthPx   Cell width in pixels.
 * @param heightPx  Cell height in pixels.
 *
 * @note MESSAGE THREAD — relaxed atomic stores; reader thread reads with relaxed loads.
 */
void Video::setCellSize (int widthPx, int heightPx) noexcept
{
    cellWidth.store (widthPx, std::memory_order_relaxed);
    cellHeight.store (heightPx, std::memory_order_relaxed);
}

/**
 * @brief Returns a mutable pointer to the named mode flag, or nullptr if unknown.
 *
 * Single SSOT for mode flag lookup.  Both `getMode()` and `setMode()` delegate
 * here, eliminating the two duplicate tables that were previously required.
 * O(n) over 13 entries; negligible cost on the reader thread.
 *
 * @param id  A Terminal::ID mode identifier.
 * @return    Pointer to the corresponding member, or nullptr if id is unknown.
 *
 * @note READER THREAD only.
 */
bool* Video::modePtr (juce::Identifier id) noexcept
{
    struct ModeEntry
    {
        juce::Identifier id;
        bool*            member;
    };

    const ModeEntry modes[]
    {
        { ID::originMode,          &originMode          },
        { ID::autoWrap,            &autoWrap            },
        { ID::applicationCursor,   &applicationCursor   },
        { ID::bracketedPaste,      &bracketedPaste      },
        { ID::insertMode,          &insertMode          },
        { ID::mouseTracking,       &mouseTracking       },
        { ID::mouseMotionTracking, &mouseMotionTracking },
        { ID::mouseAllTracking,    &mouseAllTracking    },
        { ID::mouseSgr,            &mouseSgr            },
        { ID::focusEvents,         &focusEvents         },
        { ID::applicationKeypad,   &applicationKeypad   },
        { ID::reverseVideo,        &reverseVideo        },
        { ID::win32InputMode,      &win32InputMode      },
    };

    bool* result { nullptr };

    for (const auto& entry : modes)
    {
        if (entry.id == id)
        {
            result = entry.member;
            break;
        }
    }

    return result;
}

/**
 * @brief Returns the value of the named mode flag.
 *
 * Delegates to `modePtr()` — single SSOT for the mode table.
 *
 * @param id  A Terminal::ID mode identifier.
 * @return    `true` if the mode is set, `false` if unset or the ID is unknown.
 *
 * @note READER THREAD only.
 */
bool Video::getMode (juce::Identifier id) const noexcept
{
    const bool* ptr { const_cast<Video*> (this)->modePtr (id) };
    bool result { false };
    if (ptr != nullptr) result = *ptr;
    return result;
}

/**
 * @brief Sets the named mode flag.
 *
 * Delegates to `modePtr()` — single SSOT for the mode table.
 * Unknown IDs are silently ignored.
 *
 * @param id     A Terminal::ID mode identifier.
 * @param value  The new flag value.
 *
 * @note READER THREAD only.
 */
void Video::setMode (juce::Identifier id, bool value) noexcept
{
    bool* ptr { modePtr (id) };
    if (ptr != nullptr) *ptr = value;
}

/**
 * @brief Returns the effective bottom row of the current scrolling region.
 *
 * Reads from Grid buffer dims (safe on reader thread) on every call.
 * Wraps `effectiveScrollBottom()` with the current screen and visible rows.
 *
 * @see effectiveScrollBottom()
 */
int Video::activeScrollBottom() const noexcept
{
    return effectiveScrollBottom (activeScreen, visibleRows.load (std::memory_order_relaxed));
}


// ============================================================================
// Scroll helpers
// ============================================================================

/**
 * @brief Scrolls the region up one line and fills the bottom row with the current background.
 *
 * Single-row scroll+fill helper that eliminates the repeated pattern across
 * `resolveWrapPending()`, `print()`, `executeLineFeed()`, and the ESC IND/NEL handlers.
 * The fill is only performed when `stamp.bg` is non-transparent.
 *
 * @param top     Zero-based index of the top row of the scrolling region.
 * @param bottom  Zero-based index of the bottom row of the scrolling region.
 *
 * @note READER THREAD only.
 *
 * @see resolveWrapPending()
 * @see executeLineFeed()
 */
void Video::scrollUpAndFill (int top, int bottom) noexcept
{
    grid.scrollUp (top, bottom);

    if (stamp.bg.getAlpha() > 0)
    {
        jam::Cell* row { grid.getWritePointer (bottom) };
        const jam::Cell fill { jam::Cell::erase (stamp.bg) };
        const int numCols { cols.load (std::memory_order_relaxed) };

        for (int col { 0 }; col < numCols; ++col)
            row[col] = fill;
    }
}

/**
 * @brief Scrolls the region down one line and fills the top row with the current background.
 *
 * Single-row scroll+fill helper for the reverse-index (RI) path.
 * The fill is only performed when `stamp.bg` is non-transparent.
 *
 * @param top     Zero-based index of the top row of the scrolling region.
 * @param bottom  Zero-based index of the bottom row of the scrolling region.
 *
 * @note READER THREAD only.
 *
 * @see escDispatchNoIntermediate() — RI handler
 */
void Video::scrollDownAndFill (int top, int bottom) noexcept
{
    grid.scrollDown (top, bottom);

    if (stamp.bg.getAlpha() > 0)
    {
        jam::Cell* row { grid.getWritePointer (top) };
        const jam::Cell fill { jam::Cell::erase (stamp.bg) };
        const int numCols { cols.load (std::memory_order_relaxed) };

        for (int col { 0 }; col < numCols; ++col)
            row[col] = fill;
    }
}

// ============================================================================
// VT Handler: print
// ============================================================================

/**
 * @brief Resolves a pending line wrap before writing a new character.
 *
 * When `wrapPending` is true and a new printable codepoint arrives,
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
 */
void Video::resolveWrapPending (ActiveScreen scr) noexcept
{
    if (autoWrap)
    {
        const int row { cursorRow[static_cast<int> (scr)] };
        const int scrollBot { activeScrollBottom() };
        const int vRows { this->visibleRows.load (std::memory_order_relaxed) };
        const int sTop { this->scrollTop[static_cast<int> (scr)] };

        if (row == scrollBot)
        {
            scrollUpAndFill (sTop, scrollBot);
        }
        else if (row > scrollBot)
        {
            cursorRow[static_cast<int> (scr)] = juce::jmin (row + 1, vRows - 1);
        }
        else
        {
            cursorRow[static_cast<int> (scr)] = row + 1;
        }

        cursorCol[static_cast<int> (scr)] = 0;
    }

    wrapPending[static_cast<int> (scr)] = false;
}

/**
 * @brief Writes a Unicode codepoint to the active screen at the cursor position.
 *
 * This is the primary character output function.  It handles two distinct
 * cases based on the Unicode grapheme segmentation result:
 *
 * @par Case 1 — Grapheme cluster extension (`segResult.addToCurrentCell()`)
 * The codepoint is a combining character, variation selector, or other
 * non-spacing mark that extends the previous grapheme cluster.
 * Grapheme cluster extension deferred — Grid needs a grapheme sidecar.
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
void Video::print (uint32_t codepoint) noexcept
{
    const auto scr { activeScreen };
    const auto props { charPropsFor (codepoint) };
    const auto segResult { graphemeSegmentationStep (graphemeState, props) };

    graphemeState = segResult;

    if (segResult.addToCurrentCell())
    {
        // Grapheme cluster extension deferred — Grid needs a grapheme sidecar.
    }
    else
    {
        const int rawWidth { props.width() };
        const int cellWidth { rawWidth < 1 ? 1 : rawWidth };
        const int numCols { this->cols.load (std::memory_order_relaxed) };

        if (wrapPending[static_cast<int> (scr)])
        {
            resolveWrapPending (scr);
        }

        const int row { cursorRow[static_cast<int> (scr)] };
        const int col { cursorCol[static_cast<int> (scr)] };

        if (cellWidth == 2 and col + 2 > numCols)
        {
            if (autoWrap)
            {
                const int scrollBot { activeScrollBottom() };
                const int vRows { this->visibleRows.load (std::memory_order_relaxed) };
                const int sTop { this->scrollTop[static_cast<int> (scr)] };

                if (row == scrollBot)
                {
                    scrollUpAndFill (sTop, scrollBot);
                }
                else if (row > scrollBot)
                {
                    cursorRow[static_cast<int> (scr)] = juce::jmin (row + 1, vRows - 1);
                }
                else
                {
                    cursorRow[static_cast<int> (scr)] = row + 1;
                }

                cursorCol[static_cast<int> (scr)] = 0;
                wrapPending[static_cast<int> (scr)] = false;
            }
        }

        const int writeRow { cursorRow[static_cast<int> (scr)] };
        const int writeCol { cursorCol[static_cast<int> (scr)] };

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

        // Grid needs a per-row link ID sidecar for OSC 8 hyperlink stamping.

        *grid.getWritePointer (writeRow, writeCol) = cell;

        lastGraphicChar = codepoint;

        if (cellWidth == 2 and writeCol + 1 < numCols)
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

        if (writeCol + cellWidth >= numCols)
        {
            wrapPending[static_cast<int> (scr)] = true;
        }
        else
        {
            cursorCol[static_cast<int> (scr)] = writeCol + cellWidth;
        }
    }
}

// ============================================================================
// VT Handler: execute (C0 control codes)
// ============================================================================

/**
 * @brief Performs a line feed, advancing the cursor or scrolling the region.
 *
 * Calls `scrollUpAndFill()` if at scroll bottom, delegates cursor movement to
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
 * @see applyControlCode()
 * @see cursorGoToNextLine()
 */
void Video::executeLineFeed (ActiveScreen scr) noexcept
{
    const int scrollBot { activeScrollBottom() };
    const int vRows { this->visibleRows.load (std::memory_order_relaxed) };
    const int cRow { this->cursorRow[static_cast<int> (scr)] };
    const int sTop { this->scrollTop[static_cast<int> (scr)] };

    if (cRow == scrollBot)
    {
        scrollUpAndFill (sTop, scrollBot);
    }

    cursorGoToNextLine (scr, scrollBot, vRows);

    if (events.contains (ID::extendOutputBlock)) events.get (ID::extendOutputBlock, int (cursorRow[static_cast<int> (scr)]));
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
 * | 0x07 | BEL  | Fires the `"bell"` event asynchronously on the message thread |
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
 */
void Video::applyControlCode (uint8_t controlByte) noexcept
{
    const auto scr { activeScreen };
    const int numCols { this->cols.load (std::memory_order_relaxed) };

    switch (controlByte)
    {
        case 0x07:
            if (events.contains (ID::bell))
                juce::MessageManager::callAsync ([this] { /* MESSAGE THREAD */ events.get (ID::bell); });
            break;

        case 0x08:
            if (cursorCol[static_cast<int> (scr)] > 0)
            {
                cursorCol[static_cast<int> (scr)] = cursorCol[static_cast<int> (scr)] - 1;
                wrapPending[static_cast<int> (scr)] = false;
            }
            break;

        case 0x09:
        {
            const int nextTab { nextTabStop (scr, numCols) };
            cursorCol[static_cast<int> (scr)] = nextTab;
            wrapPending[static_cast<int> (scr)] = false;
            break;
        }

        case 0x0A:
        case 0x0B:
        case 0x0C:
            executeLineFeed (scr);
            break;

        case 0x0D:
            cursorCol[static_cast<int> (scr)] = 0;
            wrapPending[static_cast<int> (scr)] = false;
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
void Video::sendResponse (const char* resp) noexcept
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
 * @brief Delivers all queued response bytes to the host via the `"writeToHost"` event.
 *
 * Called by the owner (Session) after each `process()` invocation.  If
 * `responseLen > 0` and the `"writeToHost"` handler is registered in the events
 * map, the entire `responseBuf` is passed to the handler and `responseLen` is
 * reset to zero.
 *
 * @note READER THREAD only.
 *
 * @see sendResponse()
 * @see Processor::events
 */
void Video::flushResponses() noexcept
{
    if (responseLen > 0 and events.contains (ID::writeToHost))
    {
        events.get (ID::writeToHost, static_cast<const char*> (responseBuf), int (responseLen));
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
void Video::resetModes() noexcept
{
    originMode = false;
    autoWrap = true;
    applicationCursor = false;
    bracketedPaste = false;
    insertMode = false;
    mouseTracking = false;
    mouseMotionTracking = false;
    mouseAllTracking = false;
    mouseSgr = false;
    focusEvents = false;
    applicationKeypad = false;
    cursorVisible[static_cast<int> (normal)] = true;
    reverseVideo = false;

    keyboardFlags[static_cast<int> (normal)] = 0;
    keyboardFlags[static_cast<int> (alternate)] = 0;
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
void Video::reset() noexcept
{
    activeScreen = normal;
    grid.setScreen (false);
    resetCursor (cols.load (std::memory_order_relaxed));
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
// Pen ops
// ============================================================================

/**
 * @brief Resets the active pen to default attributes.
 *
 * Replaces `pen` with a default-constructed `jam::Cell`, which has no style
 * flags set and uses the terminal's default foreground and background colours.
 * Called by `reset()` and by SGR 0 (reset all attributes) in `applySGR()`.
 *
 * @note READER THREAD only.
 *
 * @see pen
 * @see applySGR()
 */
void Video::resetPen() noexcept
{
    pen = jam::Cell {};
}

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Terminal
