/**
 * @file VideoOps.cpp
 * @brief Cursor movement primitives, tab stop management, and terminal reset for the VT parser.
 *
 * This file implements the low-level cursor and tab-stop operations of the
 * Video class.  These are the building blocks used by the CSI dispatch
 * handlers in VideoCSI.cpp and by the ESC dispatch handlers in VideoESC.cpp.
 *
 * ## Cursor movement primitives
 *
 * All movement helpers operate on a screen index (`int`, 0 = normal, 1 = alternate)
 * and clamp their results to valid bounds.  They always clear the wrap-pending
 * flag after moving, because a cursor that has been explicitly repositioned
 * should not trigger a deferred wrap on the next printed character.
 *
 * | Method                    | VT sequences that use it          |
 * |---------------------------|-----------------------------------|
 * | `cursorMoveUp()`          | CUU (CSI A), CPL (CSI F)          |
 * | `cursorMoveDown()`        | CUD (CSI B), CNL (CSI E)          |
 * | `cursorMoveForward()`     | CUF (CSI C)                       |
 * | `cursorMoveBackward()`    | CUB (CSI D)                       |
 * | `cursorSetPosition()`     | CUP (CSI H), HVP (CSI f)          |
 * | `cursorSetPositionInOrigin()` | CUP / HVP with DECOM active   |
 * | `cursorGoToNextLine()`    | LF, VT, FF, IND (ESC D), NEL      |
 * | `cursorClamp()`           | resize()                          |
 * | `cursorSetScrollRegion()` | DECSTBM (CSI r)                   |
 * | `cursorResetScrollRegion()` | reset(), alternate screen switch |
 * | `effectiveScrollBottom()` | all scroll-region-aware operations |
 *
 * ## Tab stop management
 *
 * Tab stops are stored as a `std::vector<char>` indexed by column.  A non-zero
 * value at index `c` marks column `c` as a tab stop.  The default layout places
 * stops every 8 columns (columns 8, 16, 24, …), matching the VT100 default.
 *
 * | Method              | VT sequence              | Effect                        |
 * |---------------------|--------------------------|-------------------------------|
 * | `initializeTabStops()` | reset(), resize()     | Set stops every 8 columns     |
 * | `nextTabStop()`     | HT (0x09)                | Advance cursor to next stop   |
 * | `setTabStop()`      | HTS (ESC H)              | Set stop at cursor column     |
 * | `clearTabStop()`    | TBC CSI 0 g              | Clear stop at cursor column   |
 * | `clearAllTabStops()` | TBC CSI 3 g             | Clear all stops               |
 *
 * ## Cursor save / restore (DECSC / DECRC)
 *
 * ESC 7 (DECSC) saves the cursor position and the active pen into `stamp`.
 * ESC 8 (DECRC) restores them.  These are handled in VideoESC.cpp using the
 * `pen` and `stamp` members declared in Video.h; no dedicated methods live
 * here for that operation.
 *
 * ## Terminal reset (RIS)
 *
 * ESC c (RIS — Reset to Initial State) triggers a full terminal reset:
 * `resetCursor()` (this file) resets cursor state and tab stops for both
 * screens; `resetModes()` and `resetPen()` (VideoCSI.cpp) reset mode flags
 * and drawing attributes.
 *
 * @note All functions in this file run on the READER THREAD only.
 *
 * @see Grid   — screen buffer whose geometry constrains cursor movement
 */

#include "Video.h"
#include "Grid.h"

namespace Terminal
{ /*____________________________________________________________________________*/

// ============================================================================
// New Ops: Cursor
// ============================================================================

/**
 * @brief Resets cursor state for both screens and reinitialises tab stops.
 *
 * Called by `reset()` (which is triggered by RIS, ESC c) and by `resize()`.
 * Resets cursor position, visibility, wrap-pending flag, and scroll region for
 * both the normal and alternate screens, then rebuilds the tab stop table for
 * the new column count.
 *
 * @param numCols  Current terminal column count.  Passed to `initializeTabStops()`
 *                 to size the tab stop vector and place stops every 8 columns.
 *
 * @note READER THREAD only.
 *
 * @see initializeTabStops()    — rebuilds the tab stop vector
 * @see reset()                 — full terminal reset (RIS) entry point
 */
void Video::resetCursor (int numCols) noexcept
{
    cursorRow     = 0_cell;
    cursorCol     = 0_cell;
    cursorVisible = true;
    wrapPending   = false;
    scrollTop     = 0_cell;
    scrollBottom  = 0_cell;
    initializeTabStops (numCols);
}

/**
 * @brief Moves the cursor up by `count` rows, clamped to the appropriate upper bound.
 *
 * Used by CUU (`CSI Pn A`) and CPL (`CSI Pn F`).  The clamping boundary
 * depends on whether the cursor is within the scroll region:
 * - Within margins (row >= scrollTop and row <= scrollBottom): clamp to scrollTop.
 * - Outside margins: clamp to row 0.
 *
 * @par Clamping
 * @code
 * clampTop = (row >= scrollTop and row <= scrollBottom) ? scrollTop : 0
 * newRow   = max (clampTop, cursorRow - count)
 * @endcode
 *
 * @param count  Number of rows to move up (>= 1).
 *
 * @note READER THREAD only.
 * @note Always clears the wrap-pending flag.
 *
 * @see cursorMoveDown()  — complementary downward movement
 */
void Video::cursorMoveUp (int count) noexcept
{
    const int top    { scrollTop.value };
    const int bottom { effectiveScrollBottom (visibleRows.value) };
    const int row    { cursorRow.value };
    const bool withinMargins { row >= top and row <= bottom };
    const int clampTop { withinMargins ? top : 0 };
    cursorRow   = cell (juce::jmax (clampTop, row - count));
    wrapPending = false;
}

/**
 * @brief Moves the cursor down by `count` rows, clamped to `bottom`.
 *
 * Used by CUD (`CSI Pn B`) and CNL (`CSI Pn E`).  The cursor never moves
 * below `bottom`; the caller is responsible for passing the correct bottom
 * bound — `scrollBottom` when the cursor is within the scroll region, or
 * `visibleRows - 1` when the cursor is outside the region.
 *
 * @par Clamping
 * @code
 * newRow = min (bottom, cursorRow + count)
 * @endcode
 *
 * @param count  Number of rows to move down (>= 1).
 * @param bottom Zero-based index of the last row the cursor may reach.
 *
 * @note READER THREAD only.
 * @note Always clears the wrap-pending flag.
 *
 * @see cursorMoveUp()           — complementary upward movement
 * @see effectiveScrollBottom()  — computes the effective scroll region bottom
 */
void Video::cursorMoveDown (int count, int bottom) noexcept
{
    const int row { cursorRow.value };
    cursorRow   = cell (juce::jmin (bottom, row + count));
    wrapPending = false;
}

/**
 * @brief Moves the cursor right by `count` columns, clamped to `cols - 1`.
 *
 * Used by CUF (`CSI Pn C`).  The cursor never moves past the right margin
 * (`cols - 1`).
 *
 * @par Clamping
 * @code
 * newCol = min (cols - 1, cursorCol + count)
 * @endcode
 *
 * @param count Number of columns to move right (>= 1).
 * @param cols  Current terminal column count (right margin = cols - 1).
 *
 * @note READER THREAD only.
 * @note Always clears the wrap-pending flag.
 *
 * @see cursorMoveBackward()  — complementary leftward movement
 */
void Video::cursorMoveForward (int count, int cols) noexcept
{
    const int col { cursorCol.value };
    cursorCol   = cell (juce::jmin (cols - 1, col + count));
    wrapPending = false;
}

/**
 * @brief Moves the cursor left by `count` columns, clamped to column 0.
 *
 * Used by CUB (`CSI Pn D`).  The cursor never moves past the left margin
 * (column 0).
 *
 * @par Clamping
 * @code
 * newCol = max (0, cursorCol - count)
 * @endcode
 *
 * @param count Number of columns to move left (>= 1).
 *
 * @note READER THREAD only.
 * @note Always clears the wrap-pending flag.
 *
 * @see cursorMoveForward()  — complementary rightward movement
 */
void Video::cursorMoveBackward (int count) noexcept
{
    const int col { cursorCol.value };
    cursorCol   = cell (juce::jmax (0, col - count));
    wrapPending = false;
}

/**
 * @brief Sets the cursor to an absolute (row, col) position, clamped to screen bounds.
 *
 * Used by CUP (`CSI Pr ; Pc H`) and HVP (`CSI Pr ; Pc f`) when origin mode
 * (DECOM) is inactive.  Both row and column are clamped to the valid screen
 * area `[0, visibleRows-1]` × `[0, cols-1]`.
 *
 * @par Clamping
 * @code
 * newRow = clamp (row, 0, visibleRows - 1)
 * newCol = clamp (col, 0, cols - 1)
 * @endcode
 *
 * @param row         Zero-based target row (already converted from 1-based CSI param).
 * @param col         Zero-based target column (already converted from 1-based CSI param).
 * @param cols        Current terminal column count.
 * @param visibleRows Current terminal visible row count.
 *
 * @note READER THREAD only.
 * @note Always clears the wrap-pending flag.
 *
 * @see cursorSetPositionInOrigin()  — use this variant when DECOM is active
 */
void Video::cursorSetPosition (int row, int col, int cols, int visibleRows) noexcept
{
    cursorRow   = cell (juce::jlimit (0, visibleRows - 1, row));
    cursorCol   = cell (juce::jlimit (0, cols - 1, col));
    wrapPending = false;
}

/**
 * @brief Sets the cursor position relative to the scroll region origin (DECOM).
 *
 * Used by CUP / HVP when DEC Origin Mode (DECOM, `CSI ? 6 h`) is active.
 * In origin mode, row 0 refers to `scrollTop` rather than the top of the
 * screen.  The cursor is clamped to the scroll region vertically and to the
 * full column range horizontally.
 *
 * @par Offset and clamping
 * @code
 * effectiveRow = clamp (row + scrollTop, scrollTop, scrollBottom)
 * effectiveCol = clamp (col, 0, cols - 1)
 * @endcode
 *
 * @param row         Zero-based target row relative to `scrollTop`.
 * @param col         Zero-based target column.
 * @param cols        Current terminal column count.
 * @param visibleRows Current terminal visible row count (used to compute effective bottom).
 *
 * @note READER THREAD only.
 * @note Always clears the wrap-pending flag.
 *
 * @see cursorSetPosition()      — use this variant when DECOM is inactive
 * @see effectiveScrollBottom()  — computes the effective scroll region bottom
 */
void Video::cursorSetPositionInOrigin (int row, int col, int cols, int visibleRows) noexcept
{
    const int top    { scrollTop.value };
    const int bottom { effectiveScrollBottom (visibleRows) };
    cursorRow   = cell (juce::jlimit (top, bottom, row + top));
    cursorCol   = cell (juce::jlimit (0, cols - 1, col));
    wrapPending = false;
}

/**
 * @brief Advances the cursor to the next line, returning whether the cursor moved.
 *
 * If the cursor is strictly above `bottom`, the cursor row is incremented and
 * the function returns `true`.  If the cursor is exactly at `bottom`, the
 * function returns `false` — the caller (typically `executeLineFeed()`) is
 * responsible for triggering a scroll.  If the cursor is below `bottom`
 * (outside the scroll region), the row is advanced clamped to `visibleRows - 1`
 * and the function returns `true` (no scroll needed).
 *
 * @par Wrap-pending
 * The wrap-pending flag is always cleared before the row check, so that a
 * deferred wrap is resolved before the line-feed logic runs.
 *
 * @param bottom      Zero-based index of the last row of the scrolling region.
 * @param visibleRows Total number of visible rows in the terminal.
 *
 * @return `true`  if the cursor moved down one row.
 *         `false` if the cursor was exactly at `bottom` (scroll needed).
 *
 * @note READER THREAD only.
 *
 * @see executeLineFeed()    — calls this; scrolls the region if it returns false
 * @see resolveWrapPending() — calls this when auto-wrap fires at the right margin
 */
bool Video::cursorGoToNextLine (int bottom, int visibleRows) noexcept
{
    wrapPending = false;
    const int row { cursorRow.value };
    bool moved { false };

    if (row < bottom)
    {
        cursorRow = cell (row + 1);
        moved = true;
    }
    else if (row > bottom)
    {
        cursorRow = cell (juce::jmin (row + 1, visibleRows - 1));
        moved = true;
    }

    return moved;
}

/**
 * @brief Clamps the cursor to the valid screen area after a terminal resize.
 *
 * Called by `resize()` after the new dimensions have been written to State.
 * Ensures the cursor row and column do not exceed the new terminal bounds.
 * Applied to both screens independently by the resize handler.
 *
 * @par Clamping
 * @code
 * newCol = clamp (cursorCol, 0, cols - 1)
 * newRow = clamp (cursorRow, 0, visibleRows - 1)
 * @endcode
 *
 * @param cols        New terminal column count.
 * @param visibleRows New terminal visible row count.
 *
 * @note READER THREAD only.
 *
 * @see resize()  — calls this after updating State with new dimensions
 */
void Video::cursorClamp (int cols, int visibleRows) noexcept
{
    const int col { cursorCol.value };
    const int row { cursorRow.value };
    cursorCol = cell (juce::jlimit (0, cols - 1, col));
    cursorRow = cell (juce::jlimit (0, visibleRows - 1, row));
}

/**
 * @brief Sets the scrolling region (DECSTBM) for the specified screen.
 *
 * Corresponds to `CSI Pt ; Pb r` (DECSTBM — DEC Set Top and Bottom Margins).
 * Stores `top` and `bottom` in the per-screen arrays.  The caller is responsible
 * for moving the cursor to the home position after calling this, as required by
 * the VT specification.
 *
 * @param top    Zero-based index of the first row of the scrolling region.
 * @param bottom Zero-based index of the last row of the scrolling region.
 *
 * @note READER THREAD only.
 * @note The caller must validate that `top < bottom` before calling.
 *
 * @see cursorResetScrollRegion()  — resets the region to the full screen
 * @see effectiveScrollBottom()    — interprets a stored bottom of 0 as full-screen
 */
void Video::cursorSetScrollRegion (int top, int bottom) noexcept
{
    scrollTop    = cell (top);
    scrollBottom = cell (bottom);
}

/**
 * @brief Resets the scrolling region to the full screen height.
 *
 * Stores 0 for both `scrollTop` and `scrollBottom`.  A stored bottom of 0 is
 * the sentinel value that `effectiveScrollBottom()` interprets as
 * `visibleRows - 1`, so the scroll region effectively spans the entire screen.
 *
 * Called by `reset()` (RIS), `resize()`, and when switching between the normal
 * and alternate screen buffers.
 *
 * @note READER THREAD only.
 *
 * @see cursorSetScrollRegion()  — sets an explicit scroll region
 * @see effectiveScrollBottom()  — interprets the stored bottom value
 */
void Video::cursorResetScrollRegion() noexcept
{
    scrollTop    = 0_cell;
    scrollBottom = 0_cell;
}

/**
 * @brief Returns the effective scroll-region bottom row for the given screen.
 *
 * A stored `scrollBottom` of 0 is the sentinel for "full screen" — in that
 * case, `visibleRows - 1` is returned.  Otherwise the stored value is returned
 * directly.
 *
 * @par Sentinel convention
 * @code
 * // scrollBottom == 0 → full screen (no explicit region set)
 * effectiveBottom = (scrollBottom > 0) ? scrollBottom : visibleRows - 1;
 * @endcode
 *
 * @param visibleRows Current terminal visible row count.
 *
 * @return Zero-based index of the last row of the active scrolling region.
 *
 * @note READER THREAD only.
 *
 * @see cursorSetScrollRegion()   — sets an explicit scroll region
 * @see cursorResetScrollRegion() — resets to the sentinel value (0)
 */
int Video::effectiveScrollBottom (int visibleRows) const noexcept
{
    const int sb { scrollBottom.value };
    return (sb > 0) ? sb : visibleRows - 1;
}

/**
 * @brief Returns the effective downward clamp for cursor movement on screen `s`.
 *
 * If the cursor is within the scroll margins (row >= scrollTop and
 * row <= scrollBottom), returns `scrollBottom`.  Otherwise returns
 * `visibleRows - 1`.  Used by `moveCursorDown()` and `moveCursorNextLine()`
 * to eliminate the duplicated margin-awareness check.
 *
 * @return Zero-based index of the last row the cursor may reach moving down.
 *
 * @note READER THREAD only.
 *
 * @see moveCursorDown()      — CUD handler
 * @see moveCursorNextLine()  — CNL handler
 */
int Video::effectiveClampBottom() const noexcept
{
    const int row { cursorRow.value };
    const int top { scrollTop.value };
    const bool withinMargins { row >= top and row <= activeScrollBottom() };
    return withinMargins ? activeScrollBottom() : visibleRows.value - 1;
}

// ============================================================================
// Tab Stops
// ============================================================================

/**
 * @brief Default tab stop interval in columns (VT100 standard).
 *
 * Tab stops are placed at every column that is a multiple of this value:
 * columns 8, 16, 24, 32, … .  This matches the hardware VT100 default and
 * the POSIX terminal default (`stty tab3`).
 */
static constexpr int DEFAULT_TAB_WIDTH { 8 };

/**
 * @brief Initialises tab stops to the standard 8-column interval.
 *
 * Resizes `tabStops` to `numCols` elements (all zero), then sets a stop at
 * every column index that is a non-zero multiple of `DEFAULT_TAB_WIDTH`
 * (columns 8, 16, 24, …).  Column 0 is never a tab stop.
 *
 * Called by `resetCursor()` (which is called by `reset()` and `resize()`).
 *
 * @par Example (numCols = 24)
 * @code
 * // tabStops: [0,0,0,0,0,0,0,0, 1, 0,0,0,0,0,0,0, 1, 0,0,0,0,0,0,0]
 * //            0 1 2 3 4 5 6 7  8  9 ...            16 ...
 * @endcode
 *
 * @param numCols  Number of columns in the terminal.  The `tabStops` vector
 *                 is resized to exactly this length.
 *
 * @note READER THREAD only.
 *
 * @see nextTabStop()   — advances the cursor to the next stop
 * @see setTabStop()    — sets an individual stop (HTS)
 * @see clearAllTabStops() — clears all stops (TBC CSI 3 g)
 */
void Video::initializeTabStops (int numCols) noexcept
{
    tabStops.assign (static_cast<size_t> (numCols), 0);

    for (int col { DEFAULT_TAB_WIDTH }; col < numCols; col += DEFAULT_TAB_WIDTH)
    {
        tabStops.at (static_cast<size_t> (col)) = 1;
    }
}

/**
 * @brief Returns the column index of the next tab stop to the right of the cursor.
 *
 * Scans `tabStops` from `cursorCol + 1` rightward, returning the first column
 * with a non-zero entry.  If no tab stop exists to the right, returns
 * `cols - 1` (the right margin), matching xterm behavior.
 *
 * Called by `execute()` when a HT (Horizontal Tab, 0x09) control character
 * is received.
 *
 * @par Scan logic
 * @code
 * nextTab = cursorCol + 1;
 * while (nextTab < cols):
 *     if tabStops[nextTab] != 0: break;
 *     ++nextTab;
 * return min (nextTab, cols - 1);
 * @endcode
 *
 * @param cols  Current terminal column count (right margin = cols - 1).
 *
 * @return Zero-based column index of the next tab stop, or `cols - 1` if none.
 *
 * @note READER THREAD only.
 *
 * @see initializeTabStops()  — sets the default stop layout
 * @see setTabStop()          — adds a stop at the cursor column
 */
int Video::nextTabStop (int cols) noexcept
{
    int nextTab { cursorCol.value + 1 };

    while (nextTab < cols)
    {
        if (nextTab < static_cast<int> (tabStops.size()) and tabStops.at (static_cast<size_t> (nextTab)) != 0)
        {
            break;
        }

        ++nextTab;
    }

    return juce::jmin (nextTab, cols - 1);
}

/**
 * @brief Returns the column index of the previous tab stop to the left of the cursor.
 *
 * Scans `tabStops` from `cursorCol - 1` leftward, returning the first column
 * with a non-zero entry.  If no tab stop exists to the left, returns 0,
 * matching xterm behaviour for CBT.
 *
 * @par Scan logic
 * @code
 * prevTab = cursorCol - 1;
 * while (prevTab > 0):
 *     if tabStops[prevTab] != 0: break;
 *     --prevTab;
 * return max (prevTab, 0);
 * @endcode
 *
 * @return Zero-based column index of the previous tab stop, or 0 if none.
 *
 * @note READER THREAD only.
 *
 * @see nextTabStop()         — forward direction counterpart
 * @see initializeTabStops()  — sets the default stop layout
 */
int Video::prevTabStop() noexcept
{
    int prevTab { cursorCol.value - 1 };

    while (prevTab > 0)
    {
        if (prevTab < static_cast<int> (tabStops.size()) and tabStops.at (static_cast<size_t> (prevTab)) != 0)
        {
            break;
        }

        --prevTab;
    }

    return juce::jmax (prevTab, 0);
}

/**
 * @brief Sets a tab stop at the current cursor column (HTS — Horizontal Tab Set).
 *
 * Corresponds to ESC H (HTS).  Marks the cursor's current column as a tab stop
 * by writing 1 into `tabStops[cursorCol]`.  If the cursor column is at or
 * beyond the end of the `tabStops` vector, the operation is a no-op.
 *
 * @note READER THREAD only.
 *
 * @see clearTabStop()        — clears the stop at the cursor column (TBC CSI 0 g)
 * @see clearAllTabStops()    — clears all stops (TBC CSI 3 g)
 * @see initializeTabStops()  — resets to the default 8-column layout
 */
void Video::setTabStop() noexcept
{
    const int col { cursorCol.value };
    if (col < static_cast<int> (tabStops.size()))
    {
        tabStops.at (static_cast<size_t> (col)) = 1;
    }
}

/**
 * @brief Clears the tab stop at the current cursor column (TBC mode 0).
 *
 * Corresponds to `CSI 0 g` (TBC — Tab Clear, current column).  Writes 0 into
 * `tabStops[cursorCol]`.  If the cursor column is at or beyond the end of the
 * `tabStops` vector, the operation is a no-op.
 *
 * @note READER THREAD only.
 * @note Currently unused — verify against TBC dispatch before removal.
 *
 * @see setTabStop()       — sets a stop at the cursor column (HTS)
 * @see clearAllTabStops() — clears all stops (TBC CSI 3 g)
 */
void Video::clearTabStop() noexcept
{
    const int col { cursorCol.value };
    if (col < static_cast<int> (tabStops.size()))
    {
        tabStops.at (static_cast<size_t> (col)) = 0;
    }
}

/**
 * @brief Clears all tab stops (TBC mode 3).
 *
 * Corresponds to `CSI 3 g` (TBC — Tab Clear, all columns).  Fills the entire
 * `tabStops` vector with zeros, removing every tab stop.  After this call,
 * `nextTabStop()` will always return `cols - 1` (the right margin).
 *
 * @note READER THREAD only.
 *
 * @see clearTabStop()        — clears only the stop at the cursor column
 * @see initializeTabStops()  — restores the default 8-column layout
 */
void Video::clearAllTabStops() noexcept
{
    tabStops.assign (tabStops.size(), 0);
}

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Terminal
