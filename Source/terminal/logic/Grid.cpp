/**
 * @file Grid.cpp
 * @brief Implementation of Grid — pure content storage for the terminal emulator.
 *
 * @see Grid.h
 */

#include "Grid.h"

namespace Terminal
{ /*____________________________________________________________________________*/

// =============================================================================
// Helpers (file scope, named per NAMES.md — no anonymous namespace)
// =============================================================================

static int nextPowerOfTwo (int n) noexcept
{
    jassert (n > 0);
    int p { 1 };
    while (p < n)
        p <<= 1;
    return p;
}

// =============================================================================
// TerminalLine
// =============================================================================

void Grid::TerminalLine::ensureCapacity (int needed) noexcept
{
    if (needed > capacity)
    {
        const int newCap { nextPowerOfTwo (needed) };

        cells.realloc (newCap);
        graphemes.realloc (newCap);
        linkIds.realloc (newCap);

        const int added { newCap - capacity };
        std::memset (cells.get() + capacity,    0, static_cast<size_t> (added) * sizeof (Cell));
        std::memset (graphemes.get() + capacity, 0, static_cast<size_t> (added) * sizeof (Grapheme));
        std::memset (linkIds.get() + capacity,   0, static_cast<size_t> (added) * sizeof (uint16_t));

        capacity = newCap;
    }
}

void Grid::TerminalLine::reset() noexcept
{
    if (length > 0)
    {
        std::memset (cells.get(),    0, static_cast<size_t> (length) * sizeof (Cell));
        std::memset (graphemes.get(), 0, static_cast<size_t> (length) * sizeof (Grapheme));
        std::memset (linkIds.get(),   0, static_cast<size_t> (length) * sizeof (uint16_t));
        length = 0;
    }
}

// =============================================================================
// Grid construction
// =============================================================================

Grid::Grid (State& initState)
    : state (initState)
    , scrollbackCapacity { lua::Engine::getContext()->nexus.terminal.scrollbackLines }
{
    cols         = state.getCols();
    numVisibleRows = state.getVisibleRows();

    initLines (buffers.at (normal),    numVisibleRows + scrollbackCapacity);
    initLines (buffers.at (alternate), numVisibleRows);
}

void Grid::initLines (Lines<TerminalLine>& lines, int lineCapacity)
{
    const int ringCap { nextPowerOfTwo (juce::jmax (1, lineCapacity)) };

    lines.lines.allocate (ringCap, true);

    for (int i { 0 }; i < ringCap; ++i)
        new (lines.lines.get() + i) TerminalLine {};

    lines.capacity = ringCap;
    lines.mask     = ringCap - 1;
    lines.head     = 0;
    lines.count    = 1;

    lines.lines[0].ensureCapacity (cols);
}

// =============================================================================
// Grid — lifecycle
// =============================================================================

juce::CriticalSection& Grid::getResizeLock() noexcept
{
    return resizeLock;
}

juce::CriticalSection& Grid::getResizeLock() const noexcept
{
    return resizeLock;
}

void Grid::resize (int newCols, int newVisibleRows)
{
    const juce::ScopedLock lock { resizeLock };

    cols           = newCols;
    numVisibleRows = newVisibleRows;

    for (auto& buf : buffers)
    {
        for (int i { 0 }; i < buf.count; ++i)
            buf.at (i).ensureCapacity (newCols);
    }

    markAllDirty();
}

void Grid::clearBuffer()
{
    auto& lines { bufferForScreen() };

    for (int i { 0 }; i < lines.count; ++i)
        lines.at (i).reset();

    lines.head  = 0;
    lines.count = 1;

    lines.lines[0].ensureCapacity (cols);

    markAllDirty();
}

void Grid::clearScrollback() noexcept
{
    auto& lines { bufferForScreen() };

    if (lines.count > numVisibleRows)
    {
        // Compact: move the visible tail to physical slots 0 … numVisibleRows-1,
        // discarding all scrollback lines above them.
        for (int i { 0 }; i < numVisibleRows; ++i)
        {
            const int srcPhysical { (lines.head - numVisibleRows + 1 + i + lines.capacity) & lines.mask };
            const int dstPhysical { i };

            if (srcPhysical != dstPhysical)
            {
                // Swap cell storage — avoids deep copy by swapping HeapBlocks.
                auto& src { lines.lines[srcPhysical] };
                auto& dst { lines.lines[dstPhysical] };

                std::swap (src.cells,     dst.cells);
                std::swap (src.graphemes, dst.graphemes);
                std::swap (src.linkIds,   dst.linkIds);

                const int tmpLen  { src.length };
                const int tmpCap  { src.capacity };
                src.length   = dst.length;
                src.capacity = dst.capacity;
                dst.length   = tmpLen;
                dst.capacity = tmpCap;
            }
        }

        lines.head  = numVisibleRows - 1;
        lines.count = numVisibleRows;

        markAllDirty();
    }
}

// =============================================================================
// Grid — geometry
// =============================================================================

int Grid::getScrollbackUsed() const noexcept
{
    const auto& lines { bufferForScreen() };
    int totalVisualRows { 0 };

    for (int i { 0 }; i < lines.count; ++i)
    {
        const int len { lines.at (i).length };
        totalVisualRows += juce::jmax (1, (len + cols - 1) / cols);
    }

    return juce::jmax (0, totalVisualRows - numVisibleRows);
}

// =============================================================================
// Grid — dirty tracking
// =============================================================================

void Grid::markRowDirty (int row) noexcept
{
    dirtyRows[row >> 6].fetch_or (uint64_t { 1 } << (row & 63), std::memory_order_relaxed);
    seqno.fetch_add (1, std::memory_order_relaxed);
    state.setSnapshotDirty();
}

void Grid::markAllDirty() noexcept
{
    for (auto& word : dirtyRows)
        word.store (~uint64_t { 0 }, std::memory_order_relaxed);

    scrollDelta.store (0, std::memory_order_relaxed);
    seqno.fetch_add (1, std::memory_order_relaxed);
    state.setSnapshotDirty();
}

void Grid::batchMarkDirty (const uint64_t localDirty[4]) noexcept
{
    for (int w { 0 }; w < 4; ++w)
    {
        if (localDirty[w] != 0)
            dirtyRows[w].fetch_or (localDirty[w], std::memory_order_relaxed);
    }

    seqno.fetch_add (1, std::memory_order_relaxed);
    state.setSnapshotDirty();
}

void Grid::consumeDirtyRows (uint64_t out[4]) noexcept
{
    for (int w { 0 }; w < 4; ++w)
        out[w] = dirtyRows[w].exchange (0, std::memory_order_acq_rel);
}

int Grid::consumeScrollDelta() noexcept
{
    return scrollDelta.exchange (0, std::memory_order_acq_rel);
}

// =============================================================================
// Grid — content read
// =============================================================================

const Lines<Grid::TerminalLine>& Grid::getLines() const noexcept
{
    return bufferForScreen();
}

int Grid::getLineCount() const noexcept
{
    return bufferForScreen().count;
}

const Grid::TerminalLine& Grid::getLine (int logicalIndex) const noexcept
{
    return bufferForScreen().at (logicalIndex);
}

// =============================================================================
// Grid — text extraction
// =============================================================================

juce::String Grid::extractText (juce::Point<int> /*start*/, juce::Point<int> /*end*/) const
{
    // TODO Sprint N: implement once Parser migration supplies logical-line mapping.
    return {};
}

juce::String Grid::extractBoxText (juce::Point<int> /*topLeft*/, juce::Point<int> /*bottomRight*/) const
{
    // TODO Sprint N: implement once Parser migration supplies logical-line mapping.
    return {};
}

// =============================================================================
// Grid — serialization
// =============================================================================

void Grid::getStateInformation (juce::MemoryBlock& /*destData*/) const
{
    // TODO Sprint N: implement.
}

void Grid::setStateInformation (const void* /*data*/, int /*size*/)
{
    // TODO Sprint N: implement.
}

// =============================================================================
// Grid — private buffer routing
// =============================================================================

Lines<Grid::TerminalLine>& Grid::bufferForScreen() noexcept
{
    return buffers.at (static_cast<size_t> (state.getRawValue<ActiveScreen> (ID::activeScreen)));
}

const Lines<Grid::TerminalLine>& Grid::bufferForScreen() const noexcept
{
    return buffers.at (static_cast<size_t> (state.getRawValue<ActiveScreen> (ID::activeScreen)));
}


// =============================================================================
// Grid::Writer — line creation
// =============================================================================

void Grid::Writer::lineFeed (int /*cursorCol*/)
{
    auto& lines { grid.bufferForScreen() };
    lines.advance();
    lines.lines[lines.head].ensureCapacity (grid.cols);
    grid.scrollDelta.fetch_add (1, std::memory_order_relaxed);
    grid.state.setSnapshotDirty();
}

void Grid::Writer::wrapToNextRow()
{
    auto& lines { grid.bufferForScreen() };
    auto& currentLine { lines.at (lines.count - 1) };
    currentLine.ensureCapacity (currentLine.length + grid.cols);
}

// =============================================================================
// Grid::Writer — direct pointer access
// =============================================================================

Cell* Grid::Writer::directLinePtr (int lineIndex, int cellOffset) noexcept
{
    auto& line { grid.bufferForScreen().at (lineIndex) };
    line.ensureCapacity (cellOffset + grid.cols);
    return line.cells.get() + cellOffset;
}

Grapheme* Grid::Writer::directGraphemePtr (int lineIndex, int cellOffset) noexcept
{
    auto& line { grid.bufferForScreen().at (lineIndex) };
    line.ensureCapacity (cellOffset + grid.cols);
    return line.graphemes.get() + cellOffset;
}

uint16_t* Grid::Writer::directLinkIdPtr (int lineIndex, int cellOffset) noexcept
{
    auto& line { grid.bufferForScreen().at (lineIndex) };
    line.ensureCapacity (cellOffset + grid.cols);
    return line.linkIds.get() + cellOffset;
}

// =============================================================================
// Grid::Writer — content metadata
// =============================================================================

int Grid::Writer::getTotalLines() const noexcept
{
    return grid.bufferForScreen().count;
}

int Grid::Writer::getLineLength (int lineIndex) const noexcept
{
    return grid.bufferForScreen().at (lineIndex).length;
}

void Grid::Writer::updateLineLength (int lineIndex, int minLength) noexcept
{
    auto& line { grid.bufferForScreen().at (lineIndex) };
    line.length = juce::jmax (line.length, minLength);
}

// =============================================================================
// Grid::Writer — erase (line-level coordinates)
// =============================================================================

void Grid::Writer::eraseInLine (int lineIndex, int startOffset, int endOffset, const Cell& fill) noexcept
{
    auto& line { grid.bufferForScreen().at (lineIndex) };
    line.ensureCapacity (endOffset + 1);

    for (int i { startOffset }; i <= endOffset; ++i)
        line.cells[i] = fill;

    std::memset (line.graphemes.get() + startOffset, 0,
                 static_cast<size_t> (endOffset - startOffset + 1) * sizeof (Grapheme));
    std::memset (line.linkIds.get() + startOffset, 0,
                 static_cast<size_t> (endOffset - startOffset + 1) * sizeof (uint16_t));

    grid.seqno.fetch_add (1, std::memory_order_relaxed);
    grid.state.setSnapshotDirty();
}

// =============================================================================
// Grid::Writer — buffer management
// =============================================================================

void Grid::Writer::clearScrollback() noexcept
{
    grid.clearScrollback();
}

void Grid::Writer::clearBuffer()
{
    grid.clearBuffer();
}

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Terminal
