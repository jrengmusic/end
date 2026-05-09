/**
 * @file Grid.cpp
 * @brief Implementation of the AudioBuffer-pattern terminal cell grid.
 *
 * @see Grid.h for design notes and API documentation.
 */

#include "Grid.h"
#include <cstring>

namespace Terminal
{ /*____________________________________________________________________________*/

// =============================================================================
// Buffer helpers
// =============================================================================

jam::Cell* Grid::Buffer::rowPtr (int logicalRow) noexcept
{
    const int physRow { (head + logicalRow) % capacity };
    return cells + physRow * alignedCols;
}

const jam::Cell* Grid::Buffer::rowPtr (int logicalRow) const noexcept
{
    const int physRow { (head + logicalRow) % capacity };
    return cells + physRow * alignedCols;
}

void Grid::Buffer::markDirty (int logicalRow) noexcept
{
    if (logicalRow < 64)
        dirtyRows.fetch_or (uint64_t (1) << logicalRow, std::memory_order_release);
}

void Grid::Buffer::markAllDirty() noexcept
{
    const uint64_t mask { (numRows < 64) ? ((uint64_t (1) << numRows) - 1) : ~uint64_t (0) };
    dirtyRows.fetch_or (mask, std::memory_order_release);
}

// =============================================================================
// Private helpers
// =============================================================================

Grid::Buffer& Grid::activeBuffer() noexcept
{
    return buffers.at (static_cast<size_t> (activeIndex.load (std::memory_order_relaxed)));
}

const Grid::Buffer& Grid::activeBuffer() const noexcept
{
    return buffers.at (static_cast<size_t> (activeIndex.load (std::memory_order_relaxed)));
}

int Grid::alignCols (int cols) noexcept
{
    return (cols + (simdCellAlignment - 1)) & ~(simdCellAlignment - 1);
}

void Grid::allocateBuffer (Buffer& buf, int numRows, int numCols, int scrollMargin,
                           bool keepContent, bool clearExtra, bool avoidRealloc) noexcept
{
    const int aligned { alignCols (numCols) };
    const int totalCapacity { numRows + scrollMargin };
    const size_t cellCount { static_cast<size_t> (totalCapacity) * static_cast<size_t> (aligned) };
    const size_t byteCount { cellCount * sizeof (jam::Cell) };

    const bool needsRealloc { not avoidRealloc
                              or buf.allocatedCapacity < totalCapacity
                              or buf.allocatedCols < aligned };

    if (needsRealloc)
    {
        juce::HeapBlock<char, true> newAlloc;
        newAlloc.allocate (static_cast<size_t> (byteCount), true);
        auto* newCells { reinterpret_cast<jam::Cell*> (newAlloc.getData()) };

        if (keepContent and buf.cells != nullptr)
        {
            const int copyRows { juce::jmin (buf.numRows, numRows) };
            const int copyCols { juce::jmin (buf.numCols, numCols) };

            for (int r { 0 }; r < copyRows; ++r)
            {
                const jam::Cell* src { buf.rowPtr (r) };
                jam::Cell* dst { newCells + r * aligned };
                std::memcpy (dst, src, static_cast<size_t> (copyCols) * sizeof (jam::Cell));
            }
        }

        buf.allocation = std::move (newAlloc);
        buf.cells = reinterpret_cast<jam::Cell*> (buf.allocation.getData());
        buf.allocatedCapacity = totalCapacity;
        buf.allocatedCols = aligned;
        buf.head = 0;
    }
    else
    {
        if (not keepContent)
            std::memset (buf.cells, 0, byteCount);
    }

    if (clearExtra and buf.cells != nullptr)
    {
        // Clear any newly exposed area beyond old dimensions
        const int oldRows { buf.numRows };
        const int oldCols { buf.numCols };

        if (numRows > oldRows)
        {
            for (int r { oldRows }; r < numRows; ++r)
            {
                jam::Cell* row { buf.cells + r * aligned };
                std::memset (row, 0, static_cast<size_t> (numCols) * sizeof (jam::Cell));
            }
        }

        if (numCols > oldCols)
        {
            const int rowsToClear { juce::jmin (oldRows, numRows) };

            for (int r { 0 }; r < rowsToClear; ++r)
            {
                jam::Cell* row { buf.cells + r * aligned + oldCols };
                std::memset (row, 0, static_cast<size_t> (numCols - oldCols) * sizeof (jam::Cell));
            }
        }
    }

    buf.numRows = numRows;
    buf.numCols = numCols;
    buf.alignedCols = aligned;
    buf.capacity = totalCapacity;
    buf.scrollMargin = scrollMargin;
    buf.scrolledCount.store (0, std::memory_order_relaxed);
    buf.dirtyRows.store (0, std::memory_order_relaxed);
    buf.isClear = not keepContent;
}

// =============================================================================
// Size
// =============================================================================

void Grid::setSize (int numRows, int numCols,
                    bool keepExistingContent,
                    bool clearExtraSpace,
                    bool avoidReallocating) noexcept
{
    jassert (numRows > 0 and numCols > 0);

    allocateBuffer (buffers.at (0), numRows, numCols, defaultScrollMargin,
                    keepExistingContent, clearExtraSpace, avoidReallocating);
    allocateBuffer (buffers.at (1), numRows, numCols, 0,
                    keepExistingContent, clearExtraSpace, avoidReallocating);
}

int Grid::getNumRows() const noexcept { return activeBuffer().numRows; }
int Grid::getNumCols() const noexcept { return activeBuffer().numCols; }

// =============================================================================
// Pointer access
// =============================================================================

const jam::Cell* Grid::getReadPointer (int row) const noexcept
{
    const auto& buf { activeBuffer() };
    jassert (row >= 0 and row < buf.numRows);
    return buf.rowPtr (row);
}

const jam::Cell* Grid::getReadPointer (int row, int col) const noexcept
{
    const auto& buf { activeBuffer() };
    jassert (row >= 0 and row < buf.numRows);
    jassert (col >= 0 and col < buf.numCols);
    return buf.rowPtr (row) + col;
}

jam::Cell* Grid::getWritePointer (int row) noexcept
{
    auto& buf { activeBuffer() };
    jassert (row >= 0 and row < buf.numRows);
    buf.markDirty (row);
    buf.isClear = false;
    return buf.rowPtr (row);
}

jam::Cell* Grid::getWritePointer (int row, int col) noexcept
{
    auto& buf { activeBuffer() };
    jassert (row >= 0 and row < buf.numRows);
    jassert (col >= 0 and col < buf.numCols);
    buf.markDirty (row);
    buf.isClear = false;
    return buf.rowPtr (row) + col;
}

// =============================================================================
// Element access
// =============================================================================

const jam::Cell& Grid::getCell (int row, int col) const noexcept
{
    return *getReadPointer (row, col);
}

void Grid::setCell (int row, int col, const jam::Cell& cell) noexcept
{
    *getWritePointer (row, col) = cell;
}

// =============================================================================
// Clear
// =============================================================================

void Grid::clear() noexcept
{
    auto& buf { activeBuffer() };

    if (buf.cells != nullptr)
    {
        const size_t byteCount { static_cast<size_t> (buf.capacity)
                                 * static_cast<size_t> (buf.alignedCols)
                                 * sizeof (jam::Cell) };
        std::memset (buf.cells, 0, byteCount);
    }

    buf.isClear = true;
    buf.dirtyRows.store (0, std::memory_order_relaxed);
}

void Grid::clear (int row) noexcept
{
    auto& buf { activeBuffer() };
    jassert (row >= 0 and row < buf.numRows);
    std::memset (buf.rowPtr (row), 0, static_cast<size_t> (buf.numCols) * sizeof (jam::Cell));
    buf.markDirty (row);
    buf.isClear = false;
}

void Grid::clear (int row, int startCol, int numCols) noexcept
{
    auto& buf { activeBuffer() };
    jassert (row >= 0 and row < buf.numRows);
    jassert (startCol >= 0);
    const int clampedCount { juce::jmin (numCols, buf.numCols - startCol) };

    if (clampedCount > 0)
    {
        std::memset (buf.rowPtr (row) + startCol, 0,
                     static_cast<size_t> (clampedCount) * sizeof (jam::Cell));
        buf.markDirty (row);
        buf.isClear = false;
    }
}

bool Grid::hasBeenCleared() const noexcept
{
    return activeBuffer().isClear;
}

// =============================================================================
// Dual screen
// =============================================================================

void Grid::setScreen (bool alternate) noexcept
{
    activeIndex.store (alternate ? 1 : 0, std::memory_order_relaxed);
}

bool Grid::isAlternateScreen() const noexcept
{
    return activeIndex.load (std::memory_order_relaxed) != 0;
}

// =============================================================================
// Dirty tracking
// =============================================================================

uint64_t Grid::consumeDirtyRows() noexcept
{
    return activeBuffer().dirtyRows.exchange (0, std::memory_order_acquire);
}

// =============================================================================
// Scroll
// =============================================================================

void Grid::scrollUp (int scrollTop, int scrollBottom, int count) noexcept
{
    auto& buf { activeBuffer() };
    const int clampedCount { juce::jmin (count, scrollBottom - scrollTop + 1) };

    if (clampedCount > 0)
    {
        const bool isFullScreen { scrollTop == 0 and scrollBottom == buf.numRows - 1 };
        const bool isNormal { activeIndex.load (std::memory_order_relaxed) == 0 };

        if (isFullScreen and isNormal)
        {
            // Ring-buffer scroll — advance head, capture scroll-off rows
            for (int n { 0 }; n < clampedCount; ++n)
            {
                buf.head = (buf.head + 1) % buf.capacity;

                const int prevScrolled { buf.scrolledCount.load (std::memory_order_acquire) };
                buf.scrolledCount.store (juce::jmin (prevScrolled + 1, buf.scrollMargin),
                                         std::memory_order_release);

                // Clear new bottom row
                std::memset (buf.rowPtr (buf.numRows - 1), 0,
                             static_cast<size_t> (buf.numCols) * sizeof (jam::Cell));
            }
        }
        else
        {
            // Region scroll or alternate screen — memmove
            for (int n { 0 }; n < clampedCount; ++n)
            {
                // Shift rows up within the region
                for (int r { scrollTop }; r < scrollBottom; ++r)
                    std::memcpy (buf.rowPtr (r), buf.rowPtr (r + 1),
                                 static_cast<size_t> (buf.numCols) * sizeof (jam::Cell));

                // Clear bottom row of the region
                std::memset (buf.rowPtr (scrollBottom), 0,
                             static_cast<size_t> (buf.numCols) * sizeof (jam::Cell));
            }
        }

        buf.markAllDirty();
    }
}

void Grid::scrollDown (int scrollTop, int scrollBottom, int count) noexcept
{
    auto& buf { activeBuffer() };
    const int clampedCount { juce::jmin (count, scrollBottom - scrollTop + 1) };

    if (clampedCount > 0)
    {
        for (int n { 0 }; n < clampedCount; ++n)
        {
            // Shift rows down within the region
            for (int r { scrollBottom }; r > scrollTop; --r)
                std::memcpy (buf.rowPtr (r), buf.rowPtr (r - 1),
                             static_cast<size_t> (buf.numCols) * sizeof (jam::Cell));

            // Clear top row of the region
            std::memset (buf.rowPtr (scrollTop), 0,
                         static_cast<size_t> (buf.numCols) * sizeof (jam::Cell));
        }

        buf.markAllDirty();
    }
}

// =============================================================================
// Scroll-off capture
// =============================================================================

int Grid::getNumScrolledRows() const noexcept
{
    return buffers.at (0).scrolledCount.load (std::memory_order_acquire);
}

const jam::Cell* Grid::getScrolledReadPointer (int index) const noexcept
{
    const auto& buf { buffers.at (0) };  // Scroll-off only on normal screen
    const int scrolled { buf.scrolledCount.load (std::memory_order_acquire) };
    jassert (index >= 0 and index < scrolled);

    // Scroll-off rows are behind the current head in the ring
    // index 0 = oldest unconsumed, scrolled-1 = newest
    const int physRow { (buf.head - scrolled + index + buf.capacity) % buf.capacity };
    return buf.cells + physRow * buf.alignedCols;
}

void Grid::consumeScrolledRows (int count) noexcept
{
    auto& buf { buffers.at (0) };
    const int prev { buf.scrolledCount.load (std::memory_order_acquire) };
    buf.scrolledCount.store (juce::jmax (0, prev - count), std::memory_order_release);
}

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Terminal
