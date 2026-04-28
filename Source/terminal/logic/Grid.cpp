/**
 * @file Grid.cpp
 * @brief Ring-buffer cell storage — constructor, buffer init, dirty tracking,
 *        and cell write/read operations.
 *
 * This translation unit implements the core Grid methods.  Row access,
 * scrollback, and text extraction are in GridAccess.cpp.  Serialization is in
 * GridSerialize.cpp.  Scroll, erase, and
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
    , scrollbackCapacity { lua::Engine::getContext()->nexus.terminal.scrollbackLines }
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
    buffer.linkIds.allocate (cellCount, true);

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
        rowPtr (bufferForScreen(), row)[col] = cellState;
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
        Cell* dst { rowPtr (bufferForScreen(), row) + startCol };
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

/**
 * @brief Returns a mutable pointer to the linkId entry at (visibleRow, col).
 *
 * @param buffer      Target buffer.
 * @param visibleRow  Zero-based visible row index.
 * @param col         Zero-based column index.
 * @return Pointer to `buffer.linkIds[physicalRow * cols + col]`.
 * @note READER THREAD — lock-free, noexcept.
 */
uint16_t* Grid::linkIdPtr (Buffer& buffer, int visibleRow, int col) noexcept
{
    return buffer.linkIds.get() + physicalRow (buffer, visibleRow) * getCols() + col;
}

/**
 * @brief Returns a const pointer to the linkId entry at (visibleRow, col).
 *
 * @param buffer      Target buffer.
 * @param visibleRow  Zero-based visible row index.
 * @param col         Zero-based column index.
 * @return Const pointer to `buffer.linkIds[physicalRow * cols + col]`.
 * @note MESSAGE THREAD — lock-free, noexcept.
 */
const uint16_t* Grid::linkIdPtr (const Buffer& buffer, int visibleRow, int col) const noexcept
{
    return buffer.linkIds.get() + physicalRow (buffer, visibleRow) * getCols() + col;
}

/**
 * @brief Writes the hyperlink ID sidecar for the cell at (row, col).
 *
 * Sets `Cell::LAYOUT_HYPERLINK` on the target cell and writes `linkId` into
 * the parallel linkIds block at the same physical offset.  Calls
 * `markRowDirty (row)`.
 *
 * @param row    Zero-based visible row index.
 * @param col    Zero-based column index.
 * @param linkId Hyperlink ID to store (non-zero).
 * @note READER THREAD — lock-free, noexcept.
 */
void Grid::activeWriteLinkId (int row, int col, uint16_t linkId) noexcept
{
    if (row >= 0 and row < getVisibleRows() and col >= 0 and col < getCols())
    {
        Buffer& buffer { bufferForScreen() };
        rowPtr (buffer, row)[col].layout |= Cell::LAYOUT_HYPERLINK;
        *linkIdPtr (buffer, row, col) = linkId;
        markRowDirty (row);
    }
}

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Terminal
