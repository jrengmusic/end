/**
 * @file ScreenRender.cpp
 * @brief Frame-level snapshot orchestration: dirty-row loop and snapshot dispatch.
 *
 * This translation unit implements `Screen::buildSnapshot()`, the per-frame
 * entry point that consumes dirty-row bits from `Grid`, iterates every changed
 * row, and delegates per-cell work to `processCellForSnapshot()` (implemented
 * in `GlyphCell.cpp`).  After all rows are processed it calls
 * `updateSnapshot()` (implemented in `ScreenSnapshot.cpp`) to pack and publish
 * the `Render::Snapshot`.
 *
 * ## Pipeline position
 *
 * ```
 * Screen::render()
 *   └─ Screen::buildSnapshot()             ← THIS FILE
 *        ├─ grid.consumeDirtyRows()
 *        ├─ state.consumeFullRebuild()
 *        ├─ for each dirty row:
 *        │    └─ processCellForSnapshot()  [GlyphCell.cpp]
 *        └─ updateSnapshot()              [ScreenSnapshot.cpp]
 * ```
 *
 * @see Screen.h
 * @see GlyphCell.cpp
 * @see ScreenSnapshot.cpp
 */

#include "Screen.h"
#include "../data/Identifier.h"
#include "../selection/LinkManager.h"


namespace Terminal
{ /*____________________________________________________________________________*/

// MESSAGE THREAD

// =========================================================================
// Screen::buildSnapshot
// =========================================================================

/**
 * @brief Rebuilds dirty rows in the per-row caches from `Grid` and calls `updateSnapshot()`.
 *
 * Calls `grid.consumeDirtyRows()` to obtain the set of rows that have changed
 * since the last call.  Only rows with their dirty bit set are reprocessed;
 * clean rows retain their previous-frame glyph and background caches.  For
 * each dirty row, reads cells and graphemes directly from `Grid` (scrollback
 * or active, depending on `state.getScrollOffset()`), resets the row's glyph
 * and background counts to zero, resets `ligatureSkip`, and calls
 * `processCellForSnapshot()` for every cell.  After all rows are processed,
 * calls `updateSnapshot()` to pack the caches into a `Render::Snapshot` and
 * publish it.
 *
 * @param state  Current terminal state (provides `getCols()`, `getVisibleRows()`,
 *               `getScrollOffset()`).
 * @param grid   Terminal grid (dirty bits consumed; cells read per dirty row).
 *
 * @note **MESSAGE THREAD**.
 * @see processCellForSnapshot()
 * @see updateSnapshot()
 */
template <typename Context>
void Screen<Context>::buildSnapshot (State& state, Grid& grid) noexcept
{
    const int cols { state.getCols() };
    const int rows { state.getVisibleRows() };
    const int offset { state.getScrollOffset() };
    // Always consume Grid dirty bits to keep them from accumulating.
    // If a full rebuild is requested, override with all-ones so every row
    // is processed — same initial state as a first-frame render.
    uint64_t dirtyBits[4] {};
    grid.consumeDirtyRows (dirtyBits);

    const bool fullRebuild { state.consumeFullRebuild() };

    if (fullRebuild)
    {
        std::memset (dirtyBits, 0xFF, sizeof (dirtyBits));
    }

    frameScrollDelta = grid.consumeScrollDelta();

    // Scroll shifts visible content — all rows now show different data.
    // Force all-dirty so every row is rebuilt from current grid content.
    // Without this, stale cached quads overwrite memmove'd pixels.
    if (frameScrollDelta > 0)
    {
        std::memset (dirtyBits, 0xFF, sizeof (dirtyBits));
    }

    for (int i { 0 }; i < 4; ++i)
    {
        frameDirtyBits[i] |= dirtyBits[i];
    }

    glyph.beginFrame();

    for (int r { 0 }; r < rows; ++r)
    {
        const int word { r >> 6 };
        const uint64_t bit { static_cast<uint64_t> (1) << (r & 63) };

        if ((dirtyBits[word] & bit) != 0)
        {
            const Cell* rowCells { offset > 0
                ? grid.scrollbackRow (r, offset)
                : grid.activeVisibleRow (r) };

            const Grapheme* rowGraphemes { offset > 0
                ? grid.scrollbackGraphemeRow (r, offset)
                : grid.activeVisibleGraphemeRow (r) };

            glyph.processRow (r, rowCells, cols, rowGraphemes, grid, fullRebuild);
        }
    }

    // Mark previous cursor row dirty — clears stale cursor overlay pixels
    // from the persistent CPU render target. Cost: one row clear per frame.
    if (previousCursorRow >= 0 and previousCursorRow < rows)
    {
        const int word { previousCursorRow >> 6 };
        const uint64_t bit { static_cast<uint64_t> (1) << (previousCursorRow & 63) };
        frameDirtyBits[word] |= bit;
    }

    previousCursorRow = state.getCursorRow();

    updateSnapshot (state, grid, rows);
}

template void Screen<jam::Glyph::GLContext>::buildSnapshot (State&, Grid&) noexcept;
template void Screen<jam::Glyph::GraphicsContext>::buildSnapshot (State&, Grid&) noexcept;

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Terminal
