/**
 * @file GridScroll.cpp
 * @brief Grid member functions for scroll region operations.
 *
 * Grid is pure storage — the `fill` cell provided by the caller determines
 * what newly exposed rows look like (BCE).
 *
 * @see Grid.h   — class declaration, ring-buffer layout.
 * @see Cell     — 16-byte terminal cell type stored in the ring buffer.
 */

#include "Grid.h"
#include "../data/State.h"

namespace Terminal
{ /*____________________________________________________________________________*/

void Grid::scrollUp (int count, const Cell& fill) noexcept
{
    const auto scr { state.getRawValue<ActiveScreen> (Terminal::ID::activeScreen) };
    Buffer& buffer { bufferForScreen() };

    for (int i { 0 }; i < count; ++i)
    {
        buffer.head = (buffer.head + 1) & buffer.rowMask;

        Cell* row { rowPtr (buffer, getVisibleRows() - 1) };
        std::fill (row, row + getCols(), fill);

        if (scr == normal and buffer.scrollbackUsed < scrollbackCapacity)
        {
            ++buffer.scrollbackUsed;
        }
    }

    scrollDelta.fetch_add (count, std::memory_order_relaxed);

    for (int r { getVisibleRows() - count }; r < getVisibleRows(); ++r)
    {
        if (r >= 0)
        {
            markRowDirty (r);
        }
    }
}

void Grid::scrollDown (int count, const Cell& fill) noexcept
{
    Buffer& buffer { bufferForScreen() };

    for (int i { 0 }; i < count; ++i)
    {
        buffer.head = (buffer.head - 1) & buffer.rowMask;
        clearRow (buffer, 0, fill);
    }

    markAllDirty();
}

void Grid::scrollRegionUp (int top, int bottom, int count, const Cell& fill) noexcept
{
    if (top == 0 and bottom == getVisibleRows() - 1)
    {
        scrollUp (count, fill);
    }
    else
    {
        scrollRegion (top, bottom, count, true, fill);
    }
}

void Grid::scrollRegionDown (int top, int bottom, int count, const Cell& fill) noexcept
{
    if (top == 0 and bottom == getVisibleRows() - 1)
    {
        scrollDown (count, fill);
    }
    else
    {
        scrollRegion (top, bottom, count, false, fill);
    }
}

void Grid::shiftRegionUp (Buffer& buffer, int top, int bottom, int ec, size_t rowBytes, const Cell& fill) noexcept
{
    for (int i { top }; i <= bottom - ec; ++i)
    {
        std::memcpy (rowPtr (buffer, i), rowPtr (buffer, i + ec), rowBytes);
        buffer.rowStates[physicalRow (buffer, i)] = buffer.rowStates[physicalRow (buffer, i + ec)];
    }
    for (int i { bottom - ec + 1 }; i <= bottom; ++i)
    {
        clearRow (buffer, i, fill);
    }
}

void Grid::shiftRegionDown (Buffer& buffer, int top, int bottom, int ec, size_t rowBytes, const Cell& fill) noexcept
{
    for (int i { bottom }; i >= top + ec; --i)
    {
        std::memcpy (rowPtr (buffer, i), rowPtr (buffer, i - ec), rowBytes);
        buffer.rowStates[physicalRow (buffer, i)] = buffer.rowStates[physicalRow (buffer, i - ec)];
    }
    for (int i { top }; i < top + ec; ++i)
    {
        clearRow (buffer, i, fill);
    }
}

void Grid::scrollRegion (int top, int bottom, int count, bool up, const Cell& fill) noexcept
{
    if (top >= 0 and bottom < getVisibleRows() and top < bottom and count > 0)
    {
        Buffer& buffer { bufferForScreen() };
        const int regionSize { bottom - top + 1 };
        const int ec { juce::jmin (count, regionSize) };
        const size_t rowBytes { static_cast<size_t> (getCols()) * sizeof (Cell) };

        if (up)
        {
            shiftRegionUp (buffer, top, bottom, ec, rowBytes, fill);
        }
        else
        {
            shiftRegionDown (buffer, top, bottom, ec, rowBytes, fill);
        }

        for (int r { top }; r <= bottom; ++r)
        {
            markRowDirty (r);
        }
    }
}

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Terminal
