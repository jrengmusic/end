/**
 * @file GridSerialize.cpp
 * @brief Grid member functions for binary snapshot serialization.
 *
 * This translation unit implements `getStateInformation()` and
 * `setStateInformation()` — the flat binary snapshot protocol that
 * Processor delegates to Grid for session persistence.
 *
 * @see Grid.h for the full class documentation.
 */

#include "Grid.h"

namespace Terminal
{ /*____________________________________________________________________________*/

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
        destData.append (buffer.cells.getData(),      cellCount * sizeof (Cell));
        destData.append (buffer.graphemes.getData(),  cellCount * sizeof (Grapheme));
        destData.append (buffer.linkIds.getData(),    cellCount * sizeof (uint16_t));
        destData.append (buffer.rowStates.getData(),  static_cast<size_t> (totalRows) * sizeof (RowState));
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
        const size_t cellBytes     { cellCount * sizeof (Cell) };
        const size_t graphemeBytes { cellCount * sizeof (Grapheme) };
        const size_t linkIdBytes   { cellCount * sizeof (uint16_t) };
        const size_t rowStateBytes { static_cast<size_t> (totalRows) * sizeof (RowState) };

        if ((cursor + static_cast<ptrdiff_t> (cellBytes + graphemeBytes + linkIdBytes + rowStateBytes)) > end)
            break;

        buffer.cells.allocate     (cellCount,                              false);
        buffer.graphemes.allocate (cellCount,                              true);
        buffer.linkIds.allocate   (cellCount,                              true);
        buffer.rowStates.allocate (static_cast<size_t> (totalRows),       true);
        buffer.rowSeqnos.allocate (static_cast<size_t> (totalRows),       true);

        std::memcpy (buffer.cells.getData(),     cursor, cellBytes);     cursor += static_cast<ptrdiff_t> (cellBytes);
        std::memcpy (buffer.graphemes.getData(), cursor, graphemeBytes); cursor += static_cast<ptrdiff_t> (graphemeBytes);
        std::memcpy (buffer.linkIds.getData(),   cursor, linkIdBytes);   cursor += static_cast<ptrdiff_t> (linkIdBytes);
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
