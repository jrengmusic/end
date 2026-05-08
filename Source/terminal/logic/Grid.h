/**
 * @file Grid.h
 * @brief Pure content storage for the terminal emulator.
 *
 * Grid stores what Parser writes.  No visual rows, no viewport, no scroll
 * management.  Each screen buffer is a `Terminal::Lines<TerminalLine>` ring.
 * A TerminalLine is a variable-length sequence of cells between hard line
 * breaks (LF).  Wrap extends a Line; LF creates a new Line.
 *
 * Parser maintains its own visual-row-to-Line mapping externally.
 * Screen computes its own read mapping per-frame.
 *
 * ## Thread model
 *
 * | Operation          | Thread         | Guard                  |
 * |--------------------|----------------|------------------------|
 * | Writer methods     | READER THREAD  | none (single writer)   |
 * | resize()           | MESSAGE THREAD | resizeLock (exclusive) |
 * | getLine/getLines   | MESSAGE THREAD | resizeLock (shared)    |
 * | Dirty / seqno      | both           | atomic                 |
 *
 * @see Terminal::Line   — base type: length + capacity.
 * @see Terminal::Lines  — ring buffer of Line-derived objects.
 */

#pragma once

#include <JuceHeader.h>

#include "../../lua/Engine.h"
#include "../data/State.h"
#include "../data/Cell.h"

namespace Terminal
{ /*____________________________________________________________________________*/

/*____________________________________________________________________________*/

/**
 * @struct Line
 * @brief Base type for a logical line — variable-length content between hard
 *        breaks.
 *
 * Tracks content length and allocation capacity.  Consumers inherit and add
 * their own content storage (cell arrays, attributed strings, etc.).
 * Consumer must implement `void ensureCapacity (int) noexcept` and
 * `void reset() noexcept` on the derived type.
 */
struct Line
{
    int length { 0 };
    int capacity { 0 };
};

/**
 * @struct Lines
 * @brief Ring buffer of Line-derived objects.  Content-agnostic.
 *
 * Manages ring arithmetic, advance/recycle, logical index access, and
 * scrollback tracking.  `LineType` must inherit from `Line` and provide
 * `void reset() noexcept`.
 *
 * `capacity` is always a power of two for bitwise AND indexing.
 * `head` is the physical index of the newest Line.
 * `count` is the number of active Lines in the ring.
 * `scrollbackVisualRows` tracks visual rows of content above the viewport.
 *
 * @tparam LineType  Concrete line type inheriting from `Line`.
 */
template <typename LineType>
struct Lines
{
    juce::HeapBlock<LineType> lines;
    int capacity { 0 };
    int mask { 0 };
    int head { 0 };
    int count { 0 };
    int scrollbackVisualRows { 0 };

    /** @brief Returns a mutable reference to the Line at logical ring index.
     *  @param index  Logical index (0 = oldest active, count-1 = newest). */
    LineType& at (int index) noexcept
    {
        jassert (index >= 0 and index < count);
        const int physical { (head - count + 1 + index + capacity) & mask };
        return lines[physical];
    }

    /** @brief Const overload. */
    const LineType& at (int index) const noexcept
    {
        jassert (index >= 0 and index < count);
        const int physical { (head - count + 1 + index + capacity) & mask };
        return lines[physical];
    }

    /**
     * @brief Advances the ring: recycles the oldest Line if full, returns
     *        new slot.
     *
     * Increments `head`.  If the ring is full, the oldest slot is recycled
     * via `LineType::reset()`.  Otherwise `count` increments.
     *
     * @return Mutable reference to the new (recycled or fresh) Line slot.
     */
    LineType& advance() noexcept
    {
        head = (head + 1) & mask;

        if (count < capacity)
            ++count;

        auto& line { lines[head] };
        line.reset();
        return line;
    }
};

class State;

class Grid
{
public:

    // =========================================================================
    // Public nested types
    // =========================================================================

    /**
     * @struct TerminalLine
     * @brief Terminal cell storage for a logical line.
     *
     * Inherits length/capacity tracking from `Terminal::Line`.
     * Adds parallel arrays for cells, graphemes, and hyperlink IDs.
     * One TerminalLine per LF-terminated content.  Wrap GROWS a Line
     * (increases length/capacity).  LF CREATES a new Line.
     */
    struct TerminalLine : Line
    {
        juce::HeapBlock<jam::Cell> cells;
        juce::HeapBlock<jam::Grapheme> graphemes;
        juce::HeapBlock<uint16_t> linkIds;

        void ensureCapacity (int needed) noexcept;
        void reset() noexcept;
    };

    /**
     * @struct Row
     * @brief View mapping entry — which Line segment appears at a visual row.
     *
     * Used by Parser (private mapping for writes) and Screen (per-frame
     * mapping for reads).  Grid itself does NOT store Row arrays.
     */
    struct Row
    {
        int lineIndex { 0 };
        int cellOffset { 0 };
    };

    // =========================================================================
    // Construction / lifecycle
    // =========================================================================

    explicit Grid (State& state);

    juce::CriticalSection& getResizeLock() noexcept;
    juce::CriticalSection& getResizeLock() const noexcept;

    void resize (int newCols, int newVisibleRows);
    void clearBuffer();
    void clearScrollback() noexcept;

    // =========================================================================
    // Geometry
    // =========================================================================

    int getCols() const noexcept { return cols; }
    int getVisibleRows() const noexcept { return numVisibleRows; }
    int getScrollbackUsed() const noexcept;

    // =========================================================================
    // Dirty tracking
    // =========================================================================

    void markRowDirty (int row) noexcept;
    void markAllDirty() noexcept;
    void batchMarkDirty (const uint64_t localDirty[4]) noexcept;
    void consumeDirtyRows (uint64_t out[4]) noexcept;
    int consumeScrollDelta() noexcept;
    uint64_t currentSeqno() const noexcept { return seqno.load (std::memory_order_relaxed); }

    // =========================================================================
    // Content read (MESSAGE THREAD — caller holds resizeLock)
    // =========================================================================

    /** @brief Returns const ref to the active screen's Lines ring. */
    const Lines<TerminalLine>& getLines() const noexcept;

    /** @brief Returns the number of active Lines in the ring. */
    int getLineCount() const noexcept;

    /** @brief Returns const ref to TerminalLine at logical ring index. */
    const TerminalLine& getLine (int logicalIndex) const noexcept;

    // =========================================================================
    // Text extraction (MESSAGE THREAD — caller holds resizeLock)
    // =========================================================================

    juce::String extractText (juce::Point<int> start, juce::Point<int> end) const;
    juce::String extractBoxText (juce::Point<int> topLeft, juce::Point<int> bottomRight) const;

    // =========================================================================
    // Serialization
    // =========================================================================

    void getStateInformation (juce::MemoryBlock& destData) const;
    void setStateInformation (const void* data, int size);

    // =========================================================================
    // Writer — Parser write facade
    // =========================================================================

    /**
     * @class Writer
     * @brief READER THREAD write facade for Parser access to the Grid.
     *
     * Parser holds a Writer by value.  Writer provides line-level access:
     * `directLinePtr(lineIndex, cellOffset)` returns a pointer into a
     * TerminalLine's cell array.  Parser maintains its own visual-row-to-Line
     * mapping and resolves through it before calling Writer methods.
     *
     * `lineFeed` creates a new Line.  `wrapToNextRow` extends the current Line.
     */
    class Writer
    {
    public:
        explicit Writer (Grid& g) noexcept : grid (g) {}

        // -- Line creation / viewport scroll --
        void lineFeed (int cursorCol);
        void wrapToNextRow();

        // -- Direct pointer access (hot path) --
        jam::Cell* directLinePtr (int lineIndex, int cellOffset) noexcept;
        jam::Grapheme* directGraphemePtr (int lineIndex, int cellOffset) noexcept;
        uint16_t* directLinkIdPtr (int lineIndex, int cellOffset) noexcept;

        // -- Content metadata --
        int getTotalLines() const noexcept;
        int getLineLength (int lineIndex) const noexcept;
        void updateLineLength (int lineIndex, int minLength) noexcept;

        // -- Dirty tracking --
        void markRowDirty (int row) noexcept                                   { grid.markRowDirty (row); }
        void batchMarkDirty (const uint64_t localDirty[4]) noexcept            { grid.batchMarkDirty (localDirty); }
        void markAllDirty() noexcept                                            { grid.markAllDirty(); }

        // -- Erase (line-level coordinates) --
        void eraseInLine (int lineIndex, int startOffset, int endOffset, const jam::Cell& fill = jam::Cell {}) noexcept;

        // -- Buffer management --
        void clearScrollback() noexcept;
        void clearBuffer();

    private:
        Grid& grid;
    };

private:

    // =========================================================================
    // Private members
    // =========================================================================

    mutable juce::CriticalSection resizeLock;
    State& state;
    int scrollbackCapacity { 0 };

    std::atomic<uint64_t> dirtyRows[4] { {~ uint64_t {0}}, {~ uint64_t {0}}, {~ uint64_t {0}}, {~ uint64_t {0}} };
    std::atomic<uint64_t> seqno { 0 };
    std::atomic<int> scrollDelta { 0 };

    std::array<Lines<TerminalLine>, 2> buffers;
    int cols { 0 };
    int numVisibleRows { 0 };

    // =========================================================================
    // Private methods
    // =========================================================================

    Lines<TerminalLine>& bufferForScreen() noexcept;
    const Lines<TerminalLine>& bufferForScreen() const noexcept;
    void initLines (Lines<TerminalLine>& lines, int lineCapacity);
};

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Terminal
