/**
 * @file GridScroll.cpp
 * @brief Grid member functions for scroll region operations.
 *
 * This file implements the scroll half of the Grid class.  All functions here
 * operate on the active screen buffer and are called exclusively on the
 * **READER THREAD**.
 *
 * ## Full-screen scroll (head-pointer path)
 *
 * When the scroll region covers the entire viewport, scrolling is O(1):
 * `scrollUp()` increments `buffer.head` by one (mod `totalRows`), which
 * advances the ring-buffer write head so that the physical row that was the
 * oldest visible row is now the new blank bottom row — no data is moved.
 * `scrollDown()` decrements `buffer.head` by one, exposing a new blank top row.
 *
 * @par Ring-buffer scroll invariant
 * @code
 * // After scrollUp(1):
 * buffer.head = (buffer.head + 1) & buffer.rowMask;
 * // The new bottom row (visibleRows - 1) is now the recycled physical row.
 * // It is cleared immediately after the head advance.
 * @endcode
 *
 * ## Sub-region scroll (memcpy path)
 *
 * When the scroll region is a strict sub-range of the viewport (i.e. the
 * terminal has set top/bottom margins via DECSTBM), the head-pointer trick
 * cannot be used because only a subset of rows should move.  Instead,
 * `scrollRegion()` delegates to `shiftRegionUp()` or `shiftRegionDown()`,
 * which use `std::memcpy` to slide rows within the region and then clear the
 * vacated rows.
 *
 * ## Scrollback
 *
 * Only the normal screen buffer accumulates scrollback.  Each `scrollUp()`
 * call increments `buffer.scrollbackUsed` (up to `scrollbackCapacity`) so
 * that the MESSAGE THREAD can read historical rows via `scrollbackRow()`.
 *
 * ## Dirty tracking
 *
 * - `scrollUp()` marks only the newly cleared bottom rows dirty (the rows
 *   above them are unchanged from the renderer's perspective because the
 *   ring-buffer head moved — the renderer re-reads from the new physical
 *   positions automatically).
 * - `scrollDown()` calls `markAllDirty()` because every visible row shifts.
 * - `scrollRegion()` marks every row in [top, bottom] dirty.
 *
 * @see Grid.h   — class declaration, ring-buffer layout, thread ownership table.
 * @see Cell     — 16-byte terminal cell type stored in the ring buffer.
 * @see RowState — per-row metadata (wrap flag) copied alongside cells.
 * @see State    — atomic terminal parameter store (screen index, dimensions).
 */

#include "Grid.h"
#include "../data/State.h"

namespace Terminal
{ /*____________________________________________________________________________*/

// ============================================================================
// Scroll Operations
// ============================================================================

/**
 * @brief Scrolls the entire visible area up by `count` lines.
 *
 * Advances `buffer.head` by `count` (mod `buffer.rowMask + 1`) so that the
 * ring-buffer write head moves forward without copying any cell data.  The
 * physical rows that become the new bottom rows are cleared immediately after
 * each head advance.
 *
 * @par Scrollback accounting
 * On the normal screen (`state.getScreen() == normal`), each scroll step
 * increments `buffer.scrollbackUsed` until it reaches `scrollbackCapacity`.
 * The alternate screen never accumulates scrollback.
 *
 * @par Dirty tracking
 * Only the `count` newly cleared bottom rows are marked dirty.  Rows above
 * them are not marked because the ring-buffer head advance already makes the
 * renderer read from the correct (shifted) physical positions.
 *
 * @param count  Number of lines to scroll up.  Must be > 0.
 * @note READER THREAD — lock-free, noexcept.
 * @see scrollDown(), scrollRegionUp()
 */
void Grid::scrollUp (int count) noexcept
{
    const auto scr { state.getScreen() };
    Buffer& buffer { bufferForScreen() };

    for (int i { 0 }; i < count; ++i)
    {
        buffer.head = (buffer.head + 1) & buffer.rowMask;

        // Clear only cells — skip graphemes and rowStates (overwritten by next write)
        Cell* row { rowPtr (buffer, visibleRows - 1) };
        const Cell defaultCell {};
        std::fill (row, row + cols, defaultCell);

        if (scr == normal and buffer.scrollbackUsed < scrollbackCapacity)
        {
            ++buffer.scrollbackUsed;
        }
    }

    scrollDelta.fetch_add (count, std::memory_order_relaxed);

    for (int r { visibleRows - count }; r < visibleRows; ++r)
    {
        if (r >= 0)
        {
            markRowDirty (r);
        }
    }
}

/**
 * @brief Scrolls the entire visible area down by `count` lines.
 *
 * Decrements `buffer.head` by `count` (mod `buffer.rowMask + 1`), exposing
 * `count` new blank rows at the top of the viewport.  Each newly exposed row
 * is cleared via `clearRow()`.
 *
 * @par Dirty tracking
 * Calls `markAllDirty()` because every visible row shifts to a new physical
 * position — the renderer must repaint the entire viewport.
 *
 * @param count  Number of lines to scroll down.  Must be > 0.
 * @note READER THREAD — lock-free, noexcept.
 * @see scrollUp(), scrollRegionDown()
 */
void Grid::scrollDown (int count) noexcept
{
    Buffer& buffer { bufferForScreen() };

    for (int i { 0 }; i < count; ++i)
    {
        buffer.head = (buffer.head - 1) & buffer.rowMask;
        clearRow (buffer, 0);
    }

    markAllDirty();
}

/**
 * @brief Scrolls a sub-region of the screen up by `count` lines.
 *
 * If the region spans the full viewport (`top == 0` and
 * `bottom == visibleRows - 1`), delegates to `scrollUp()` to use the
 * efficient O(1) head-pointer path.  Otherwise, delegates to `scrollRegion()`
 * which uses `memcpy` to shift rows within the region.
 *
 * @par Scroll region (DECSTBM margins)
 * The VT terminal standard allows programs to set top and bottom scroll
 * margins via the DECSTBM CSI sequence.  When margins are active, scroll
 * operations must be confined to [top, bottom] and must not affect rows
 * outside that range.
 *
 * @param top     First row of the scroll region (inclusive, 0-based).
 * @param bottom  Last row of the scroll region (inclusive, 0-based).
 * @param count   Number of lines to scroll up.
 * @note READER THREAD — lock-free, noexcept.
 * @see scrollRegionDown(), scrollUp(), scrollRegion()
 */
void Grid::scrollRegionUp (int top, int bottom, int count) noexcept
{
    if (top == 0 and bottom == visibleRows - 1)
    {
        scrollUp (count);
    }
    else
    {
        scrollRegion (top, bottom, count, true);
    }
}

/**
 * @brief Scrolls a sub-region of the screen down by `count` lines.
 *
 * If the region spans the full viewport (`top == 0` and
 * `bottom == visibleRows - 1`), delegates to `scrollDown()` to use the
 * efficient O(1) head-pointer path.  Otherwise, delegates to `scrollRegion()`
 * which uses `memcpy` to shift rows within the region.
 *
 * @param top     First row of the scroll region (inclusive, 0-based).
 * @param bottom  Last row of the scroll region (inclusive, 0-based).
 * @param count   Number of lines to scroll down.
 * @note READER THREAD — lock-free, noexcept.
 * @see scrollRegionUp(), scrollDown(), scrollRegion()
 */
void Grid::scrollRegionDown (int top, int bottom, int count) noexcept
{
    if (top == 0 and bottom == visibleRows - 1)
    {
        scrollDown (count);
    }
    else
    {
        scrollRegion (top, bottom, count, false);
    }
}

/**
 * @brief Shifts rows [top, bottom] upward by `ec` lines within `buffer`.
 *
 * Copies rows `[top + ec, bottom]` down to `[top, bottom - ec]` using
 * `std::memcpy`, then clears the vacated rows `[bottom - ec + 1, bottom]`.
 * RowState entries are copied alongside cell data so that wrap flags and
 * double-width flags follow their rows.
 *
 * @par Memory layout
 * Because `rowPtr()` returns a pointer into the flat ring-buffer array at the
 * correct physical offset, `memcpy` copies exactly one row's worth of Cell
 * data (`rowBytes` bytes) per call.  The source and destination physical rows
 * never overlap because the region is strictly within the visible window.
 *
 * @param buffer    Target buffer.
 * @param top       First row of the region (inclusive, 0-based).
 * @param bottom    Last row of the region (inclusive, 0-based).
 * @param ec        Effective scroll count (already clamped to region size by
 *                  `scrollRegion()`).
 * @param rowBytes  Byte size of one row: `cols * sizeof(Cell)`.
 * @note READER THREAD — lock-free, noexcept.
 * @see shiftRegionDown(), scrollRegion()
 */
void Grid::shiftRegionUp (Buffer& buffer, int top, int bottom, int ec, size_t rowBytes) noexcept
{
    for (int i { top }; i <= bottom - ec; ++i)
    {
        std::memcpy (rowPtr (buffer, i), rowPtr (buffer, i + ec), rowBytes);
        buffer.rowStates[physicalRow (buffer, i)] = buffer.rowStates[physicalRow (buffer, i + ec)];
    }
    for (int i { bottom - ec + 1 }; i <= bottom; ++i)
    {
        clearRow (buffer, i);
    }
}

/**
 * @brief Shifts rows [top, bottom] downward by `ec` lines within `buffer`.
 *
 * Copies rows `[top, bottom - ec]` up to `[top + ec, bottom]` using
 * `std::memcpy` (iterating from bottom to top to avoid overwriting source
 * rows before they are copied), then clears the vacated rows `[top, top + ec - 1]`.
 * RowState entries are copied alongside cell data.
 *
 * @par Iteration order
 * The loop runs from `bottom` down to `top + ec` so that each destination row
 * is written before its source row could be overwritten.  This is the
 * downward-shift equivalent of a `memmove` with overlapping regions.
 *
 * @param buffer    Target buffer.
 * @param top       First row of the region (inclusive, 0-based).
 * @param bottom    Last row of the region (inclusive, 0-based).
 * @param ec        Effective scroll count (already clamped to region size by
 *                  `scrollRegion()`).
 * @param rowBytes  Byte size of one row: `cols * sizeof(Cell)`.
 * @note READER THREAD — lock-free, noexcept.
 * @see shiftRegionUp(), scrollRegion()
 */
void Grid::shiftRegionDown (Buffer& buffer, int top, int bottom, int ec, size_t rowBytes) noexcept
{
    for (int i { bottom }; i >= top + ec; --i)
    {
        std::memcpy (rowPtr (buffer, i), rowPtr (buffer, i - ec), rowBytes);
        buffer.rowStates[physicalRow (buffer, i)] = buffer.rowStates[physicalRow (buffer, i - ec)];
    }
    for (int i { top }; i < top + ec; ++i)
    {
        clearRow (buffer, i);
    }
}

/**
 * @brief Shared implementation for `scrollRegionUp()` and `scrollRegionDown()`.
 *
 * Validates the region bounds, clamps `count` to the region size so that
 * over-scrolling simply clears the entire region, and delegates to
 * `shiftRegionUp()` or `shiftRegionDown()`.  Marks every row in [top, bottom]
 * dirty after the shift.
 *
 * @par Bounds validation
 * The function is a no-op if any of the following hold:
 * - `top < 0` or `bottom >= visibleRows`
 * - `top >= bottom` (degenerate region)
 * - `count <= 0`
 *
 * @param top     First row of the scroll region (inclusive, 0-based).
 * @param bottom  Last row of the scroll region (inclusive, 0-based).
 * @param count   Number of lines to shift.
 * @param up      `true` to shift rows upward (scroll up); `false` to shift
 *                rows downward (scroll down).
 * @note READER THREAD — lock-free, noexcept.
 * @see shiftRegionUp(), shiftRegionDown()
 */
void Grid::scrollRegion (int top, int bottom, int count, bool up) noexcept
{
    if (top >= 0 and bottom < visibleRows and top < bottom and count > 0)
    {
        Buffer& buffer { bufferForScreen() };
        const int regionSize { bottom - top + 1 };
        const int ec { juce::jmin (count, regionSize) };
        const size_t rowBytes { static_cast<size_t> (cols) * sizeof (Cell) };

        if (up)
        {
            shiftRegionUp (buffer, top, bottom, ec, rowBytes);
        }
        else
        {
            shiftRegionDown (buffer, top, bottom, ec, rowBytes);
        }

        for (int r { top }; r <= bottom; ++r)
        {
            markRowDirty (r);
        }
    }
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Terminal
