/**
 * @file Video.cpp
 * @brief Video constructor, setWinsize, calc, cached geometry helpers, and ground-state VT handlers.
 *
 * This translation unit implements:
 *
 * - Construction, dimension update (`setWinsize()`), and cached geometry (`calc()`).
 * - `setWinsize()` and `setCellSize()` — cross-thread dimension setters.
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
 * @see Grid         — row-stride storage (Block<Row>[2])
 * @see CharProps    — Unicode character property queries used by print()
 */

#include "Video.h"

namespace terminal
{
/*____________________________________________________________________________*/


// ============================================================================
// Public API
// ============================================================================

/**
 * @brief Constructs the Video, allocates the owned Buffer and builds Block views.
 *
 * Allocates a 2-channel Buffer (normal + alternate) with `rows` visible rows
 * and `cols` columns.  Builds per-channel Block views into the buffer.
 * Internal terminal state is initialised to VT power-on defaults
 * (cursor at home, autoWrap on, cursor visible).
 *
 * The constructor does **not** call `calc()`.  The owner must call `setWinsize()`
 * after construction to synchronise internal geometry before the first process().
 *
 * @param dims    Terminal dimensions in cells.
 * @param events  Events map owned by Processor.  Video fires events through this map.
 *
 * @note MESSAGE THREAD — called before the reader thread starts.
 *
 * @see calc()
 * @see setWinsize()
 * @see Video.h
 */
Video::Video (jam::Cell::Rectangle dims,
              jam::Function::Map<juce::Identifier, void>& events) noexcept
    : events (events)
{
    buffer.setSize (2, dims.getHeight().value, dims.getWidth().value);
    blocks.at (0) = jam::Block<jam::Row> (buffer, 0);
    blocks.at (1) = jam::Block<jam::Row> (buffer, 1);
    rowTouched.allocate (static_cast<size_t> (dims.getHeight().value), true);
}

/**
 * @brief Marks the pen style cache dirty and recomputes cached geometry.
 *
 * Sets penStyleDirty so that currentStyleId() will re-query the Stamp table
 * on the next cell write.  Must be called after construction and after every
 * `setWinsize()`.  Also called internally by `cursorSetScrollRegion()` and
 * `cursorResetScrollRegion()`.
 *
 * @note READER THREAD only.
 *
 * @see setWinsize()
 */
void Video::calc() noexcept
{
    penStyleDirty = true;
}

/**
 * @brief Resizes the owned buffer and resets cursor, scroll region, and tab stops.
 *
 * Resizes the owned 2-channel Buffer to newRows × newCols, rebuilds per-channel
 * Block views, reads ring heads from the rebuilt blocks, resets geometry state,
 * and calls calc() to synchronise internal cached geometry.
 *
 * Called by Processor::prepare() from Session's Resizer stop trigger while
 * processing is suspended (message-thread-safe under suspension), and directly
 * for cold-start initial sizing.
 *
 * @param dims  Terminal dimensions in cells.
 *
 * @note MESSAGE THREAD — called while processing is suspended.
 */
void Video::setWinsize (jam::Cell::Rectangle dims) noexcept
{
    cols        = dims.getWidth();
    visibleRows = dims.getHeight();
    wrapPending = false;
    cursorClamp (dims.getWidth(), dims.getHeight());
    cursorResetScrollRegion();
    initializeTabStops (dims.getWidth().value);

    buffer.setSize (2, dims.getHeight().value, dims.getWidth().value);
    blocks.at (0) = jam::Block<jam::Row> (buffer, 0);
    blocks.at (1) = jam::Block<jam::Row> (buffer, 1);
    rowTouched.allocate (static_cast<size_t> (dims.getHeight().value), true);

    calc();
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

/**
 * @brief Zeros all rows of the specified channel through the active blocks.
 *
 * Called by Processor's clearBuffer event handler after the screen is cleared.
 *
 * @param screen  Channel index (0 = normal, 1 = alternate).
 * @note MESSAGE THREAD.
 */
void Video::clearChannel (int screen) noexcept
{
    // Buffer is flat — head is always 0.
    const int numRows { blocks.at (static_cast<size_t> (screen)).getNumRows() };

    for (int r { 0 }; r < numRows; ++r)
        blocks.at (static_cast<size_t> (screen)).clear (r, 0);
}

void Video::flush() noexcept
{
    events.get (id::activeScreen, int (activeScreen));

    const CursorState cs { cursorRow.value, cursorCol.value, cursorVisible ? 1 : 0, 0 };
    events.get (id::cursor, cs.pack());

    events.get (id::applicationCursor,   bool (applicationCursor));
    events.get (id::bracketedPaste,      bool (bracketedPaste));
    events.get (id::mouseTracking,       bool (mouseTracking));
    events.get (id::mouseMotionTracking, bool (mouseMotionTracking));
    events.get (id::mouseAllTracking,    bool (mouseAllTracking));
    events.get (id::focusEvents,         bool (focusEvents));
    events.get (id::win32InputMode,      bool (win32InputMode));

    // Mark dirty rows in State via events — fires before screenDirty so Processor
    // row-dirty flags are set before the repaint path runs.
    if (events.contains (id::rowDirty))
    {
        for (int r { 0 }; r < visibleRows.value; ++r)
        {
            if (rowTouched[r])
            {
                events.get (id::rowDirty, r);
                rowTouched[r] = false;
            }
        }
    }

    if (events.contains (id::screenDirty))
        events.get (id::screenDirty);
}

void Video::setCursor (CursorState cs) noexcept
{
    cursorRow     = cell (cs.row);
    cursorCol     = cell (cs.col);
    cursorVisible = cs.visible != 0;
    keyboardFlags = static_cast<uint32_t> (cs.kbFlags);
}

void Video::setWrapPending (bool pending) noexcept
{
    wrapPending = pending;
}

/**
 * @brief Returns a mutable pointer to the named mode flag, or nullptr if unknown.
 *
 * Single SSOT for mode flag lookup.  Both `getMode()` and `setMode()` delegate
 * here, eliminating the two duplicate tables that were previously required.
 * O(n) over 13 entries; negligible cost on the reader thread.
 *
 * @param id  A terminal::ID mode identifier.
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
        { id::originMode,          &originMode          },
        { id::autoWrap,            &autoWrap            },
        { id::applicationCursor,   &applicationCursor   },
        { id::bracketedPaste,      &bracketedPaste      },
        { id::insertMode,          &insertMode          },
        { id::mouseTracking,       &mouseTracking       },
        { id::mouseMotionTracking, &mouseMotionTracking },
        { id::mouseAllTracking,    &mouseAllTracking    },
        { id::mouseSgr,            &mouseSgr            },
        { id::focusEvents,         &focusEvents         },
        { id::applicationKeypad,   &applicationKeypad   },
        { id::reverseVideo,        &reverseVideo        },
        { id::win32InputMode,      &win32InputMode      },
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
 * @param id  A terminal::ID mode identifier.
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
 * @param id     A terminal::ID mode identifier.
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
cell Video::activeScrollBottom() const noexcept
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
void Video::scrollUpAndFill (int top, int bottom, int count) noexcept
{
    const int clampedCount { juce::jmin (count, bottom - top + 1) };

    if (clampedCount > 0)
    {
        const bool isFullScreen { top == 0 and bottom == visibleRows.value - 1 };

        const int scr { activeScreen };

        if (isFullScreen)
        {
            for (int n { 0 }; n < clampedCount; ++n)
            {
                // Commit departing row (logical row 0) before shifting.
                if (events.contains (id::pushLine))
                {
                    events.get (id::pushLine, int (scr), 0);
                }

                // Shift all rows up by 1 — row[r] = row[r+1].
                for (int r { 0 }; r < bottom; ++r)
                    blocks.at (static_cast<size_t> (scr)).copyRow (r, r + 1, 0);

                // Clear the vacated bottom row.
                blocks.at (static_cast<size_t> (scr)).clear (bottom, 0);
            }
        }
        else
        {
            for (int n { 0 }; n < clampedCount; ++n)
            {
                for (int r { top }; r < bottom; ++r)
                    blocks.at (static_cast<size_t> (scr)).copyRow (r, r + 1, 0);

                blocks.at (static_cast<size_t> (scr)).clear (bottom, 0);
            }
        }

        // Mark all rows in the scrolled region as touched.
        for (int r { top }; r <= bottom; ++r)
            rowTouched[r] = true;

        if (top == 0 and events.contains (id::scrollUp))
            events.get (id::scrollUp, int (clampedCount));

        if (penBg.getAlpha() > 0)
        {
            const jam::Cell fill { jam::Cell::erase (eraseStyleId()) };
            const int numCols { cols.value };

            for (int r { bottom - clampedCount + 1 }; r <= bottom; ++r)
            {
                jam::Row* const row { blocks.at (static_cast<size_t> (scr)).getWritePointer (r, 0) };

                for (int col { 0 }; col < numCols; ++col)
                    row->cells[col] = fill;

                row->usedCols = 0;
                row->flags    = 0;
            }
        }
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
    const int scr { activeScreen };
    const bool isFullScreen { top == 0 and bottom == visibleRows.value - 1 };

    if (isFullScreen)
    {
        // Flat buffer: shift all rows down by 1 — row[r] = row[r-1].
        for (int r { bottom }; r > top; --r)
            blocks.at (static_cast<size_t> (scr)).copyRow (r, r - 1, 0);

        // Clear the vacated top row.
        blocks.at (static_cast<size_t> (scr)).clear (0, 0);

        if (events.contains (id::scrollUp))
            events.get (id::scrollUp, 0);
    }
    else
    {
        for (int r { bottom }; r > top; --r)
            blocks.at (static_cast<size_t> (scr)).copyRow (r, r - 1, 0);

        blocks.at (static_cast<size_t> (scr)).clear (top, 0);
    }

    if (penBg.getAlpha() > 0)
    {
        jam::Row* const row { blocks.at (static_cast<size_t> (scr)).getWritePointer (top, 0) };
        const jam::Cell fill { jam::Cell::erase (eraseStyleId()) };
        const int numCols { cols.value };

        for (int col { 0 }; col < numCols; ++col)
            row->cells[col] = fill;

        row->usedCols = 0;
        row->flags    = 0;
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
        const int  row       { cursorRow.value };
        const cell scrollBot { activeScrollBottom() };
        const int  vRows     { visibleRows.value };
        const int  sTop      { scrollTop.value };

        {
            jam::Row* const completedRow { blocks.at (static_cast<size_t> (activeScreen)).getWritePointer (row, 0) };
            completedRow->flags |= jam::Row::flexWrap;
        }

        if (row == scrollBot.value)
        {
            scrollUpAndFill (sTop, scrollBot.value);
        }
        else if (row > scrollBot.value)
        {
            cursorRow = cell (juce::jmin (row + 1, vRows - 1));
        }
        else
        {
            cursorRow = cell (row + 1);
        }

        cursorCol = 0_cell;
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
        jam::Cell* const baseCell { &blocks.at (static_cast<size_t> (scr)).getWritePointer (lastWriteRow, 0)->cells[lastWriteCol] };

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
        const int rawWidth  { props.width() };
        const int charWidth { rawWidth < 1 ? 1 : rawWidth };
        const int numCols   { cols.value };

        if (wrapPending)
        {
            resolveWrapPending (scr);
        }

        const int row { cursorRow.value };
        const int col { cursorCol.value };

        if (charWidth == 2 and col + 2 > numCols)
        {
            if (autoWrap)
            {
                const cell scrollBot { activeScrollBottom() };
                const int  vRows     { visibleRows.value };
                const int  sTop      { scrollTop.value };

                if (row == scrollBot.value)
                {
                    scrollUpAndFill (sTop, scrollBot.value);
                }
                else if (row > scrollBot.value)
                {
                    cursorRow = cell (juce::jmin (row + 1, vRows - 1));
                }
                else
                {
                    cursorRow = cell (row + 1);
                }

                cursorCol   = 0_cell;
                wrapPending = false;
            }
        }

        const int writeRow { cursorRow.value };
        const int writeCol { cursorCol.value };

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

        const jam::Cell glyph { jam::Cell::make (cp, jam::Cell::CONTENT_CODEPOINT, wideHint, sid) };

        jam::Row* const writeRowPtr { blocks.at (static_cast<size_t> (scr)).getWritePointer (writeRow, 0) };
        writeRowPtr->cells[writeCol] = glyph;
        writeRowPtr->usedCols = static_cast<uint16_t> (juce::jmax (static_cast<int> (writeRowPtr->usedCols), writeCol + charWidth));
        rowTouched[writeRow] = true;

        // Stamp consecutive blanks as FLEX_GAP — elastic whitespace for reflow.
        if (cp == 0x20 and writeCol > 0)
        {
            auto& prev { writeRowPtr->cells[writeCol - 1] };

            if (prev.codepoint() == 0x20)
            {
                if (prev.contentTag() != jam::Cell::FLEX_GAP)
                    prev = jam::Cell::make (0x20, jam::Cell::FLEX_GAP, jam::Cell::NARROW, prev.styleId());

                writeRowPtr->cells[writeCol] = jam::Cell::make (0x20, jam::Cell::FLEX_GAP, jam::Cell::NARROW, sid);
                writeRowPtr->flags |= jam::Row::justify;
            }
        }

        lastGraphicChar = codepoint;
        lastWriteRow    = writeRow;
        lastWriteCol    = writeCol;

        if (charWidth == 2 and writeCol + 1 < numCols)
        {
            const jam::Cell cont { jam::Cell::make (0, jam::Cell::CONTENT_CODEPOINT,
                                                    jam::Cell::SPACER_TAIL, sid) };
            writeRowPtr->cells[writeCol + 1] = cont;
        }

        if (writeCol + charWidth >= numCols)
        {
            wrapPending = true;
        }
        else
        {
            cursorCol = cell (writeCol + charWidth);
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
    const cell scrollBot { activeScrollBottom() };
    const int  cRow      { cursorRow.value };
    const int  sTop      { scrollTop.value };

    if (cRow == scrollBot.value)
        scrollUpAndFill (sTop, scrollBot.value);

    cursorGoToNextLine (scrollBot, visibleRows);

    if (events.contains (id::extendOutputBlock)) events.get (id::extendOutputBlock, cursorRow.value);
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

    switch (controlByte)
    {
        case 0x07:
            if (events.contains (id::bell))
                juce::MessageManager::callAsync ([this] { /* MESSAGE THREAD */ events.get (id::bell); });
            break;

        case 0x08:
            if (cursorCol > cell (0))
            {
                --cursorCol;
                wrapPending = false;
            }
            break;

        case 0x09:
        {
            cursorCol   = nextTabStop (cols);
            wrapPending = false;
            break;
        }

        case 0x0A:
        case 0x0B:
        case 0x0C:
            executeLineFeed (scr);
            break;

        case 0x0D:
            cursorCol   = 0_cell;
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
    if (responseLen > 0 and events.contains (id::writeToHost))
    {
        events.get (id::writeToHost, static_cast<const char*> (responseBuf), int (responseLen));
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
 * 6. Fires `id::clearBuffer` event to clear the normal screen.
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
    activeScreen = Map::Screen::normal;
    resetCursor (cols);
    resetModes();
    resetPen();
    useLineDrawing = false;
    g0LineDrawing  = false;
    g1LineDrawing  = false;
    activeLinkId   = 0;

    events.get (id::clearBuffer, int (Map::Screen::normal));

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
} // namespace terminal
