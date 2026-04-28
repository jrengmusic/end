/**
 * @file Grid.h
 * @brief Ring-buffer cell storage for the terminal screen grid.
 *
 * Grid owns the raw cell memory for both the normal and alternate screen
 * buffers.  It is the lowest-level storage layer in the terminal stack —
 * everything above it (Screen, Snapshot, Renderer) reads through Grid.
 *
 * ## Ring-buffer layout
 *
 * Each Buffer holds a flat `HeapBlock<Cell>` of size `totalRows × cols`,
 * where `totalRows` is rounded up to the next power of two so that modular
 * row indexing reduces to a bitwise AND:
 *
 * @code
 * physicalRow = (head - visibleRows + 1 + visibleRow) & rowMask
 * @endcode
 *
 * `head` is the physical index of the **last** visible row (bottom of the
 * viewport).  Scrolling up increments `head` by one (mod `totalRows`),
 * which recycles the oldest row as the new blank bottom row without moving
 * any data.  The scrollback region occupies the physical rows immediately
 * before the visible window in the ring.
 *
 * ## Dual screen
 *
 * `buffers[normal]` holds the primary screen with a scrollback region.
 * `buffers[alternate]` holds the alternate screen (no scrollback).
 * `bufferForScreen()` selects the active buffer via `State::getScreen()`.
 *
 * ## Dirty tracking
 *
 * `dirtyRows[4]` is a 256-bit atomic bitmask (4 × `uint64_t`).  Bit `r`
 * in word `r >> 6` is set whenever visible row `r` is modified.  The
 * MESSAGE THREAD calls `consumeDirtyRows()` to atomically swap the bitmask
 * to zero and obtain the set of rows that need repainting.
 *
 * ## Grapheme sidecar
 *
 * Multi-codepoint grapheme clusters store their extra codepoints in a
 * parallel `HeapBlock<Grapheme>` that is co-indexed with the cell array.
 * A cell with `Cell::LAYOUT_GRAPHEME` set has a valid entry at the same
 * physical offset in the grapheme block.
 *
 * ## Thread ownership
 *
 * | Operation              | Thread         |
 * |------------------------|----------------|
 * | `activeWrite*()`       | READER THREAD  |
 * | `scroll*()`, `erase*()` | READER THREAD |
 * | `resize()`                   | MESSAGE THREAD (holds resizeLock) |
 * | `clearBuffer()`              | READER THREAD (holds resizeLock)  |
 * | `activeVisibleRow()` reads | MESSAGE THREAD |
 * | `consumeDirtyRows()`   | MESSAGE THREAD |
 * | `extractText()`        | MESSAGE THREAD (holds resizeLock) |
 *
 * @see Cell       — the 16-byte terminal cell type.
 * @see Grapheme   — extra codepoints for grapheme clusters.
 * @see RowState   — per-row metadata (wrap flag, double-width flag).
 * @see State      — atomic terminal parameter store.
 */

#pragma once

#include <JuceHeader.h>

#include "../../lua/Engine.h"
#include "../data/State.h"
#include "../data/Cell.h"
#include "SixelDecoder.h"

namespace Terminal
{ /*____________________________________________________________________________*/

class State;

/**
 * @class Grid
 * @brief Ring-buffer storage for terminal cells, graphemes, and row metadata.
 *
 * Grid manages two independent screen buffers (normal + alternate) as
 * power-of-two ring buffers.  It provides:
 *
 * - **Cell read/write** — single-cell and run-length write paths.
 * - **Grapheme sidecar** — parallel storage for multi-codepoint clusters.
 * - **Dirty tracking** — 256-bit atomic bitmask for incremental repaint.
 * - **Scroll operations** — full-screen and region scroll via head pointer.
 * - **Erase operations** — row, row-range, cell, and cell-range erasure.
 * - **Resize / reflow** — re-wraps soft-wrapped lines to the new column width.
 * - **Scrollback** — normal screen retains up to `scrollbackCapacity` rows.
 *
 * @note All `activeWrite*()`, `scroll*()`, and `erase*()` methods are called
 *       exclusively on the READER THREAD.  All `activeVisibleRow()` const
 *       overloads and `consumeDirtyRows()` are called on the MESSAGE THREAD.
 *       `resize()` and `extractText()` acquire `resizeLock` to synchronise
 *       between the two threads.
 *
 * @see Cell, Grapheme, RowState, State
 */
class Grid
{
public:
    /**
     * @brief Constructs the Grid and allocates both screen buffers.
     *
     * Reads initial dimensions from `state` and the scrollback capacity from
     * `Config`.  Allocates `buffers[normal]` with `visibleRows +
     * scrollbackCapacity` total rows and `buffers[alternate]` with
     * `visibleRows` total rows (no scrollback).
     *
     * @param state  Terminal parameter store; must outlive this Grid.
     * @note READER THREAD — called during terminal initialisation.
     */
    explicit Grid (State& state);

    /**
     * @brief Returns the resize lock used to synchronise resize and text extraction.
     *
     * The MESSAGE THREAD acquires this lock during `resize()`.  The READER
     * THREAD acquires it during `clearBuffer()` and data processing
     * (`tty->onData`).  The MESSAGE THREAD acquires it during `extractText()`.
     * No other methods require the lock.
     *
     * @return Reference to the internal `juce::CriticalSection`.
     * @note Lock-free getter — the lock itself is not acquired here.
     */
    juce::CriticalSection& getResizeLock() noexcept;

    /**
     * @brief Const overload of `getResizeLock()`.
     *
     * Allows read-only `const Grid&` holders (e.g. `LinkManager`) to acquire
     * the resize lock without casting away const.
     *
     * @return Reference to the internal `juce::CriticalSection`.
     * @note Lock-free getter — the lock itself is not acquired here.
     */
    juce::CriticalSection& getResizeLock() const noexcept;

    /**
     * @brief Re-allocates both screen buffers to the new dimensions and reflows
     *        content on the normal screen.
     *
     * Acquires `resizeLock` and writes the new dimensions to State inside the
     * lock, ensuring that State and Grid are always in sync from the message
     * thread's perspective.  Reflows soft-wrapped lines on the normal screen;
     * the alternate screen is re-initialised without reflow (xterm behaviour).
     *
     * @param newCols        New terminal width in character columns.
     * @param newVisibleRows New terminal height in character rows.
     * @note MESSAGE THREAD — acquires `resizeLock`.  Allocates heap memory.
     */
    void resize (int newCols, int newVisibleRows);

    /**
     * @brief Clears the active screen buffer and marks all rows dirty.
     *
     * Acquires `resizeLock` and re-initialises the active buffer without
     * changing dimensions.  Equivalent to erasing every cell and resetting
     * every row state.
     *
     * @note READER THREAD — acquires `resizeLock`.
     */
    void clearBuffer();

    /**
     * @brief Resets the scrollback counter to zero without reallocating.
     *
     * Called by `CSI 3 J` (Erase Display mode 3) to discard scrollback
     * history.  The ring buffer data is not cleared — only `scrollbackUsed`
     * is reset so the message thread can no longer scroll back into it.
     *
     * @note READER THREAD only — called from Parser during VT processing.
     */
    void clearScrollback() noexcept;

    /**
     * @brief Returns the current column count.
     * @return Allocated column count of the normal screen buffer.
     * @note Safe from any thread when holding `resizeLock`.  Always equal to
     *       State dims because grid.resize() runs on the message thread.
     */
    int getCols () const noexcept { return buffers.at (normal).allocatedCols; }

    /**
     * @brief Returns the number of visible rows in the terminal viewport.
     * @return Allocated visible row count of the normal screen buffer.
     * @note Safe from any thread when holding `resizeLock`.  Always equal to
     *       State dims because grid.resize() runs on the message thread.
     */
    int getVisibleRows () const noexcept { return buffers.at (normal).allocatedVisibleRows; }

    /**
     * @brief Marks a single visible row as dirty in the atomic bitmask.
     *
     * Sets bit `row & 63` in `dirtyRows[row >> 6]` using
     * `memory_order_relaxed`, then calls `State::setSnapshotDirty()`.
     * Rows outside [0, 255] are silently ignored.
     *
     * @param row  Zero-based visible row index (0 … visibleRows-1).
     * @note READER THREAD — lock-free, noexcept.
     */
    void markRowDirty (int row) noexcept;

    /**
     * @brief Marks all 256 tracked rows dirty and resets the scroll delta.
     *
     * Stores `~uint64_t{0}` into all four `dirtyRows` words and resets
     * `scrollDelta` to zero.  Called after resize or full-screen clear.
     *
     * @note READER THREAD — lock-free, noexcept.
     */
    void markAllDirty() noexcept;

    /**
     * @brief OR-merges a local 256-bit dirty bitmask into the shared bitmask.
     *
     * Allows a caller that has accumulated dirty bits locally (e.g. a bulk
     * write loop) to flush them all at once with four atomic OR operations
     * instead of one per row.
     *
     * @param localDirty  Four `uint64_t` words representing rows 0–255.
     *                    Word 0 covers rows 0–63, word 1 covers 64–127, etc.
     * @note READER THREAD — lock-free, noexcept.
     */
    void batchMarkDirty (const uint64_t localDirty[4]) noexcept;

    /**
     * @brief Atomically swaps the dirty bitmask to zero and returns the old value.
     *
     * Used by the MESSAGE THREAD to determine which rows need repainting.
     * The exchange uses `memory_order_acq_rel` to ensure all READER THREAD
     * cell writes are visible before the MESSAGE THREAD reads the grid.
     *
     * @param out  Output array of four `uint64_t` words.  On return, bit `r`
     *             in `out[r >> 6]` is set if visible row `r` was dirtied since
     *             the last call.
     * @note MESSAGE THREAD — lock-free, noexcept.
     */
    void consumeDirtyRows (uint64_t out[4]) noexcept;

    /**
     * @brief Atomically swaps the scroll delta to zero and returns the old value.
     *
     * The scroll delta accumulates the net number of lines scrolled up since
     * the last call.  The MESSAGE THREAD uses this to animate smooth scrolling
     * or to update the scrollback offset.
     *
     * @return Net lines scrolled up (positive = scrolled up).
     * @note MESSAGE THREAD — lock-free, noexcept.
     */
    int consumeScrollDelta() noexcept;

    // =========================================================================
    /** @name Decoded image storage (READER writes, MESSAGE reads on demand)
     *
     *  Decoded images are produced on the READER THREAD and stored in Grid.
     *  The MESSAGE THREAD pulls them on first atlas encounter — same pattern
     *  as codepoints in cells: READER writes, renderer reads.
     *
     *  Protocol:
     *  1. READER THREAD calls `reserveImageId()` — atomic increment.
     *  2. READER THREAD calls `activeWriteImage()` with the reserved ID.
     *  3. READER THREAD calls `storeDecodedImage()` — stores RGBA in map.
     *  4. MESSAGE THREAD encounters image cell → calls `getDecodedImage()`.
     *  5. MESSAGE THREAD calls `ImageAtlas::stageWithId()` for the entry.
     *  6. MESSAGE THREAD calls `releaseDecodedImage()` to free the RGBA.
     * @{ */

    /**
     * @brief Reserves the next image ID via atomic increment.
     *
     * The READER THREAD calls this immediately before `activeWriteImage()` to
     * obtain an ID that both Grid cells and the atlas agree on.  IDs start at 1;
     * 0 is reserved as "invalid".
     *
     * @return A unique image ID in [1, UINT32_MAX].
     * @note READER THREAD — lock-free, noexcept.
     */
    uint32_t reserveImageId() noexcept;

    /**
     * @brief Stores decoded image data for later atlas consumption.
     *
     * @param img  PendingImage to store; moved into the internal map.
     * @note READER THREAD — called after image decode completes.
     */
    void storeDecodedImage (PendingImage&& img) noexcept;

    /**
     * @brief Retrieves stored decoded image data without removing it.
     *
     * @param imageId  Image ID to look up.
     * @return Pointer to PendingImage, or nullptr if not found.
     * @note MESSAGE THREAD — called by renderer on first atlas encounter.
     */
    PendingImage* getDecodedImage (uint32_t imageId) noexcept;

    /**
     * @brief Removes decoded image data after atlas has consumed it.
     *
     * @param imageId  Image ID to remove.
     * @note MESSAGE THREAD.
     */
    void releaseDecodedImage (uint32_t imageId) noexcept;

    /** @} */

    /** @name Cell access
     *  All write methods are READER THREAD only.  Const read methods are
     *  MESSAGE THREAD only (called from the render path).
     * @{ */

    /**
     * @brief Writes a single cell to the active screen at (row, col).
     *
     * Bounds-checks both coordinates, writes the cell, and calls
     * `markRowDirty (row)`.
     *
     * @param row        Zero-based visible row index.
     * @param col        Zero-based column index.
     * @param cellState  Cell value to write.
     * @note READER THREAD — lock-free, noexcept.
     */
    void activeWriteCell (int row, int col, const Cell& cellState) noexcept;

    /**
     * @brief Writes a contiguous run of cells to the active screen.
     *
     * Copies `count` cells from `cells` into row `row` starting at column
     * `startCol` using `std::memcpy`.  Bounds-checks the entire range before
     * writing.  Calls `markRowDirty (row)` on success.
     *
     * @param row       Zero-based visible row index.
     * @param startCol  First destination column.
     * @param cells     Source cell array; must contain at least `count` elements.
     * @param count     Number of cells to copy (must be > 0).
     * @note READER THREAD — lock-free, noexcept.
     */
    void activeWriteRun (int row, int startCol, const Cell* cells, int count) noexcept;

    /**
     * @brief Writes a grapheme cluster sidecar for the cell at (row, col).
     *
     * Sets `Cell::LAYOUT_GRAPHEME` on the target cell and writes `grapheme`
     * into the parallel grapheme block at the same physical offset.
     * Calls `markRowDirty (row)`.
     *
     * @param row      Zero-based visible row index.
     * @param col      Zero-based column index.
     * @param grapheme Grapheme cluster to store.
     * @note READER THREAD — lock-free, noexcept.
     * @see activeEraseGrapheme(), activeReadGrapheme()
     */
    void activeWriteGrapheme (int row, int col, const Grapheme& grapheme) noexcept;

    /**
     * @brief Writes a multi-cell image span to the active buffer.
     *
     * Computes the cell span from pixel dimensions, writes `LAYOUT_IMAGE` on the
     * head cell (top-left), `LAYOUT_IMAGE_CONT` on all continuation cells.
     * Shared cell-writing path for all three SKiT decoders (Sixel, Kitty, iTerm2).
     *
     * @param startRow     Zero-based visible row index of the top-left image cell.
     * @param startCol     Zero-based column index of the top-left image cell.
     * @param imageId      Unique image identifier (stored in head cell's codepoint).
     * @param widthPx      Image width in pixels.
     * @param heightPx     Image height in pixels.
     * @param cellWidthPx  Terminal cell width in pixels.
     * @param cellHeightPx Terminal cell height in pixels.
     * @note READER THREAD — lock-free, noexcept.
     */
    void activeWriteImage (int startRow, int startCol,
                           uint32_t imageId, int widthPx, int heightPx,
                           int cellWidthPx, int cellHeightPx) noexcept;

    /**
     * @brief Clears the grapheme cluster sidecar for the cell at (row, col).
     *
     * Clears `Cell::LAYOUT_GRAPHEME` and zero-fills the grapheme entry.
     * No-op if the cell does not have a grapheme.  Calls `markRowDirty (row)`.
     *
     * @param row  Zero-based visible row index.
     * @param col  Zero-based column index.
     * @note READER THREAD — lock-free, noexcept.
     * @see activeWriteGrapheme()
     */
    void activeEraseGrapheme (int row, int col) noexcept;

    /**
     * @brief Returns a pointer to the grapheme cluster for the cell at (row, col).
     *
     * Returns `nullptr` if the coordinates are out of range or if the cell
     * does not have `Cell::LAYOUT_GRAPHEME` set.
     *
     * @param row  Zero-based visible row index.
     * @param col  Zero-based column index.
     * @return Pointer to the Grapheme entry, or `nullptr`.
     * @note MESSAGE THREAD — lock-free, noexcept.
     * @see activeWriteGrapheme()
     */
    const Grapheme* activeReadGrapheme (int row, int col) const noexcept;

    /** @} */

    /** @name Row access
     *  Returns pointers into the flat cell/grapheme arrays.  The returned
     *  pointer is valid until the next `resize()` or `clearBuffer()` call.
     * @{ */

    /**
     * @brief Returns a mutable pointer to the start of visible row `row`.
     *
     * @param row  Zero-based visible row index (0 … visibleRows-1).
     * @return Pointer to the first Cell in the row, or `nullptr` if out of range.
     * @note READER THREAD — lock-free, noexcept.
     */
    Cell* activeVisibleRow (int row) noexcept;

    /**
     * @brief Returns a const pointer to the start of visible row `row`.
     *
     * @param row  Zero-based visible row index (0 … visibleRows-1).
     * @return Pointer to the first Cell in the row, or `nullptr` if out of range.
     * @note MESSAGE THREAD — lock-free, noexcept.
     */
    const Cell* activeVisibleRow (int row) const noexcept;

    /**
     * @brief Returns a const pointer to the grapheme row for visible row `row`.
     *
     * The returned pointer addresses `cols` Grapheme entries co-indexed with
     * the cell row returned by `activeVisibleRow (row)`.
     *
     * @param row  Zero-based visible row index (0 … visibleRows-1).
     * @return Pointer to the first Grapheme in the row, or `nullptr` if out of range.
     * @note MESSAGE THREAD — lock-free, noexcept.
     */
    const Grapheme* activeVisibleGraphemeRow (int row) const noexcept;

    /**
     * @brief Returns the number of scrollback rows currently stored.
     *
     * Only meaningful for the normal screen buffer; always 0 for alternate.
     *
     * @return Number of scrollback rows available (0 … scrollbackCapacity).
     * @note MESSAGE THREAD — lock-free, noexcept.
     */
    int getScrollbackUsed() const noexcept;

    /**
     * @brief Returns a const pointer to a scrolled-back cell row.
     *
     * Applies `scrollOffset` to the ring-buffer index so that the caller can
     * read historical rows without copying data.  A `scrollOffset` of 1 returns
     * the row one line above the current live view.
     *
     * @param visibleRow   Zero-based visible row index (0 … visibleRows-1).
     * @param scrollOffset Number of lines scrolled back into the scrollback buffer.
     * @return Pointer to the first Cell in the scrolled row, or `nullptr` if
     *         `visibleRow` is out of range.
     * @note MESSAGE THREAD — lock-free, noexcept.
     * @see scrollbackGraphemeRow()
     */
    const Cell* scrollbackRow (int visibleRow, int scrollOffset) const noexcept;

    /**
     * @brief Returns a const pointer to a scrolled-back grapheme row.
     *
     * Parallel to `scrollbackRow()` but for the grapheme sidecar array.
     *
     * @param visibleRow   Zero-based visible row index (0 … visibleRows-1).
     * @param scrollOffset Number of lines scrolled back into the scrollback buffer.
     * @return Pointer to the first Grapheme in the scrolled row, or `nullptr` if
     *         `visibleRow` is out of range.
     * @note MESSAGE THREAD — lock-free, noexcept.
     * @see scrollbackRow()
     */
    const Grapheme* scrollbackGraphemeRow (int visibleRow, int scrollOffset) const noexcept;

    /**
     * @brief Returns a mutable reference to the RowState for visible row `row`.
     *
     * @param row  Zero-based visible row index.
     * @return Reference to the RowState entry for the physical row.
     * @note READER THREAD — lock-free, noexcept.
     */
    RowState& activeVisibleRowState (int row) noexcept;

    /**
     * @brief Returns a const reference to the RowState for visible row `row`.
     *
     * @param row  Zero-based visible row index.
     * @return Const reference to the RowState entry for the physical row.
     * @note MESSAGE THREAD — lock-free, noexcept.
     */
    const RowState& activeVisibleRowState (int row) const noexcept;

    /** @} */

    /**
     * @brief Extracts a rectangular region of text as a UTF-32 string.
     *
     * Iterates visible rows from `start.y` to `end.y`, collecting codepoints
     * from each cell (including grapheme cluster extra codepoints).  Empty
     * cells are emitted as spaces; wide-continuation cells are skipped.
     * Trailing whitespace is trimmed from each row.  Rows are separated by
     * `'\n'` except the last.
     *
     * @param start        Top-left corner of the selection (x = col, y = row).
     * @param end          Bottom-right corner of the selection (inclusive).
     * @param scrollOffset Number of rows the viewport is scrolled back (0 = live view).
     * @return A `juce::String` containing the selected text.
     * @note MESSAGE THREAD — caller must hold `resizeLock`.
     */
    juce::String extractText (juce::Point<int> start, juce::Point<int> end, int scrollOffset) const;

    /**
     * @brief Serializes both screen buffers into destData.
     *
     * Acquires resizeLock and writes a flat binary snapshot of both the normal
     * and alternate Buffer structs (scalars + cells + graphemes + rowStates).
     * Called by Processor::getStateInformation to delegate grid serialization.
     *
     * @param destData  MemoryBlock to append the snapshot to.
     * @note MESSAGE THREAD.
     */
    void getStateInformation (juce::MemoryBlock& destData) const;

    /**
     * @brief Restores both screen buffers from a snapshot produced by getStateInformation.
     *
     * Acquires resizeLock, reads scalars, allocates HeapBlocks to match, and
     * memcpys bulk data (cells + graphemes + rowStates). Called by Processor::setStateInformation.
     *
     * @param data  Pointer to the snapshot bytes positioned at the grid section.
     * @param size  Size in bytes of the grid section.
     * @note MESSAGE THREAD.
     */
    void setStateInformation (const void* data, int size);

    /**
     * @brief Extracts a box (rectangle) selection of text as a UTF-32 string.
     *
     * Unlike `extractText()`, which uses row-wrapped selection semantics, this
     * method applies the same column range `[topLeft.x, bottomRight.x]` to
     * every row in `[topLeft.y, bottomRight.y]`.  This produces a strict
     * rectangular region regardless of content.
     *
     * Empty cells are emitted as spaces; wide-continuation cells are skipped.
     * Trailing whitespace is trimmed from each row.  Rows are separated by
     * `'\n'` except the last.
     *
     * @param topLeft      Top-left corner of the rectangle (x = col, y = row).
     *                     Must already be normalised (min col/row of the selection).
     * @param bottomRight  Bottom-right corner of the rectangle (inclusive).
     *                     Must already be normalised (max col/row of the selection).
     * @param scrollOffset Number of rows the viewport is scrolled back (0 = live view).
     * @return A `juce::String` containing the selected text.
     * @note MESSAGE THREAD — caller must hold `resizeLock`.
     */
    juce::String extractBoxText (juce::Point<int> topLeft, juce::Point<int> bottomRight, int scrollOffset) const;

    /** @brief Returns the current monotonic seqno. READER THREAD — relaxed load. */
    uint64_t currentSeqno() const noexcept { return seqno.load (std::memory_order_relaxed); }

    /** @name Scroll operations
     *  All scroll methods operate on the active screen buffer and are called
     *  exclusively on the READER THREAD.
     * @{ */

    /**
     * @brief Scrolls the entire visible area up by `count` lines.
     *
     * Advances `buffer.head` by `count` (mod `totalRows`), clearing the new
     * bottom rows.  For the normal screen, increments `scrollbackUsed` up to
     * `scrollbackCapacity`.  Accumulates `count` into `scrollDelta`.
     *
     * @param count  Number of lines to scroll up (must be > 0).
     * @param fill   Cell value for newly exposed rows.
     * @note READER THREAD — lock-free, noexcept.
     */
    void scrollUp (int count, const Cell& fill = Cell {}) noexcept;

    /**
     * @brief Scrolls the entire visible area down by `count` lines.
     *
     * Decrements `buffer.head` by `count` (mod `totalRows`), clearing the new
     * top rows.  Calls `markAllDirty()` because every visible row changes.
     *
     * @param count  Number of lines to scroll down (must be > 0).
     * @param fill   Cell value for newly exposed rows.
     * @note READER THREAD — lock-free, noexcept.
     */
    void scrollDown (int count, const Cell& fill = Cell {}) noexcept;

    /**
     * @brief Scrolls a sub-region of the screen up by `count` lines.
     *
     * If the region spans the full viewport, delegates to `scrollUp()` to use
     * the efficient head-pointer path.  Otherwise, shifts rows within the
     * region using `memcpy` and clears the vacated rows at the bottom.
     *
     * @param top     First row of the scroll region (inclusive, 0-based).
     * @param bottom  Last row of the scroll region (inclusive, 0-based).
     * @param count   Number of lines to scroll up.
     * @param fill    Cell value for newly exposed rows.
     * @note READER THREAD — lock-free, noexcept.
     * @see scrollRegionDown()
     */
    void scrollRegionUp (int top, int bottom, int count, const Cell& fill = Cell {}) noexcept;

    /**
     * @brief Scrolls a sub-region of the screen down by `count` lines.
     *
     * If the region spans the full viewport, delegates to `scrollDown()`.
     * Otherwise, shifts rows within the region downward and clears the
     * vacated rows at the top.
     *
     * @param top     First row of the scroll region (inclusive, 0-based).
     * @param bottom  Last row of the scroll region (inclusive, 0-based).
     * @param count   Number of lines to scroll down.
     * @param fill    Cell value for newly exposed rows.
     * @note READER THREAD — lock-free, noexcept.
     * @see scrollRegionUp()
     */
    void scrollRegionDown (int top, int bottom, int count, const Cell& fill = Cell {}) noexcept;

    /** @} */

    /** @name Erase operations
     *  All erase methods operate on the active screen buffer and are called
     *  exclusively on the READER THREAD.
     * @{ */

    /**
     * @brief Erases all cells in visible row `row` and resets its RowState.
     *
     * Fills the cell row with default-constructed Cells, zero-fills the
     * grapheme row, and resets the RowState.  Calls `markRowDirty (row)`.
     *
     * @param row   Zero-based visible row index.
     * @param fill  Cell value for erased positions.
     * @note READER THREAD — lock-free, noexcept.
     */
    void eraseRow (int row, const Cell& fill = Cell {}) noexcept;

    /**
     * @brief Erases all rows in the inclusive range [startRow, endRow].
     *
     * Calls `eraseRow()` for each row in the range.
     *
     * @param startRow  First row to erase (inclusive, 0-based).
     * @param endRow    Last row to erase (inclusive, 0-based).
     * @param fill      Cell value for erased positions.
     * @note READER THREAD — lock-free, noexcept.
     */
    void eraseRowRange (int startRow, int endRow, const Cell& fill = Cell {}) noexcept;

    /**
     * @brief Erases the single cell at (row, col).
     *
     * Writes the provided fill cell and calls `markRowDirty (row)`.
     *
     * @param row   Zero-based visible row index.
     * @param col   Zero-based column index.
     * @param fill  Cell value for the erased position.
     * @note READER THREAD — lock-free, noexcept.
     */
    void eraseCell (int row, int col, const Cell& fill = Cell {}) noexcept;

    /**
     * @brief Erases cells in the inclusive column range [startCol, endCol] on row `row`.
     *
     * Clamps the column range to [0, cols-1] before writing.  Calls
     * `markRowDirty (row)`.
     *
     * @param row       Zero-based visible row index.
     * @param startCol  First column to erase (inclusive).
     * @param endCol    Last column to erase (inclusive).
     * @param fill      Cell value for erased positions.
     * @note READER THREAD — lock-free, noexcept.
     */
    void eraseCellRange (int row, int startCol, int endCol, const Cell& fill = Cell {}) noexcept;

    /** @} */

    /**
     * @brief Returns a raw pointer to the start of visible row `row` for bulk writes.
     *
     * Bypasses bounds checking for performance on the hot write path.  The
     * caller is responsible for staying within [0, cols) and must call
     * `markRowDirty (row)` after writing.
     *
     * @param visibleRow  Zero-based visible row index.
     * @return Pointer to the first Cell in the row.
     * @note READER THREAD — lock-free, noexcept.
     *       Caller must markRowDirty after writing.
     */
    Cell* directRowPtr (int visibleRow) noexcept;

    //==========================================================================
    /**
     * @brief Write-only facade for Parser access to the Grid buffer.
     *
     * Restricts the caller to cell writes, scrolling, erasing, and dirty
     * tracking — no geometry reads, no resize, no extract, no lock access.
     * All methods are inline passthroughs with zero overhead.
     *
     * @note Constructed from a Grid reference.  Lifetime must not exceed
     *       the Grid it wraps.
     */
    class Writer
    {
    public:
        explicit Writer (Grid& g) noexcept : grid (g) {}

        // -- Hot path (per-character) --
        Cell*              directRowPtr (int visibleRow) noexcept                                         { return grid.directRowPtr (visibleRow); }
        void               batchMarkDirty (const uint64_t localDirty[4]) noexcept                        { grid.batchMarkDirty (localDirty); }
        void               markRowDirty (int row) noexcept                                               { grid.markRowDirty (row); }
        void               activeWriteCell (int row, int col, const Cell& cellState) noexcept            { grid.activeWriteCell (row, col, cellState); }
        void               activeWriteGrapheme (int row, int col, const Grapheme& grapheme) noexcept     { grid.activeWriteGrapheme (row, col, grapheme); }
        void               activeWriteImage (int startRow, int startCol, uint32_t imageId, int widthPx, int heightPx, int cellWidthPx, int cellHeightPx) noexcept { grid.activeWriteImage (startRow, startCol, imageId, widthPx, heightPx, cellWidthPx, cellHeightPx); }
        uint32_t           reserveImageId() noexcept                                                                                                             { return grid.reserveImageId(); }
        void               storeDecodedImage (PendingImage&& img) noexcept                                                                                       { grid.storeDecodedImage (std::move (img)); }
        void               activeEraseGrapheme (int row, int col) noexcept                               { grid.activeEraseGrapheme (row, col); }
        Cell*              activeVisibleRow (int row) noexcept                                           { return grid.activeVisibleRow (row); }
        const Cell*        activeVisibleRow (int row) const noexcept                                     { return grid.activeVisibleRow (row); }
        const Grapheme*    activeReadGrapheme (int row, int col) const noexcept                          { return grid.activeReadGrapheme (row, col); }
        RowState&          activeVisibleRowState (int row) noexcept                                      { return grid.activeVisibleRowState (row); }
        const RowState&    activeVisibleRowState (int row) const noexcept                                { return grid.activeVisibleRowState (row); }

        // -- Scroll --
        void scrollRegionUp (int top, int bottom, int count, const Cell& fill = Cell {}) noexcept
        {
            grid.scrollRegionUp (top, bottom, count, fill);
            if (onScrollbackChanged != nullptr)
                onScrollbackChanged (grid.getScrollbackUsed());
        }

        void               scrollRegionDown (int top, int bottom, int count, const Cell& fill = Cell {}) noexcept { grid.scrollRegionDown (top, bottom, count, fill); }

        // -- Erase --
        void               eraseRow (int row, const Cell& fill = Cell {}) noexcept                      { grid.eraseRow (row, fill); }
        void               eraseRowRange (int startRow, int endRow, const Cell& fill = Cell {}) noexcept { grid.eraseRowRange (startRow, endRow, fill); }
        void               eraseCellRange (int row, int startCol, int endCol, const Cell& fill = Cell {}) noexcept { grid.eraseCellRange (row, startCol, endCol, fill); }

        // -- Buffer management --
        void clearScrollback() noexcept
        {
            grid.clearScrollback();
            if (onScrollbackChanged != nullptr)
                onScrollbackChanged (grid.getScrollbackUsed());
        }

        void clearBuffer()
        {
            grid.clearBuffer();
            if (onScrollbackChanged != nullptr)
                onScrollbackChanged (grid.getScrollbackUsed());
        }

        void               markAllDirty() noexcept          { grid.markAllDirty(); }

        /** Fires after scroll or clear operations with the updated scrollback count. */
        std::function<void (int)> onScrollbackChanged;

    private:
        Grid& grid;
    };

private:
    /**
     * @struct Buffer
     * @brief Internal ring-buffer storage for one screen (normal or alternate).
     *
     * All three HeapBlocks (`cells`, `rowStates`, `graphemes`) are allocated
     * to `totalRows × cols` entries (rowStates to `totalRows` only).  `totalRows` is always a power of two so
     * that `physicalRow()` can use `& rowMask` instead of `% totalRows`.
     *
     * ### Ring-buffer invariant
     * `head` is the physical index of the **bottom** visible row.  The top
     * visible row is at physical index `(head - visibleRows + 1) & rowMask`.
     * Scrollback rows occupy the physical rows immediately before the visible
     * window (wrapping around the ring).
     */
    struct Buffer
    {
        /**
         * @brief Flat cell array: `totalRows × cols` Cell entries.
         *
         * Indexed as `cells[physicalRow * cols + col]`.  Allocated with
         * `false` (no zero-init) and explicitly filled with default Cells
         * in `initBuffer()`.
         */
        juce::HeapBlock<Cell> cells;

        /**
         * @brief Per-row metadata: `totalRows` RowState entries.
         *
         * Indexed by physical row index.  Allocated with `true` (zero-init).
         */
        juce::HeapBlock<RowState> rowStates;

        /**
         * @brief Grapheme sidecar: `totalRows × cols` Grapheme entries.
         *
         * Co-indexed with `cells`.  A Grapheme entry is valid only when the
         * corresponding Cell has `Cell::LAYOUT_GRAPHEME` set.  Allocated with
         * `true` (zero-init).
         */
        juce::HeapBlock<Grapheme> graphemes;

        /**
         * @brief Per-row write sequence numbers: `totalRows` entries, parallel to `rowStates`.
         *
         * `rowSeqnos[physicalRow]` is updated by `markRowDirty()`,
         * `batchMarkDirty()`, and `markAllDirty()` to the Grid-level `seqno`
         * counter at the time the row was last dirtied.  Allocated with
         * `true` (zero-init).
         *
         * `currentSeqno()` returns the current value for client use.
         */
        juce::HeapBlock<uint64_t> rowSeqnos;

        /**
         * @brief Total number of rows allocated (always a power of two).
         *
         * Equals `visibleRows + scrollbackCapacity` rounded up to the next
         * power of two for the normal buffer; `visibleRows` rounded up for
         * the alternate buffer.
         */
        int totalRows { 0 };

        /**
         * @brief Bitmask for modular row indexing: `totalRows - 1`.
         *
         * Used in `physicalRow()` as `(raw) & rowMask` instead of `% totalRows`.
         */
        int rowMask { 0 };

        /**
         * @brief Physical index of the bottom visible row (the ring-buffer write head).
         *
         * Incremented (mod `totalRows`) by `scrollUp()` to advance the ring
         * without moving data.  Decremented by `scrollDown()`.
         * Initialised to `visibleRows - 1` so that visible row 0 maps to
         * physical row 0 before any scrolling.
         */
        int head { 0 };

        /**
         * @brief Number of scrollback rows currently stored (0 … scrollbackCapacity).
         *
         * Incremented by `scrollUp()` on the normal screen until it reaches
         * `scrollbackCapacity`.  Always 0 for the alternate screen.
         */
        int scrollbackUsed { 0 };

        /** @brief Column count this buffer was allocated for. */
        int allocatedCols { 0 };

        /** @brief Visible row count this buffer was allocated for. */
        int allocatedVisibleRows { 0 };

    };

    /**
     * @brief Lock that serialises `resize()` / `clearBuffer()` against `extractText()`.
     *
     * The MESSAGE THREAD acquires this lock during `resize()`.  The READER
     * THREAD acquires it during `clearBuffer()` and data processing
     * (`tty->onData`).  The MESSAGE THREAD acquires it during text extraction.
     * No other methods require the lock.
     */
    mutable juce::CriticalSection resizeLock;

    // =========================================================================
    // Decoded image storage (READER writes, MESSAGE reads on demand)
    // =========================================================================

    /**
     * @brief Monotonically increasing image ID counter for pending images.
     *
     * The READER THREAD atomically increments this in `reserveImageId()`.
     * Starts at 1 so that 0 remains the "invalid" sentinel.
     *
     * @note Atomic — safe for READER THREAD increment + MESSAGE THREAD read.
     */
    std::atomic<uint32_t> nextImageId { 1 };

    /**
     * @brief Decoded RGBA pixel data keyed by imageId.
     *
     * READER stores after decode.  MESSAGE reads on first atlas encounter.
     * Same role as codepoints in cells — source data for atlas to consume.
     */
    std::unordered_map<uint32_t, PendingImage> decodedImages;

    /**
     * @brief Reference to the terminal parameter store.
     *
     * Used to read `getCols()`, `getVisibleRows()`, and the active screen index.
     * Never written by Grid.
     */
    State& state;

    /**
     * @brief Maximum number of scrollback rows for the normal screen buffer.
     *
     * Read from `Config` at construction and never changed.  The alternate
     * screen always has zero scrollback.
     */
    int scrollbackCapacity { 0 };

    /**
     * @brief 256-bit atomic dirty bitmask: one bit per visible row (rows 0–255).
     *
     * `dirtyRows[w]` covers rows `w*64` through `w*64 + 63`.  Bit `r & 63`
     * in word `r >> 6` is set by `markRowDirty (r)` and cleared atomically
     * by `consumeDirtyRows()`.  All operations use `memory_order_relaxed`
     * except `consumeDirtyRows()` which uses `memory_order_acq_rel`.
     */
    std::atomic<uint64_t> dirtyRows[4] { {~ uint64_t {0}}, {~ uint64_t {0}}, {~ uint64_t {0}}, {~ uint64_t {0}} };

    /**
     * @brief Monotonic write sequence number.
     *
     * Incremented by `markRowDirty()`, `batchMarkDirty()`, and `markAllDirty()`
     * each time one or more rows are dirtied.  The per-row seqno in
     * `Buffer::rowSeqnos` records the value of this counter at the time each
     * row was last marked dirty.
     *
     * Per-row seqnos in `Buffer::rowSeqnos` record the value of this counter
     * at the time each row was last marked dirty.
     *
     * @note READER THREAD writes (relaxed).  MESSAGE THREAD reads (relaxed).
     *       Same concurrency contract as `dirtyRows`.
     */
    std::atomic<uint64_t> seqno { 0 };

    /**
     * @brief Net lines scrolled up since the last `consumeScrollDelta()` call.
     *
     * Incremented by `scrollUp()` and reset to zero by `markAllDirty()` and
     * `consumeScrollDelta()`.  The MESSAGE THREAD reads this to drive smooth
     * scroll animation.
     */
    std::atomic<int> scrollDelta { 0 };

    /**
     * @brief The two screen buffers: index 0 = normal, index 1 = alternate.
     *
     * Indexed by `ActiveScreen` enum values (`normal = 0`, `alternate = 1`).
     * `bufferForScreen()` selects the active buffer via `state.getScreen()`.
     */
    std::array<Buffer, 2> buffers;

    /**
     * @brief Returns a mutable reference to the buffer for the currently active screen.
     * @return Reference to `buffers[state.getScreen()]`.
     * @note READER THREAD — lock-free, noexcept.
     */
    Buffer& bufferForScreen() noexcept;

    /**
     * @brief Returns a const reference to the buffer for the currently active screen.
     * @return Const reference to `buffers[state.getScreen()]`.
     * @note MESSAGE THREAD — lock-free, noexcept.
     */
    const Buffer& bufferForScreen() const noexcept;

    /**
     * @brief Converts a visible row index to a physical ring-buffer row index.
     *
     * @par Ring-buffer math
     * @code
     * physicalRow = (head - visibleRows + 1 + visibleRow) & rowMask
     * @endcode
     * Because `totalRows` is a power of two, the modular reduction is a
     * single bitwise AND.  Negative intermediate values wrap correctly because
     * `head` is always in [0, totalRows) and `rowMask = totalRows - 1`.
     *
     * @param buffer      The buffer whose ring geometry to use.
     * @param visibleRow  Zero-based visible row index (0 = top of viewport).
     * @return Physical row index in [0, buffer.totalRows).
     * @note Lock-free, noexcept.
     */
    int physicalRow (const Buffer& buffer, int visibleRow) const noexcept;

    /**
     * @brief Returns a mutable pointer to the start of a visible row in `buffer`.
     *
     * @param buffer      Target buffer.
     * @param visibleRow  Zero-based visible row index.
     * @return Pointer to `buffer.cells[physicalRow * cols]`.
     * @note READER THREAD — lock-free, noexcept.
     */
    Cell* rowPtr (Buffer& buffer, int visibleRow) noexcept;

    /**
     * @brief Returns a const pointer to the start of a visible row in `buffer`.
     *
     * @param buffer      Target buffer.
     * @param visibleRow  Zero-based visible row index.
     * @return Const pointer to `buffer.cells[physicalRow * cols]`.
     * @note MESSAGE THREAD — lock-free, noexcept.
     */
    const Cell* rowPtr (const Buffer& buffer, int visibleRow) const noexcept;

    /**
     * @brief Allocates and initialises a Buffer to the given dimensions.
     *
     * Rounds `totalLines` up to the next power of two, allocates all four
     * HeapBlocks, fills cells with default-constructed Cells, and sets
     * `head = numVisibleRows - 1` so that visible row 0 maps to physical row 0.
     *
     * @param buffer         Buffer to initialise (existing allocation is replaced).
     * @param numCols        Number of columns.
     * @param totalLines     Desired total rows (rounded up to next power of two).
     * @param numVisibleRows Number of visible rows (used to set `head`).
     * @note READER THREAD — allocates heap memory.
     */
    void initBuffer (Buffer& buffer, int numCols, int totalLines, int numVisibleRows);

    /**
     * @brief Shared implementation for `scrollRegionUp()` and `scrollRegionDown()`.
     *
     * Validates the region bounds, clamps `count` to the region size, and
     * delegates to `shiftRegionUp()` or `shiftRegionDown()`.  Marks all rows
     * in [top, bottom] dirty.
     *
     * @param top     First row of the scroll region (inclusive, 0-based).
     * @param bottom  Last row of the scroll region (inclusive, 0-based).
     * @param count   Number of lines to shift.
     * @param up      `true` to shift up, `false` to shift down.
     * @note READER THREAD — lock-free, noexcept.
     */
    void scrollRegion (int top, int bottom, int count, bool up, const Cell& fill = Cell {}) noexcept;

    void shiftRegionUp (Buffer& buffer, int top, int bottom, int ec, size_t rowBytes, const Cell& fill = Cell {}) noexcept;
    void shiftRegionDown (Buffer& buffer, int top, int bottom, int ec, size_t rowBytes, const Cell& fill = Cell {}) noexcept;

    /**
     * @brief Fills all cells in visible row `visibleRow` with `fill`,
     *        resets graphemes and RowState.
     *
     * @param buffer      Target buffer.
     * @param visibleRow  Zero-based visible row index.
     * @param fill        Cell value to fill with.
     * @note READER THREAD — lock-free, noexcept.
     */
    void clearRow (Buffer& buffer, int visibleRow, const Cell& fill = Cell {}) noexcept;

    /**
     * @brief Returns a mutable pointer to the grapheme entry at (visibleRow, col).
     *
     * @param buffer      Target buffer.
     * @param visibleRow  Zero-based visible row index.
     * @param col         Zero-based column index.
     * @return Pointer to `buffer.graphemes[physicalRow * cols + col]`.
     * @note READER THREAD — lock-free, noexcept.
     */
    Grapheme* graphemePtr (Buffer& buffer, int visibleRow, int col) noexcept;

    /**
     * @brief Returns a const pointer to the grapheme entry at (visibleRow, col).
     *
     * @param buffer      Target buffer.
     * @param visibleRow  Zero-based visible row index.
     * @param col         Zero-based column index.
     * @return Const pointer to `buffer.graphemes[physicalRow * cols + col]`.
     * @note MESSAGE THREAD — lock-free, noexcept.
     */
    const Grapheme* graphemePtr (const Buffer& buffer, int visibleRow, int col) const noexcept;

    /**
     * @brief Reflows the content of `oldBuffer` into `newBuffer` at the new column width.
     *
     * Walks every logical line in `oldBuffer` (following soft-wrap chains),
     * flattens each logical line into a temporary buffer, then re-wraps it at
     * `newCols` and writes the result into `newBuffer`.  Preserves as much
     * scrollback as fits in the new buffer.
     *
     * @par Algorithm overview
     * 1. `findLastContent()` — find the last non-empty visible row.
     * 2. `reflowPass()` (dry-run) — compute total output row count.
     * 3. `reflowPass()` (write)   — write rows, skipping overflow at the top.
     * 4. Update `newBuffer.head` and `newBuffer.scrollbackUsed`.
     *
     * @param oldBuffer      Source buffer (read-only).
     * @param oldCols        Column count of the source buffer.
     * @param oldVisibleRows Visible row count of the source buffer.
     * @param newBuffer      Destination buffer (already allocated by `initBuffer()`).
     * @param newCols        Column count of the destination buffer.
     * @param newVisibleRows Visible row count of the destination buffer.
     * @note MESSAGE THREAD — allocates temporary heap memory for the flat buffer.
     */
    void reflow (const Buffer& oldBuffer,
                        int oldCols,
                        int oldVisibleRows,
                        Buffer& newBuffer,
                        int newCols,
                        int newVisibleRows);
};

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Terminal
