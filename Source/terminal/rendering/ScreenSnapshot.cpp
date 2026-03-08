/**
 * @file ScreenSnapshot.cpp
 * @brief Snapshot packing and publication for the terminal renderer.
 *
 * This translation unit implements `Screen::updateSnapshot()`, the final step
 * of the per-frame rendering pipeline.  It is responsible for:
 *
 * 1. **Capacity management** — calling `Render::Snapshot::ensureCapacity()` to
 *    grow the snapshot's `HeapBlock` arrays if the current frame has more
 *    glyphs or backgrounds than the previous one.
 *
 * 2. **Data packing** — copying per-row glyph and background data from the
 *    `cachedMono`, `cachedEmoji`, and `cachedBg` arrays into the contiguous
 *    `Render::Snapshot` arrays via `memcpy`.
 *
 * 3. **Cursor state** — writing the cursor position and visibility from
 *    `State` into the snapshot.
 *
 * 4. **Publication** — calling `jreng::GLSnapshotBuffer::write()` to hand
 *    the snapshot to the GL THREAD.  Double-buffer rotation is handled
 *    internally by `GLSnapshotBuffer`.
 *
 * @see Screen.h
 * @see ScreenRender.cpp
 * @see jreng::GLSnapshotBuffer
 * @see Render::Snapshot
 */

#include "Screen.h"

namespace Terminal
{ /*____________________________________________________________________________*/

// MESSAGE THREAD

/**
 * @brief Packs per-row caches into a `Render::Snapshot` and publishes it.
 *
 * Called at the end of `Screen::buildSnapshot()` after all dirty rows have
 * been processed.  Performs the following steps:
 *
 * 1. Obtains the write buffer from `GLSnapshotBuffer::getWriteBuffer()`.
 * 2. Totals `monoCount[r]`, `emojiCount[r]`, and `bgCount[r]` across all rows.
 * 3. Calls `Render::Snapshot::ensureCapacity()` to grow the snapshot arrays
 *    if needed.
 * 4. Sets `gridWidth` and `gridHeight` on the snapshot.
 * 5. Copies per-row glyph and background data into the contiguous snapshot
 *    arrays via `memcpy`, advancing offsets as each row is packed.
 * 6. Writes the total counts (`monoCount`, `emojiCount`, `backgroundCount`)
 *    and cursor state (`cursorPosition`, `cursorVisible`) into the snapshot.
 * 7. Calls `jreng::GLSnapshotBuffer::write()` to hand the snapshot to the GL THREAD.
 *
 * @param state      Current terminal state; provides cursor position,
 *                   visibility, and active screen type.
 * @param rows       Number of visible rows (= `state.getVisibleRows()`).
 * @param maxGlyphs  Maximum glyph slots per row (= `cacheCols * 2`).
 *
 * @note **MESSAGE THREAD** only.  Must not be called from the GL THREAD.
 *
 * @see Render::Snapshot::ensureCapacity()
 * @see jreng::GLSnapshotBuffer::write()
 * @see Screen::buildSnapshot()
 */
void Screen::updateSnapshot (const State& state, int rows, int maxGlyphs) noexcept
{
    const ActiveScreen scr { state.getScreen() };

    auto& snapshot { resources.snapshotBuffer.getWriteBuffer() };

    int totalMono  { 0 };
    int totalEmoji { 0 };
    int totalBg    { 0 };

    for (int r { 0 }; r < rows; ++r)
    {
        totalMono  += monoCount[r];
        totalEmoji += emojiCount[r];
        totalBg    += bgCount[r];
    }

    snapshot.ensureCapacity (totalMono, totalEmoji, totalBg);
    snapshot.gridWidth  = cacheCols;
    snapshot.gridHeight = rows;

    int monoOffset  { 0 };
    int emojiOffset { 0 };
    int bgOffset    { 0 };

    for (int r { 0 }; r < rows; ++r)
    {
        if (monoCount[r] > 0)
        {
            std::memcpy (snapshot.mono.get() + monoOffset,
                         cachedMono.get() + r * maxGlyphs,
                         static_cast<size_t> (monoCount[r]) * sizeof (Render::Glyph));
            monoOffset += monoCount[r];
        }

        if (emojiCount[r] > 0)
        {
            std::memcpy (snapshot.emoji.get() + emojiOffset,
                         cachedEmoji.get() + r * maxGlyphs,
                         static_cast<size_t> (emojiCount[r]) * sizeof (Render::Glyph));
            emojiOffset += emojiCount[r];
        }

        if (bgCount[r] > 0)
        {
            std::memcpy (snapshot.backgrounds.get() + bgOffset,
                         cachedBg.get() + r * bgCacheCols,
                         static_cast<size_t> (bgCount[r]) * sizeof (Render::Background));
            bgOffset += bgCount[r];
        }
    }

    snapshot.monoCount        = totalMono;
    snapshot.emojiCount       = totalEmoji;
    snapshot.backgroundCount  = totalBg;

    snapshot.cursorPosition.x = state.getCursorCol (scr);
    snapshot.cursorPosition.y = state.getCursorRow (scr);
    snapshot.cursorVisible    = state.isCursorVisible (scr);

    resources.snapshotBuffer.write();
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Terminal
