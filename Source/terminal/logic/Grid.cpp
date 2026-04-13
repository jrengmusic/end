/**
 * @file Grid.cpp
 * @brief Ring-buffer cell storage — constructor, buffer init, dirty tracking,
 *        cell access, row access, scrollback, and text extraction.
 *
 * This translation unit implements the core Grid methods.  Scroll, erase, and
 * reflow operations are split across GridScroll.cpp, GridErase.cpp, and
 * GridReflow.cpp respectively to keep each file focused.
 *
 * ## Ring-buffer indexing
 *
 * Every visible row index is converted to a physical flat-array index by:
 * @code
 * physicalRow = (head - visibleRows + 1 + visibleRow) & rowMask
 * @endcode
 * `head` is the physical index of the **bottom** visible row.  Because
 * `totalRows` is always a power of two, `rowMask = totalRows - 1` and the
 * modular reduction is a single bitwise AND — no division, no branch.
 *
 * ## Dirty tracking
 *
 * `dirtyRows[4]` is a 256-bit bitmask stored as four `std::atomic<uint64_t>`.
 * The READER THREAD sets bits with `fetch_or (bit, memory_order_relaxed)`.
 * The MESSAGE THREAD atomically swaps each word to zero with
 * `exchange (0, memory_order_acq_rel)`, which provides the acquire fence
 * needed to see all cell writes that preceded the dirty mark.
 *
 * @see Grid.h for the full class documentation.
 */

#include "Grid.h"
#include "../data/State.h"
#include "../../config/Config.h"

namespace Terminal
{ /*____________________________________________________________________________*/

// ============================================================================
// Constructor
// ============================================================================

/**
 * @brief Constructs the Grid and allocates both screen buffers.
 *
 * Reads `cols` and `visibleRows` from `initState` and `scrollbackCapacity`
 * from `Config`.  Allocates the normal buffer with `visibleRows +
 * scrollbackCapacity` total rows and the alternate buffer with `visibleRows`
 * total rows (no scrollback).
 *
 * @param initState  Terminal parameter store; must outlive this Grid.
 * @note MESSAGE THREAD — called during terminal initialisation.
 */
Grid::Grid (State& initState)
    : state (initState)
    , scrollbackCapacity { Config::getContext()->getInt (Config::Key::terminalScrollbackLines) }
{
    initBuffer (buffers.at (normal), state.getCols(), state.getVisibleRows() + scrollbackCapacity, state.getVisibleRows());
    initBuffer (buffers.at (alternate), state.getCols(), state.getVisibleRows(), state.getVisibleRows());
}

/**
 * @brief Returns the resize lock used to synchronise resize and text extraction.
 *
 * @return Reference to the internal `juce::CriticalSection`.
 * @note Lock-free getter — the lock itself is not acquired here.
 */
juce::CriticalSection& Grid::getResizeLock() noexcept { return resizeLock; }
juce::CriticalSection& Grid::getResizeLock() const noexcept { return resizeLock; }

/**
 * @brief Clears the active screen buffer and marks all rows dirty.
 *
 * Acquires `resizeLock`, re-initialises the active buffer to the current
 * dimensions, and calls `markAllDirty()`.  The normal buffer retains its
 * scrollback capacity; the alternate buffer has none.
 *
 * @note READER THREAD — acquires `resizeLock`.
 */
void Grid::clearBuffer()
{
    juce::ScopedLock lock (resizeLock);
    const auto scr { state.getRawValue<ActiveScreen> (Terminal::ID::activeScreen) };

    if (scr == normal)
    {
        initBuffer (buffers.at (scr), getCols(), getVisibleRows() + scrollbackCapacity, getVisibleRows());
    }
    else
    {
        initBuffer (buffers.at (scr), getCols(), getVisibleRows(), getVisibleRows());
    }

    markAllDirty();
}

void Grid::clearScrollback() noexcept
{
    bufferForScreen().scrollbackUsed = 0;
}

// ============================================================================
// Region Initialization
// ============================================================================

/**
 * @brief Allocates and initialises a Buffer to the given dimensions.
 *
 * Rounds `totalLines` up to the next power of two so that `physicalRow()`
 * can use a bitwise AND instead of a modulo.  Allocates all three HeapBlocks
 * (`cells`, `rowStates`, `graphemes`), fills cells with default-constructed
 * Cells, and sets `head = numVisibleRows - 1` so that visible row 0 maps to
 * physical row 0 before any scrolling occurs.
 *
 * @par Power-of-two rounding
 * @code
 * int po2 { 1 };
 * while (po2 < totalLines) po2 <<= 1;
 * @endcode
 * This ensures `buffer.rowMask = po2 - 1` is a valid bitmask.
 *
 * @param buffer         Buffer to initialise (existing allocation is replaced).
 * @param numCols        Number of columns.
 * @param totalLines     Desired total rows (rounded up to next power of two).
 * @param numVisibleRows Number of visible rows (used to set `head`).
 * @note Called from both MESSAGE THREAD (`resize()`, constructor) and READER
 *       THREAD (`clearBuffer()`).  Allocates heap memory.
 */
void Grid::initBuffer (Buffer& buffer, int numCols, int totalLines, int numVisibleRows)
{
    // Round up to next power of 2 for bitwise AND in physicalRow
    int po2 { 1 };
    while (po2 < totalLines)
        po2 <<= 1;

    const size_t cellCount { static_cast<size_t> (po2) * static_cast<size_t> (numCols) };
    buffer.cells.allocate (cellCount, false);
    buffer.rowStates.allocate (static_cast<size_t> (po2), true);
    buffer.rowSeqnos.allocate (static_cast<size_t> (po2), true);  // zero-init

    const Cell defaultCell {};
    std::fill (buffer.cells.get(), buffer.cells.get() + cellCount, defaultCell);

    buffer.graphemes.allocate (cellCount, true);

    buffer.totalRows = po2;
    buffer.rowMask = po2 - 1;
    buffer.head = numVisibleRows - 1;
    buffer.scrollbackUsed = 0;
    buffer.allocatedCols = numCols;
    buffer.allocatedVisibleRows = numVisibleRows;
}

// ============================================================================
// Dirty Row Tracking
// ============================================================================

/**
 * @brief Marks a single visible row as dirty in the atomic bitmask.
 *
 * Computes the word index as `row >> 6` and the bit position as `row & 63`,
 * then OR-sets the bit with `memory_order_relaxed`.  Rows outside [0, 255]
 * are silently ignored.  Calls `State::setSnapshotDirty()` to wake the
 * MESSAGE THREAD.
 *
 * @par Bit layout
 * @code
 * word  = row >> 6          // 0..3
 * bit   = 1ULL << (row & 63)
 * dirtyRows[word] |= bit    // atomic fetch_or, relaxed
 * @endcode
 *
 * @param row  Zero-based visible row index (0 … 255).
 * @note READER THREAD — lock-free, noexcept.
 */
void Grid::markRowDirty (int row) noexcept
{
    static constexpr int maxTrackedRows { 256 };

    if (row >= 0 and row < maxTrackedRows)
    {
        const int word { row >> 6 };
        const uint64_t bit { uint64_t { 1 } << (row & 63) };
        dirtyRows[word].fetch_or (bit, std::memory_order_relaxed);

        const uint64_t currentSeqno { seqno.fetch_add (1, std::memory_order_relaxed) + 1 };
        auto& buffer { bufferForScreen() };
        const int physRow { physicalRow (buffer, row) };
        buffer.rowSeqnos[static_cast<size_t> (physRow)] = currentSeqno;

        state.setSnapshotDirty();
    }
}

/**
 * @brief OR-merges a local 256-bit dirty bitmask into the shared bitmask.
 *
 * Iterates the four words and skips any that are zero to avoid unnecessary
 * atomic operations.  Calls `State::setSnapshotDirty()` once after all words
 * are merged.
 *
 * @param localDirty  Four `uint64_t` words (rows 0–63, 64–127, 128–191, 192–255).
 * @note READER THREAD — lock-free, noexcept.
 */
void Grid::batchMarkDirty (const uint64_t localDirty[4]) noexcept
{
    const uint64_t currentSeqno { seqno.fetch_add (1, std::memory_order_relaxed) + 1 };
    auto& buffer { bufferForScreen() };
    const int visibleRows { getVisibleRows() };

    for (int i { 0 }; i < 4; ++i)
    {
        if (localDirty[i] != 0)
        {
            dirtyRows[i].fetch_or (localDirty[i], std::memory_order_relaxed);

            for (int bit { 0 }; bit < 64; ++bit)
            {
                if ((localDirty[i] & (uint64_t { 1 } << bit)) != 0)
                {
                    const int row { i * 64 + bit };

                    if (row < visibleRows)
                    {
                        const int physRow { physicalRow (buffer, row) };
                        buffer.rowSeqnos[static_cast<size_t> (physRow)] = currentSeqno;
                    }
                }
            }
        }
    }

    state.setSnapshotDirty();
}

/**
 * @brief Marks all 256 tracked rows dirty and resets the scroll delta.
 *
 * Stores `~uint64_t{0}` (all bits set) into all four `dirtyRows` words and
 * resets `scrollDelta` to zero.  Used after resize, clear, or any operation
 * that invalidates the entire visible area.
 *
 * @note READER THREAD — lock-free, noexcept.
 */
void Grid::markAllDirty() noexcept
{
    for (int i { 0 }; i < 4; ++i)
        dirtyRows[i].store (~uint64_t { 0 }, std::memory_order_relaxed);

    scrollDelta.store (0, std::memory_order_relaxed);

    const uint64_t currentSeqno { seqno.fetch_add (1, std::memory_order_relaxed) + 1 };
    auto& buffer { bufferForScreen() };

    for (int row { 0 }; row < buffer.totalRows; ++row)
        buffer.rowSeqnos[static_cast<size_t> (row)] = currentSeqno;

    state.setSnapshotDirty();
}

/**
 * @brief Atomically swaps the dirty bitmask to zero and returns the old value.
 *
 * Uses `memory_order_acq_rel` on each exchange so that all READER THREAD
 * cell writes that preceded the `markRowDirty()` calls are visible to the
 * MESSAGE THREAD after this call returns.
 *
 * @param out  Output array of four `uint64_t` words.  Bit `r` in `out[r>>6]`
 *             is set if visible row `r` was dirtied since the last call.
 * @note MESSAGE THREAD — lock-free, noexcept.
 */
void Grid::consumeDirtyRows (uint64_t out[4]) noexcept
{
    for (int i { 0 }; i < 4; ++i)
    {
        out[i] = dirtyRows[i].exchange (0, std::memory_order_acq_rel);
    }
}

/**
 * @brief Atomically swaps the scroll delta to zero and returns the old value.
 *
 * Uses `memory_order_acq_rel` to ensure ordering with the READER THREAD's
 * `scrollDelta.fetch_add()` in `scrollUp()`.
 *
 * @return Net lines scrolled up since the last call (positive = scrolled up).
 * @note MESSAGE THREAD — lock-free, noexcept.
 */
int Grid::consumeScrollDelta() noexcept
{
    return scrollDelta.exchange (0, std::memory_order_acq_rel);
}

// ============================================================================
// Buffer Access by Screen
// ============================================================================

/**
 * @brief Returns a mutable reference to the buffer for the currently active screen.
 *
 * Reads `state.getScreen()` (an atomic load) and indexes into `buffers`.
 *
 * @return Reference to `buffers[state.getScreen()]`.
 * @note READER THREAD — lock-free, noexcept.
 */
Grid::Buffer& Grid::bufferForScreen() noexcept
{
    return buffers.at (static_cast<size_t> (state.getRawValue<ActiveScreen> (Terminal::ID::activeScreen)));
}

/**
 * @brief Returns a const reference to the buffer for the currently active screen.
 *
 * @return Const reference to `buffers[state.getActiveScreen()]`.
 * @note MESSAGE THREAD — lock-free, noexcept.
 */
const Grid::Buffer& Grid::bufferForScreen() const noexcept
{
    return buffers.at (static_cast<size_t> (state.getRawValue<ActiveScreen> (Terminal::ID::activeScreen)));
}

// ============================================================================
// Physical Index Calculations
// ============================================================================

/**
 * @brief Converts a visible row index to a physical ring-buffer row index.
 *
 * @par Ring-buffer math
 * @code
 * physicalRow = (head - visibleRows + 1 + visibleRow) & rowMask
 * @endcode
 * `head` is the physical index of the bottom visible row.  Subtracting
 * `visibleRows - 1` gives the physical index of the top visible row, then
 * adding `visibleRow` offsets into the visible window.  The bitwise AND
 * wraps the result into [0, totalRows) without a branch.
 *
 * @param buffer      The buffer whose ring geometry to use.
 * @param visibleRow  Zero-based visible row index (0 = top of viewport).
 * @return Physical row index in [0, buffer.totalRows).
 * @note Lock-free, noexcept.
 */
int Grid::physicalRow (const Buffer& buffer, int visibleRow) const noexcept
{
    return (buffer.head - getVisibleRows() + 1 + visibleRow) & buffer.rowMask;
}

/**
 * @brief Returns a mutable pointer to the start of a visible row in `buffer`.
 *
 * @param buffer      Target buffer.
 * @param visibleRow  Zero-based visible row index.
 * @return Pointer to `buffer.cells[physicalRow * cols]`.
 * @note READER THREAD — lock-free, noexcept.
 */
Cell* Grid::rowPtr (Buffer& buffer, int visibleRow) noexcept
{
    return buffer.cells.get() + physicalRow (buffer, visibleRow) * getCols();
}

/**
 * @brief Returns a const pointer to the start of a visible row in `buffer`.
 *
 * @param buffer      Target buffer.
 * @param visibleRow  Zero-based visible row index.
 * @return Const pointer to `buffer.cells[physicalRow * cols]`.
 * @note MESSAGE THREAD — lock-free, noexcept.
 */
const Cell* Grid::rowPtr (const Buffer& buffer, int visibleRow) const noexcept
{
    return buffer.cells.get() + physicalRow (buffer, visibleRow) * getCols();
}

// ============================================================================
// Row Access
// ============================================================================

/**
 * @brief Returns a raw pointer to the start of visible row `row` for bulk writes.
 *
 * Inlines the ring-buffer index calculation without bounds checking.  The
 * caller must stay within [0, cols) and call `markRowDirty (row)` after
 * writing.
 *
 * @param visibleRow  Zero-based visible row index.
 * @return Pointer to the first Cell in the row.
 * @note READER THREAD — lock-free, noexcept.
 *       Caller must markRowDirty after writing.
 */
Cell* Grid::directRowPtr (int visibleRow) noexcept
{
    Buffer& buffer { bufferForScreen() };
    return buffer.cells.get() + ((buffer.head - getVisibleRows() + 1 + visibleRow) & buffer.rowMask) * getCols();
}

/**
 * @brief Returns a mutable pointer to the start of visible row `row`.
 *
 * Bounds-checks `row` against [0, visibleRows) before computing the physical
 * index.  Returns `nullptr` if out of range.
 *
 * @param row  Zero-based visible row index.
 * @return Pointer to the first Cell in the row, or `nullptr` if out of range.
 * @note READER THREAD — lock-free, noexcept.
 */
Cell* Grid::activeVisibleRow (int row) noexcept
{
    if (row >= 0 and row < getVisibleRows())
    {
        Buffer& buffer { bufferForScreen() };
        return rowPtr (buffer, row);
    }
    return nullptr;
}

/**
 * @brief Returns a const pointer to the start of visible row `row`.
 *
 * @param row  Zero-based visible row index.
 * @return Const pointer to the first Cell in the row, or `nullptr` if out of range.
 * @note MESSAGE THREAD — lock-free, noexcept.
 */
const Cell* Grid::activeVisibleRow (int row) const noexcept
{
    if (row >= 0 and row < getVisibleRows())
    {
        const Buffer& buffer { bufferForScreen() };
        return rowPtr (buffer, row);
    }
    return nullptr;
}

/**
 * @brief Returns a const pointer to the grapheme row for visible row `row`.
 *
 * The returned pointer addresses `cols` Grapheme entries co-indexed with the
 * cell row.  Returns `nullptr` if `row` is out of range.
 *
 * @param row  Zero-based visible row index.
 * @return Pointer to the first Grapheme in the row, or `nullptr` if out of range.
 * @note MESSAGE THREAD — lock-free, noexcept.
 */
const Grapheme* Grid::activeVisibleGraphemeRow (int row) const noexcept
{
    if (row >= 0 and row < getVisibleRows())
    {
        const Buffer& buffer { bufferForScreen() };
        return buffer.graphemes.get() + physicalRow (buffer, row) * getCols();
    }
    return nullptr;
}

/**
 * @brief Returns the number of scrollback rows currently stored.
 *
 * Reads `scrollbackUsed` from the active buffer.  Only meaningful for the
 * normal screen; always 0 for the alternate screen.
 *
 * @return Number of scrollback rows available (0 … scrollbackCapacity).
 * @note MESSAGE THREAD — lock-free, noexcept.
 */
int Grid::getScrollbackUsed() const noexcept
{
    const Buffer& buffer { bufferForScreen() };
    return buffer.scrollbackUsed;
}

/**
 * @brief Returns a const pointer to a scrolled-back cell row.
 *
 * Applies `scrollOffset` to the ring-buffer index:
 * @code
 * phys = (head - visibleRows + 1 + visibleRow - scrollOffset) & rowMask
 * @endcode
 * A `scrollOffset` of 1 returns the row one line above the live view.
 * Returns `nullptr` if `visibleRow` is out of range.
 *
 * @param visibleRow   Zero-based visible row index (0 … visibleRows-1).
 * @param scrollOffset Lines scrolled back (0 = live view).
 * @return Const pointer to the first Cell in the scrolled row, or `nullptr`.
 * @note MESSAGE THREAD — lock-free, noexcept.
 */
const Cell* Grid::scrollbackRow (int visibleRow, int scrollOffset) const noexcept
{
    if (visibleRow >= 0 and visibleRow < getVisibleRows())
    {
        const Buffer& buffer { bufferForScreen() };
        const int phys { (buffer.head - getVisibleRows() + 1 + visibleRow - scrollOffset) & buffer.rowMask };
        return buffer.cells.get() + phys * getCols();
    }
    return nullptr;
}

/**
 * @brief Returns a const pointer to a scrolled-back grapheme row.
 *
 * Parallel to `scrollbackRow()` but for the grapheme sidecar array.
 * Returns `nullptr` if `visibleRow` is out of range.
 *
 * @param visibleRow   Zero-based visible row index (0 … visibleRows-1).
 * @param scrollOffset Lines scrolled back (0 = live view).
 * @return Const pointer to the first Grapheme in the scrolled row, or `nullptr`.
 * @note MESSAGE THREAD — lock-free, noexcept.
 */
const Grapheme* Grid::scrollbackGraphemeRow (int visibleRow, int scrollOffset) const noexcept
{
    if (visibleRow >= 0 and visibleRow < getVisibleRows())
    {
        const Buffer& buffer { bufferForScreen() };
        const int phys { (buffer.head - getVisibleRows() + 1 + visibleRow - scrollOffset) & buffer.rowMask };
        return buffer.graphemes.get() + phys * getCols();
    }
    return nullptr;
}

/**
 * @brief Returns a mutable reference to the RowState for visible row `row`.
 *
 * @param row  Zero-based visible row index.
 * @return Reference to the RowState entry for the physical row.
 * @note READER THREAD — lock-free, noexcept.
 */
RowState& Grid::activeVisibleRowState (int row) noexcept
{
    Buffer& buffer { bufferForScreen() };
    const int phys { physicalRow (buffer, row) };
    return buffer.rowStates[phys];
}

/**
 * @brief Returns a const reference to the RowState for visible row `row`.
 *
 * @param row  Zero-based visible row index.
 * @return Const reference to the RowState entry for the physical row.
 * @note MESSAGE THREAD — lock-free, noexcept.
 */
const RowState& Grid::activeVisibleRowState (int row) const noexcept
{
    const Buffer& buffer { bufferForScreen() };
    const int phys { physicalRow (buffer, row) };
    return buffer.rowStates[phys];
}

// ============================================================================
// Cell Write/Read
// ============================================================================

/**
 * @brief Writes a single cell to the active screen at (row, col).
 *
 * Bounds-checks both coordinates, writes the cell into the ring-buffer row,
 * and calls `markRowDirty (row)`.
 *
 * @param row        Zero-based visible row index.
 * @param col        Zero-based column index.
 * @param cellState  Cell value to write.
 * @note READER THREAD — lock-free, noexcept.
 */
void Grid::activeWriteCell (int row, int col, const Cell& cellState) noexcept
{
    if (row >= 0 and row < getVisibleRows() and col >= 0 and col < getCols())
    {
        Buffer& buffer { bufferForScreen() };
        rowPtr (buffer, row)[col] = cellState;
        markRowDirty (row);
    }
}

/**
 * @brief Writes a contiguous run of cells to the active screen.
 *
 * Validates that the entire range [startCol, startCol+count) lies within
 * [0, cols), then copies `count` cells using `std::memcpy`.  Calls
 * `markRowDirty (row)` on success.
 *
 * @param row       Zero-based visible row index.
 * @param startCol  First destination column.
 * @param cells     Source cell array; must contain at least `count` elements.
 * @param count     Number of cells to copy (must be > 0).
 * @note READER THREAD — lock-free, noexcept.
 */
void Grid::activeWriteRun (int row, int startCol, const Cell* cells, int count) noexcept
{
    if (row >= 0 and row < getVisibleRows()
        and startCol >= 0 and startCol + count <= getCols()
        and count > 0)
    {
        Buffer& buffer { bufferForScreen() };
        Cell* dst { rowPtr (buffer, row) + startCol };
        std::memcpy (dst, cells, static_cast<size_t> (count) * sizeof (Cell));
        markRowDirty (row);
    }
}

/**
 * @brief Writes a grapheme cluster sidecar for the cell at (row, col).
 *
 * Sets `Cell::LAYOUT_GRAPHEME` on the target cell and writes `grapheme` into
 * the parallel grapheme block at the same physical offset.  Calls
 * `markRowDirty (row)`.
 *
 * @param row      Zero-based visible row index.
 * @param col      Zero-based column index.
 * @param grapheme Grapheme cluster to store.
 * @note READER THREAD — lock-free, noexcept.
 * @see activeEraseGrapheme(), activeReadGrapheme()
 */
void Grid::activeWriteGrapheme (int row, int col, const Grapheme& grapheme) noexcept
{
    if (row >= 0 and row < getVisibleRows() and col >= 0 and col < getCols())
    {
        Buffer& buffer { bufferForScreen() };
        rowPtr (buffer, row)[col].layout |= Cell::LAYOUT_GRAPHEME;
        *graphemePtr (buffer, row, col) = grapheme;
        markRowDirty (row);
    }
}

/**
 * @brief Returns a pointer to the grapheme cluster for the cell at (row, col).
 *
 * Returns `nullptr` if the coordinates are out of range or if the cell does
 * not have `Cell::LAYOUT_GRAPHEME` set.
 *
 * @param row  Zero-based visible row index.
 * @param col  Zero-based column index.
 * @return Const pointer to the Grapheme entry, or `nullptr`.
 * @note MESSAGE THREAD — lock-free, noexcept.
 */
const Grapheme* Grid::activeReadGrapheme (int row, int col) const noexcept
{
    if (row >= 0 and row < getVisibleRows() and col >= 0 and col < getCols())
    {
        const Buffer& buffer { bufferForScreen() };
        const Cell& cell { rowPtr (buffer, row)[col] };

        if (cell.hasGrapheme())
        {
            return graphemePtr (buffer, row, col);
        }
    }

    return nullptr;
}

/**
 * @brief Clears the grapheme cluster sidecar for the cell at (row, col).
 *
 * Clears `Cell::LAYOUT_GRAPHEME` and zero-fills the grapheme entry.  No-op
 * if the cell does not have a grapheme.  Calls `markRowDirty (row)`.
 *
 * @param row  Zero-based visible row index.
 * @param col  Zero-based column index.
 * @note READER THREAD — lock-free, noexcept.
 */
void Grid::activeEraseGrapheme (int row, int col) noexcept
{
    if (row >= 0 and row < getVisibleRows() and col >= 0 and col < getCols())
    {
        Buffer& buffer { bufferForScreen() };
        Cell& cell { rowPtr (buffer, row)[col] };

        if (cell.hasGrapheme())
        {
            cell.layout &= ~Cell::LAYOUT_GRAPHEME;
            *graphemePtr (buffer, row, col) = Grapheme {};
            markRowDirty (row);
        }
    }
}

/**
 * @brief Returns a mutable pointer to the grapheme entry at (visibleRow, col).
 *
 * @param buffer      Target buffer.
 * @param visibleRow  Zero-based visible row index.
 * @param col         Zero-based column index.
 * @return Pointer to `buffer.graphemes[physicalRow * cols + col]`.
 * @note READER THREAD — lock-free, noexcept.
 */
Grapheme* Grid::graphemePtr (Buffer& buffer, int visibleRow, int col) noexcept
{
    return buffer.graphemes.get() + physicalRow (buffer, visibleRow) * getCols() + col;
}

/**
 * @brief Returns a const pointer to the grapheme entry at (visibleRow, col).
 *
 * @param buffer      Target buffer.
 * @param visibleRow  Zero-based visible row index.
 * @param col         Zero-based column index.
 * @return Const pointer to `buffer.graphemes[physicalRow * cols + col]`.
 * @note MESSAGE THREAD — lock-free, noexcept.
 */
const Grapheme* Grid::graphemePtr (const Buffer& buffer, int visibleRow, int col) const noexcept
{
    return buffer.graphemes.get() + physicalRow (buffer, visibleRow) * getCols() + col;
}

// =============================================================================

/**
 * @brief Append a single cell's text contribution to a row string.
 *
 * Skips wide-continuation cells entirely.  Appends a space for empty cells
 * (`codepoint == 0`).  For all other cells, appends the base codepoint
 * followed by any extra codepoints from the grapheme sidecar.
 *
 * @param cell      Cell to convert.
 * @param grapheme  Grapheme sidecar entry co-indexed with the cell (may be nullptr).
 * @param rowText   String to append to.
 */
static void appendCellText (const Cell& cell, const Grapheme* grapheme, juce::String& rowText)
{
    if (not cell.isWideContinuation())
    {
        if (cell.codepoint == 0)
        {
            rowText += " ";
        }
        else
        {
            rowText += juce::String::charToString (static_cast<juce::juce_wchar> (cell.codepoint));

            if (cell.hasGrapheme() and grapheme != nullptr)
            {
                const uint8_t safeCount { std::min (grapheme->count,
                    static_cast<uint8_t> (grapheme->extraCodepoints.size())) };

                for (uint8_t g { 0 }; g < safeCount; ++g)
                {
                    rowText += juce::String::charToString (
                        static_cast<juce::juce_wchar> (grapheme->extraCodepoints.at (g)));
                }
            }
        }
    }
}

// =============================================================================

/**
 * @brief Extracts a rectangular region of text as a UTF-32 string.
 *
 * Normalises the selection so that `start` is always before `end`, clamps
 * both endpoints to the visible area, then iterates rows collecting
 * codepoints:
 *
 * - Empty cells (`codepoint == 0`) are emitted as spaces.
 * - Wide-continuation cells (`LAYOUT_WIDE_CONT`) are skipped.
 * - Grapheme clusters append their extra codepoints after the base.
 * - Trailing whitespace is trimmed from each row.
 * - Rows are separated by `'\n'` except the last.
 *
 * @param start        Top-left corner of the selection (x = col, y = row).
 * @param end          Bottom-right corner of the selection (inclusive).
 * @param scrollOffset Number of rows the viewport is scrolled back (0 = live view).
 * @return A `juce::String` containing the selected text.
 * @note MESSAGE THREAD — caller must hold `resizeLock`.
 */
juce::String Grid::extractText (juce::Point<int> start, juce::Point<int> end, int scrollOffset) const
{
    int startRow { start.y };
    int startCol { start.x };
    int endRow { end.y };
    int endCol { end.x };

    if (startRow > endRow or (startRow == endRow and startCol > endCol))
    {
        std::swap (startRow, endRow);
        std::swap (startCol, endCol);
    }

    startRow = juce::jlimit (0, getVisibleRows() - 1, startRow);
    endRow = juce::jlimit (0, getVisibleRows() - 1, endRow);
    startCol = juce::jlimit (0, getCols() - 1, startCol);
    endCol = juce::jlimit (0, getCols() - 1, endCol);

    juce::String result;

    for (int row { startRow }; row <= endRow; ++row)
    {
        const Cell* cells { scrollbackRow (row, scrollOffset) };

        if (cells != nullptr)
        {
            int firstCol { (row == startRow) ? startCol : 0 };
            const int lastCol { (row == endRow) ? endCol : getCols() - 1 };

            if (firstCol > 0 and (*(cells + firstCol)).isWideContinuation())
                --firstCol;

            juce::String rowText;

            for (int col { firstCol }; col <= lastCol; ++col)
            {
                const Cell& cell { *(cells + col) };
                const Grapheme* gRow { cell.hasGrapheme() ? scrollbackGraphemeRow (row, scrollOffset) : nullptr };
                const Grapheme* grapheme { gRow != nullptr ? gRow + col : nullptr };
                appendCellText (cell, grapheme, rowText);
            }

            rowText = rowText.trimEnd();

            if (row < endRow)
                rowText += "\n";

            result += rowText;
        }
    }

    return result;
}

/**
 * @brief Extracts a box (rectangle) selection of text as a UTF-32 string.
 *
 * Applies the same column range `[topLeft.x, bottomRight.x]` to every row in
 * `[topLeft.y, bottomRight.y]`.  This produces a strict rectangular region —
 * the column range does not change between rows, unlike `extractText()` which
 * uses row-wrapped semantics.
 *
 * Empty cells are emitted as spaces; wide-continuation cells are skipped.
 * Trailing whitespace is trimmed from each row.  Rows are separated by `'\n'`
 * except the last.
 *
 * @param topLeft      Top-left corner of the rectangle (x = col, y = row).
 *                     Must already be normalised (min col/row of the selection).
 * @param bottomRight  Bottom-right corner of the rectangle (inclusive).
 *                     Must already be normalised (max col/row of the selection).
 * @param scrollOffset Number of rows the viewport is scrolled back (0 = live view).
 * @return A `juce::String` containing the selected text.
 * @note MESSAGE THREAD — caller must hold `resizeLock`.
 */
juce::String Grid::extractBoxText (juce::Point<int> topLeft, juce::Point<int> bottomRight, int scrollOffset) const
{
    const int startRow { juce::jlimit (0, getVisibleRows() - 1, topLeft.y) };
    const int endRow   { juce::jlimit (0, getVisibleRows() - 1, bottomRight.y) };
    const int startCol { juce::jlimit (0, getCols() - 1, topLeft.x) };
    const int endCol   { juce::jlimit (0, getCols() - 1, bottomRight.x) };

    juce::String result;

    for (int row { startRow }; row <= endRow; ++row)
    {
        const Cell* cells { scrollbackRow (row, scrollOffset) };

        if (cells != nullptr)
        {
            juce::String rowText;

            for (int col { startCol }; col <= endCol; ++col)
            {
                const Cell& cell { *(cells + col) };
                const Grapheme* gRow { cell.hasGrapheme() ? scrollbackGraphemeRow (row, scrollOffset) : nullptr };
                const Grapheme* grapheme { gRow != nullptr ? gRow + col : nullptr };
                appendCellText (cell, grapheme, rowText);
            }

            rowText = rowText.trimEnd();

            if (row < endRow)
                rowText += "\n";

            result += rowText;
        }
    }

    return result;
}

// ============================================================================
// Serialization
// ============================================================================

/**
 * @brief Serializes both screen buffers into destData.
 *
 * Acquires resizeLock and writes for each buffer (normal, alternate):
 *   - Scalar ring metadata (head, scrollbackUsed, totalRows, allocatedCols,
 *     allocatedVisibleRows) as int32_t.
 *   - Flat cell array: totalRows * allocatedCols * sizeof(Cell) bytes.
 *   - Flat grapheme array: totalRows * allocatedCols * sizeof(Grapheme) bytes.
 *   - RowState array: totalRows * sizeof(RowState) bytes.
 *
 * @note MESSAGE THREAD.
 */
void Grid::getStateInformation (juce::MemoryBlock& destData) const
{
    const juce::ScopedLock lock (resizeLock);

    for (int screenIndex { 0 }; screenIndex < 2; ++screenIndex)
    {
        const Buffer& buffer { buffers.at (static_cast<size_t> (screenIndex)) };

        const int32_t head            { static_cast<int32_t> (buffer.head) };
        const int32_t scrollbackUsed  { static_cast<int32_t> (buffer.scrollbackUsed) };
        const int32_t totalRows       { static_cast<int32_t> (buffer.totalRows) };
        const int32_t allocatedCols   { static_cast<int32_t> (buffer.allocatedCols) };
        const int32_t allocatedVisible { static_cast<int32_t> (buffer.allocatedVisibleRows) };

        destData.append (&head,             sizeof (int32_t));
        destData.append (&scrollbackUsed,   sizeof (int32_t));
        destData.append (&totalRows,        sizeof (int32_t));
        destData.append (&allocatedCols,    sizeof (int32_t));
        destData.append (&allocatedVisible, sizeof (int32_t));

        const size_t cellCount { static_cast<size_t> (totalRows) * static_cast<size_t> (allocatedCols) };
        destData.append (buffer.cells.getData(),     cellCount * sizeof (Cell));
        destData.append (buffer.graphemes.getData(), cellCount * sizeof (Grapheme));
        destData.append (buffer.rowStates.getData(), static_cast<size_t> (totalRows) * sizeof (RowState));
    }
}

/**
 * @brief Restores both screen buffers from a snapshot produced by getStateInformation.
 *
 * Acquires resizeLock, reads scalars, allocates HeapBlocks to match, and
 * memcpys bulk data. The rowSeqnos block is zero-initialised (not serialized).
 *
 * @note MESSAGE THREAD.
 */
void Grid::setStateInformation (const void* data, int size)
{
    const juce::ScopedLock lock (resizeLock);

    const char* cursor { static_cast<const char*> (data) };
    const char* const end { cursor + size };

    for (int screenIndex { 0 }; screenIndex < 2; ++screenIndex)
    {
        Buffer& buffer { buffers.at (static_cast<size_t> (screenIndex)) };

        const int32_t scalarsBytes { 5 * static_cast<int32_t> (sizeof (int32_t)) };

        if ((cursor + scalarsBytes) > end)
            break;

        int32_t head            { 0 };
        int32_t scrollbackUsed  { 0 };
        int32_t totalRows       { 0 };
        int32_t allocatedCols   { 0 };
        int32_t allocatedVisible { 0 };

        std::memcpy (&head,             cursor,                         sizeof (int32_t)); cursor += sizeof (int32_t);
        std::memcpy (&scrollbackUsed,   cursor,                         sizeof (int32_t)); cursor += sizeof (int32_t);
        std::memcpy (&totalRows,        cursor,                         sizeof (int32_t)); cursor += sizeof (int32_t);
        std::memcpy (&allocatedCols,    cursor,                         sizeof (int32_t)); cursor += sizeof (int32_t);
        std::memcpy (&allocatedVisible, cursor,                         sizeof (int32_t)); cursor += sizeof (int32_t);

        const size_t cellCount { static_cast<size_t> (totalRows) * static_cast<size_t> (allocatedCols) };
        const size_t cellBytes      { cellCount * sizeof (Cell) };
        const size_t graphemeBytes  { cellCount * sizeof (Grapheme) };
        const size_t rowStateBytes  { static_cast<size_t> (totalRows) * sizeof (RowState) };

        if ((cursor + static_cast<ptrdiff_t> (cellBytes + graphemeBytes + rowStateBytes)) > end)
            break;

        buffer.cells.allocate     (cellCount,                              false);
        buffer.graphemes.allocate (cellCount,                              true);
        buffer.rowStates.allocate (static_cast<size_t> (totalRows),       true);
        buffer.rowSeqnos.allocate (static_cast<size_t> (totalRows),       true);

        std::memcpy (buffer.cells.getData(),     cursor, cellBytes);     cursor += static_cast<ptrdiff_t> (cellBytes);
        std::memcpy (buffer.graphemes.getData(), cursor, graphemeBytes); cursor += static_cast<ptrdiff_t> (graphemeBytes);
        std::memcpy (buffer.rowStates.getData(), cursor, rowStateBytes); cursor += static_cast<ptrdiff_t> (rowStateBytes);

        buffer.head               = static_cast<int> (head);
        buffer.scrollbackUsed     = static_cast<int> (scrollbackUsed);
        buffer.totalRows          = static_cast<int> (totalRows);
        buffer.rowMask            = static_cast<int> (totalRows) - 1;
        buffer.allocatedCols      = static_cast<int> (allocatedCols);
        buffer.allocatedVisibleRows = static_cast<int> (allocatedVisible);
    }

    markAllDirty();
}

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Terminal
