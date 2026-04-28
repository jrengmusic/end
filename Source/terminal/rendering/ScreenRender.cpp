/**
 * @file ScreenRender.cpp
 * @brief Frame-level snapshot orchestration: dirty-row loop and snapshot dispatch.
 *
 * This translation unit implements `Screen::buildSnapshot()`, the per-frame
 * entry point that consumes dirty-row bits from `Grid`, iterates every changed
 * row, and delegates per-cell work to `processCellForSnapshot()` (implemented
 * in `ScreenRenderCell.cpp`).  After all rows are processed it calls
 * `updateSnapshot()` (implemented in `ScreenSnapshot.cpp`) to pack and publish
 * the `Render::Snapshot`.
 *
 * ## Pipeline position
 *
 * ```
 * Screen::render()
 *   └─ Screen::buildSnapshot()          ← THIS FILE
 *        ├─ grid.consumeDirtyRows()
 *        ├─ state.consumeFullRebuild()
 *        ├─ for each dirty row:
 *        │    └─ processCellForSnapshot()  [ScreenRenderCell.cpp]
 *        └─ updateSnapshot()              [ScreenSnapshot.cpp]
 * ```
 *
 * @see Screen.h
 * @see ScreenRenderCell.cpp
 * @see ScreenSnapshot.cpp
 */

#include "Screen.h"
#include "../logic/SixelDecoder.h"
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
template <typename Renderer>
void Screen<Renderer>::buildSnapshot (State& state, Grid& grid) noexcept
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

    // Pull hint overlay directly from LinkManager (no State relay).
    hintOverlay      = linkManager != nullptr ? linkManager->getActiveHintsData() : nullptr;
    hintOverlayCount = linkManager != nullptr ? linkManager->getActiveHintsCount() : 0;

    for (int r { 0 }; r < rows; ++r)
    {
        const int word { r >> 6 };
        const uint64_t bit { static_cast<uint64_t> (1) << (r & 63) };

        if ((dirtyBits[word] & bit) != 0)
        {
            const Cell* rowCells { offset > 0
                ? grid.scrollbackRow (r, offset)
                : grid.activeVisibleRow (r) };

            if (rowCells != nullptr)
            {
                Cell* prevRow { previousCells.get()
                                + static_cast<size_t> (r) * static_cast<size_t> (cacheCols) };

                // Row-level memcmp: if all cells unchanged and no overlay rebuild
                // needed, skip entirely. Previous frame's cached quads remain valid.
                // fullRebuild bypasses skip — selection/hint overlays need regeneration.
                if (fullRebuild
                    or std::memcmp (rowCells, prevRow, static_cast<size_t> (cols) * sizeof (Cell)) != 0)
                {
                    const Grapheme* rowGraphemes { offset > 0
                        ? grid.scrollbackGraphemeRow (r, offset)
                        : grid.activeVisibleGraphemeRow (r) };

                    monoCount[r]        = 0;
                    emojiCount[r]       = 0;
                    bgCount[r]          = 0;
                    imageCacheCount[r]  = 0;
                    ligatureSkip        = 0;

                    // Leading/trailing blank trim
                    int firstCol { 0 };

                    while (firstCol < cols
                           and rowCells[firstCol].codepoint == 0
                           and rowCells[firstCol].bg.isTheme()
                           and rowCells[firstCol].style == 0)
                    {
                        ++firstCol;
                    }

                    int lastCol { cols - 1 };

                    while (lastCol >= firstCol
                           and rowCells[lastCol].codepoint == 0
                           and rowCells[lastCol].bg.isTheme()
                           and rowCells[lastCol].style == 0)
                    {
                        --lastCol;
                    }

                    for (int c { firstCol }; c <= lastCol; ++c)
                    {
                        processCellForSnapshot (rowCells[c], rowCells, rowGraphemes, c, r, grid);
                    }

                    // Store current row for next-frame comparison.
                    std::memcpy (prevRow, rowCells, static_cast<size_t> (cols) * sizeof (Cell));
                }
            }
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

    updateSnapshot (state, grid, rows, maxGlyphsPerRow);
}

template void Screen<jam::Glyph::GLContext>::buildSnapshot (State&, Grid&) noexcept;
template void Screen<jam::Glyph::GraphicsContext>::buildSnapshot (State&, Grid&) noexcept;

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Terminal
