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
 * @see Grid         — flat cell storage (Buffer<Cell>)
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
    wrapPending = false;
    cursorClamp (newCols, newVisibleRows);
    cursorResetScrollRegion();
    initializeTabStops (newCols);
    calc();
}

/**
 * @brief Marks the pen style cache dirty and recomputes cached geometry.
 *
 * Sets penStyleDirty so that currentStyleId() will re-query the Stamp table
 * on the next cell write.  Must be called after construction and after every
 * `resize()`.  Also called internally by `cursorSetScrollRegion()` and
 * `cursorResetScrollRegion()`.
 *
 * @note READER THREAD only.
 *
 * @see resize()
 */
void Video::calc() noexcept
{
    penStyleDirty = true;
}

/**
 * @brief Sets terminal dimensions on the reader thread.
 *
 * Writes new column and row counts to plain int members.  Called by
 * Processor::process() at batch start after consuming a pending resize —
 * all writes occur on the reader thread, so no synchronisation is needed.
 *
 * @param newCols  New terminal width in character columns.
 * @param newRows  New terminal height in visible rows.
 *
 * @note READER THREAD only.
 */
void Video::setDimensions (int newCols, int newRows) noexcept
{
    cols = newCols;
    visibleRows = newRows;
}

/**
 * @brief Sets physical cell dimensions on the reader thread.
 *
 * Writes `widthPx` and `heightPx` to plain int members.  Called by
 * Processor::process() at batch start after consuming a pending cell-size
 * change — all writes occur on the reader thread, so no synchronisation is needed.
 *
 * @param widthPx   Cell width in pixels.
 * @param heightPx  Cell height in pixels.
 *
 * @note READER THREAD only.
 */
void Video::setCellSize (int widthPx, int heightPx) noexcept
{
    cellWidth = widthPx;
    cellHeight = heightPx;
}

void Video::flushState() noexcept
{
    events.get (ID::activeScreen, int (activeScreen));
    events.get (ID::cursorRow, int (activeScreen), int (cursorRow));
    events.get (ID::cursorCol, int (activeScreen), int (cursorCol));
    events.get (ID::cursorVisible, int (activeScreen), bool (cursorVisible));

    events.get (ID::applicationCursor,   bool (applicationCursor));
    events.get (ID::bracketedPaste,      bool (bracketedPaste));
    events.get (ID::mouseTracking,       bool (mouseTracking));
    events.get (ID::mouseMotionTracking, bool (mouseMotionTracking));
    events.get (ID::mouseAllTracking,    bool (mouseAllTracking));
    events.get (ID::focusEvents,         bool (focusEvents));
    events.get (ID::win32InputMode,      bool (win32InputMode));
}

void Video::loadScreenState (int row, int col, bool visible,
                              int top, int bottom, bool wrap,
                              uint32_t kbFlags) noexcept
{
    cursorRow    = row;
    cursorCol    = col;
    cursorVisible = visible;
    scrollTop    = top;
    scrollBottom = bottom;
    wrapPending  = wrap;
    keyboardFlags = kbFlags;
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
    return effectiveScrollBottom (visibleRows);
}


// ============================================================================
// Scroll helpers
// ============================================================================

/**
 * @brief Scrolls the region up one line and fills the bottom row with the current background.
 *
 * Single-row scroll+fill helper that eliminates the repeated pattern across
 * `resolveWrapPending()`, `print()`, `executeLineFeed()`, and the ESC IND/NEL handlers.
 * The fill is only performed when `penBg` is non-transparent.
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
    grid.scrollUp (activeScreen, top, bottom);

    if (penBg.getAlpha() > 0)
    {
        jam::Cell* row { grid.getWritePointer (activeScreen, bottom) };
        const jam::Cell fill { jam::Cell::erase (eraseStyleId()) };

        for (int col { 0 }; col < cols; ++col)
            row[col] = fill;
    }
}

/**
 * @brief Scrolls the region down one line and fills the top row with the current background.
 *
 * Single-row scroll+fill helper for the reverse-index (RI) path.
 * The fill is only performed when `penBg` is non-transparent.
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
    grid.scrollDown (activeScreen, top, bottom);

    if (penBg.getAlpha() > 0)
    {
        jam::Cell* row { grid.getWritePointer (activeScreen, top) };
        const jam::Cell fill { jam::Cell::erase (eraseStyleId()) };

        for (int col { 0 }; col < cols; ++col)
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
void Video::resolveWrapPending (int /*scr*/) noexcept
{
    if (autoWrap)
    {
        const int row { cursorRow };
        const int scrollBot { activeScrollBottom() };
        const int vRows { visibleRows };
        const int sTop { scrollTop };

        if (row == scrollBot)
        {
            scrollUpAndFill (sTop, scrollBot);
        }
        else if (row > scrollBot)
        {
            cursorRow = juce::jmin (row + 1, vRows - 1);
        }
        else
        {
            cursorRow = row + 1;
        }

        cursorCol = 0;
    }

    wrapPending = false;
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
 * 3. A packed `jam::Cell` is built from the codepoint, styleId (via Stamp),
 *    and wide hint.  The cell is written to Grid via getWritePointer().
 * 4. For wide characters, a second cell with SPACER_TAIL is written
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
        jam::Cell* const baseCell { grid.getWritePointer (scr, lastWriteRow, lastWriteCol) };

        jam::Grapheme::Entry cluster {};

        if (baseCell->contentTag() == jam::Cell::CONTENT_GRAPHEME)
        {
            cluster = jam::Grapheme::getContext()->get (baseCell->codepoint());

            if (cluster.count < 8)
            {
                cluster.codepoints[cluster.count] = static_cast<char32_t> (codepoint);
                ++cluster.count;
            }
        }
        else
        {
            cluster.codepoints[0] = static_cast<char32_t> (baseCell->codepoint());
            cluster.codepoints[1] = static_cast<char32_t> (codepoint);
            cluster.count = 2;
        }

        const uint32_t graphemeIndex { static_cast<uint32_t> (jam::Grapheme::getContext()->addIfNotAlreadyThere (cluster)) };
        *baseCell = jam::Cell::make (graphemeIndex, jam::Cell::CONTENT_GRAPHEME,
                                     baseCell->wide(), baseCell->styleId());
    }
    else
    {
        const int rawWidth { props.width() };
        const int charWidth { rawWidth < 1 ? 1 : rawWidth };
        const int numCols { cols };

        if (wrapPending)
        {
            resolveWrapPending (scr);
        }

        const int row { cursorRow };
        const int col { cursorCol };

        if (charWidth == 2 and col + 2 > numCols)
        {
            if (autoWrap)
            {
                const int scrollBot { activeScrollBottom() };
                const int vRows { visibleRows };
                const int sTop { scrollTop };

                if (row == scrollBot)
                {
                    scrollUpAndFill (sTop, scrollBot);
                }
                else if (row > scrollBot)
                {
                    cursorRow = juce::jmin (row + 1, vRows - 1);
                }
                else
                {
                    cursorRow = row + 1;
                }

                cursorCol = 0;
                wrapPending = false;
            }
        }

        const int writeRow { cursorRow };
        const int writeCol { cursorCol };

        const uint32_t cp { translateCharset (codepoint, useLineDrawing) };
        const uint8_t wideHint { charWidth == 2 ? jam::Cell::WIDE : jam::Cell::NARROW };

        uint16_t sid;

        if (activeLinkId != 0)
        {
            const uint8_t linkFlags { static_cast<uint8_t> (penFlags | jam::Stamp::UNDERLINE) };
            sid = static_cast<uint16_t> (jam::Stamp::getContext()->addIfNotAlreadyThere ({ penFg, penBg, linkFlags }));
        }
        else
        {
            sid = currentStyleId();
        }

        const jam::Cell cell { jam::Cell::make (cp, jam::Cell::CONTENT_CODEPOINT, wideHint, sid) };

        *grid.getWritePointer (scr, writeRow, writeCol) = cell;

        lastGraphicChar = codepoint;
        lastWriteRow    = writeRow;
        lastWriteCol    = writeCol;

        if (charWidth == 2 and writeCol + 1 < numCols)
        {
            const jam::Cell cont { jam::Cell::make (0, jam::Cell::CONTENT_CODEPOINT,
                                                    jam::Cell::SPACER_TAIL, sid) };
            *grid.getWritePointer (scr, writeRow, writeCol + 1) = cont;
        }

        if (writeCol + charWidth >= numCols)
        {
            wrapPending = true;
        }
        else
        {
            cursorCol = writeCol + charWidth;
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
void Video::executeLineFeed (int scr) noexcept
{
    const int scrollBot { activeScrollBottom() };
    const int vRows { visibleRows };
    const int cRow { cursorRow };
    const int sTop { scrollTop };

    if (cRow == scrollBot)
    {
        scrollUpAndFill (sTop, scrollBot);
    }

    cursorGoToNextLine (scrollBot, vRows);

    if (events.contains (ID::extendOutputBlock)) events.get (ID::extendOutputBlock, int (cursorRow));
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
    const int numCols { cols };

    switch (controlByte)
    {
        case 0x07:
            if (events.contains (ID::bell))
                juce::MessageManager::callAsync ([this] { /* MESSAGE THREAD */ events.get (ID::bell); });
            break;

        case 0x08:
            if (cursorCol > 0)
            {
                cursorCol = cursorCol - 1;
                wrapPending = false;
            }
            break;

        case 0x09:
        {
            const int nextTab { nextTabStop (numCols) };
            cursorCol = nextTab;
            wrapPending = false;
            break;
        }

        case 0x0A:
        case 0x0B:
        case 0x0C:
            executeLineFeed (scr);
            break;

        case 0x0D:
            cursorCol = 0;
            wrapPending = false;
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
    cursorVisible = true;
    reverseVideo = false;

    keyboardFlags = 0;
}

/**
 * @brief Performs a full terminal reset (RIS — Reset to Initial State).
 *
 * Equivalent to the ESC c sequence.  Restores the terminal to a clean
 * power-on state:
 * 1. Resets `activeScreen` to normal.
 * 2. Homes the cursor via `resetCursor()`.
 * 3. Resets all mode flags via `resetModes()`.
 * 4. Resets the active pen via `resetPen()`.
 * 5. Disables the line-drawing charset.
 * 6. Calls `grid.clear(Screen::Map::normal)` to clear the normal screen.
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
    activeScreen = Screen::Map::normal;
    resetCursor (cols);
    resetModes();
    resetPen();
    useLineDrawing = false;
    g0LineDrawing  = false;
    g1LineDrawing  = false;
    activeLinkId   = 0;

    grid.clear (Screen::Map::normal);

    calc();
}

// ============================================================================
// Pen ops
// ============================================================================

/**
 * @brief Resets the active pen to default attributes.
 *
 * Clears penFg and penBg to alpha-zero (theme default sentinel) and sets
 * penFlags to 0.  Marks penStyleDirty so currentStyleId() re-queries the
 * Stamp table on the next cell write.  Called by `reset()` and by SGR 0.
 *
 * @note READER THREAD only.
 *
 * @see applySGR()
 */
void Video::resetPen() noexcept
{
    penFg = {};
    penBg = {};
    penFlags = 0;
    penStyleDirty = true;
}

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Terminal
