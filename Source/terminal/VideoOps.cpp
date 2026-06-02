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

namespace terminal
{
/*____________________________________________________________________________*/

// ============================================================================
// New Ops: Cursor
// ============================================================================

void Video::resetCursor (cell cols) noexcept
{
    cursorRow     = 0_cell;
    cursorCol     = 0_cell;
    cursorVisible = true;
    wrapPending   = false;
    scrollTop     = 0_cell;
    scrollBottom  = 0_cell;
    initializeTabStops (cols.value);
}

void Video::cursorMoveUp (int count) noexcept
{
    const int top    { scrollTop.value };
    const int bottom { effectiveScrollBottom (visibleRows).value };
    const int row    { cursorRow.value };
    const bool withinMargins { row >= top and row <= bottom };
    const int clampTop { withinMargins ? top : 0 };
    cursorRow   = cell (juce::jmax (clampTop, row - count));
    wrapPending = false;
}

void Video::cursorMoveDown (int count, cell bottom) noexcept
{
    const int row { cursorRow.value };
    cursorRow   = cell (juce::jmin (bottom.value, row + count));
    wrapPending = false;
}

void Video::cursorMoveForward (int count, cell cols) noexcept
{
    const int col { cursorCol.value };
    cursorCol   = cell (juce::jmin (cols.value - 1, col + count));
    wrapPending = false;
}

void Video::cursorMoveBackward (int count) noexcept
{
    const int col { cursorCol.value };
    cursorCol   = cell (juce::jmax (0, col - count));
    wrapPending = false;
}

void Video::cursorSetPosition (cell row, cell col, cell cols, cell visibleRows) noexcept
{
    cursorRow   = cell (juce::jlimit (0, visibleRows.value - 1, row.value));
    cursorCol   = cell (juce::jlimit (0, cols.value - 1, col.value));
    wrapPending = false;
}

void Video::cursorSetPositionInOrigin (cell row, cell col, cell cols, cell visibleRows) noexcept
{
    const int top    { scrollTop.value };
    const int bottom { effectiveScrollBottom (visibleRows).value };
    cursorRow   = cell (juce::jlimit (top, bottom, row.value + top));
    cursorCol   = cell (juce::jlimit (0, cols.value - 1, col.value));
    wrapPending = false;
}

bool Video::cursorGoToNextLine (cell bottom, cell visibleRows) noexcept
{
    wrapPending = false;
    const int row { cursorRow.value };
    bool moved { false };

    if (row < bottom.value)
    {
        cursorRow = cell (row + 1);
        moved = true;
    }
    else if (row > bottom.value)
    {
        cursorRow = cell (juce::jmin (row + 1, visibleRows.value - 1));
        moved = true;
    }

    return moved;
}

void Video::cursorClamp (cell cols, cell visibleRows) noexcept
{
    const int col { cursorCol.value };
    const int row { cursorRow.value };
    cursorCol = cell (juce::jlimit (0, cols.value - 1, col));
    cursorRow = cell (juce::jlimit (0, visibleRows.value - 1, row));
}

void Video::cursorSetScrollRegion (cell top, cell bottom) noexcept
{
    scrollTop    = top;
    scrollBottom = bottom;
}

void Video::cursorResetScrollRegion() noexcept
{
    scrollTop    = 0_cell;
    scrollBottom = 0_cell;
}

cell Video::effectiveScrollBottom (cell visibleRows) const noexcept
{
    const int sb { scrollBottom.value };
    return (sb > 0) ? scrollBottom : cell (visibleRows.value - 1);
}

cell Video::effectiveClampBottom() const noexcept
{
    const int row { cursorRow.value };
    const int top { scrollTop.value };
    const bool withinMargins { row >= top and row <= activeScrollBottom().value };
    return withinMargins ? activeScrollBottom() : cell (visibleRows.value - 1);
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

void Video::initializeTabStops (int numCols) noexcept
{
    tabStops.assign (static_cast<size_t> (numCols), 0);

    for (int col { DEFAULT_TAB_WIDTH }; col < numCols; col += DEFAULT_TAB_WIDTH)
    {
        tabStops.at (static_cast<size_t> (col)) = 1;
    }
}

cell Video::nextTabStop (cell cols) noexcept
{
    int nextTab { cursorCol.value + 1 };

    while (nextTab < cols.value)
    {
        if (nextTab < static_cast<int> (tabStops.size()) and tabStops.at (static_cast<size_t> (nextTab)) != 0)
        {
            break;
        }

        ++nextTab;
    }

    return cell (juce::jmin (nextTab, cols.value - 1));
}

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
} // namespace terminal
