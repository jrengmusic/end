/**
 * @file CellFifo.h
 * @brief Lock-free SPSC ring buffer for cross-thread cell row delivery.
 *
 * Video pushes raw cell rows on the reader thread via pushRow().
 * Processor drains on the message thread via drainInto(), which joins
 * continued rows into logical TextLines and commits them to TextLineArray.
 *
 * Storage is a pre-allocated HeapBlock<char> sized at construction.
 * AbstractFifo manages read/write indices — lock-free, SPSC-safe.
 *
 * Per-entry layout in the ring (variable-length):
 *   [int32_t cellCount | uint8_t flags | Cell cells[cellCount]]
 *
 * flags bit 0: isContinued (flexWrap)
 * flags bit 1: isJustified
 *
 * @see juce::AbstractFifo
 */
#pragma once

namespace terminal
{
/*____________________________________________________________________________*/

class CellFifo
{
public:
    /** @brief Constructs with the given capacity in Cell-equivalent slots.
     *  Actual byte allocation includes header overhead per entry.
     *  @param capacityInCells  Maximum total cells the ring can hold. */
    explicit CellFifo (int capacityInCells) noexcept
        : fifo (capacityInCells + headerSlots * (capacityInCells / minCellsPerRow + 1))
        , buffer (static_cast<size_t> (fifo.getTotalSize()))
    {
    }

    /** @brief Pushes one row of cells into the ring. Reader thread only.
     *
     *  Writes a header (cellCount + flags packed into headerSlots Cell-sized slots)
     *  followed by the cell data. If the ring is full, drops oldest entries
     *  to make room (advances read pointer).
     *
     *  @param cells  Pointer to the row's cell data.
     *  @param count  Number of cells in the row.
     *  @param flags  Row flags — bit 0: isContinued, bit 1: isJustified.
     */
    void pushRow (const jam::Cell* cells, int count, uint8_t flags) noexcept
    {
        const int slotsNeeded { headerSlots + count };

        // Drop oldest if ring is full (overflow policy: drop oldest).
        while (fifo.getFreeSpace() < slotsNeeded)
        {
            // Skip one entry from the read side to free space.
            int s1 { 0 }, b1 { 0 }, s2 { 0 }, b2 { 0 };
            fifo.prepareToRead (headerSlots, s1, b1, s2, b2);

            if (b1 + b2 >= headerSlots)
            {
                // Read the header to find out how many cells to skip.
                Header hdr {};
                readHeader (hdr, s1, b1, s2, b2);
                fifo.finishedRead (headerSlots);

                // Skip the cell data.
                int cs1 { 0 }, cb1 { 0 }, cs2 { 0 }, cb2 { 0 };
                fifo.prepareToRead (hdr.cellCount, cs1, cb1, cs2, cb2);
                fifo.finishedRead (hdr.cellCount);
            }
            else
            {
                // Not enough data to read a header — ring is corrupt or empty.
                fifo.finishedRead (b1 + b2);
                break;
            }
        }

        // Write header.
        int s1 { 0 }, b1 { 0 }, s2 { 0 }, b2 { 0 };
        fifo.prepareToWrite (slotsNeeded, s1, b1, s2, b2);

        if (b1 + b2 >= slotsNeeded)
        {
            // Pack header into first headerSlots slots.
            Header hdr { count, flags };
            writeHeader (hdr, s1, b1, s2, b2);

            // Write cell data after header.
            const int cellStart { (s1 + headerSlots) };
            const int cellStartWrapped { cellStart >= fifo.getTotalSize() ? cellStart - fifo.getTotalSize() : cellStart };

            // Recalculate blocks for cell data region.
            const int cellsInBlock1 { juce::jmin (count, fifo.getTotalSize() - cellStartWrapped) };
            const int cellsInBlock2 { count - cellsInBlock1 };

            std::memcpy (buffer.getData() + cellStartWrapped, cells, static_cast<size_t> (cellsInBlock1) * sizeof (jam::Cell));

            if (cellsInBlock2 > 0)
                std::memcpy (buffer.getData(), cells + cellsInBlock1, static_cast<size_t> (cellsInBlock2) * sizeof (jam::Cell));

            fifo.finishedWrite (slotsNeeded);
        }
    }

    /** @brief Drains all ready entries, joins continued rows into logical TextLines,
     *  and appends them to the target TextLineArray. Message thread only.
     *
     *  @param target  TextLineArray to append committed logical lines to.
     *  @return Number of logical lines committed. */
    int drainInto (jam::TextLineArray& target) noexcept
    {
        int linesCommitted { 0 };
        jam::TextLine pending;

        while (fifo.getNumReady() >= headerSlots)
        {
            // Read header.
            int s1 { 0 }, b1 { 0 }, s2 { 0 }, b2 { 0 };
            fifo.prepareToRead (headerSlots, s1, b1, s2, b2);

            if (b1 + b2 < headerSlots)
            {
                fifo.finishedRead (0);
                break;
            }

            Header hdr {};
            readHeader (hdr, s1, b1, s2, b2);
            fifo.finishedRead (headerSlots);

            // Read cell data.
            if (hdr.cellCount > 0 and fifo.getNumReady() >= hdr.cellCount)
            {
                int cs1 { 0 }, cb1 { 0 }, cs2 { 0 }, cb2 { 0 };
                fifo.prepareToRead (hdr.cellCount, cs1, cb1, cs2, cb2);

                // Append cells to the pending logical line.
                const int prevCount { pending.cellCount };
                pending.cells.realloc (prevCount + hdr.cellCount);

                if (cb1 > 0)
                    std::memcpy (pending.cells.getData() + prevCount,
                                 buffer.getData() + cs1,
                                 static_cast<size_t> (cb1) * sizeof (jam::Cell));

                if (cb2 > 0)
                    std::memcpy (pending.cells.getData() + prevCount + cb1,
                                 buffer.getData() + cs2,
                                 static_cast<size_t> (cb2) * sizeof (jam::Cell));

                pending.cellCount = prevCount + hdr.cellCount;
                fifo.finishedRead (hdr.cellCount);
            }
            else if (hdr.cellCount > 0)
            {
                // Not enough cell data ready — shouldn't happen in SPSC, but guard.
                break;
            }

            if (hdr.flags & isJustifiedFlag)
                pending.isJustified = true;

            // If this row is NOT continued, the logical line is complete.
            if (not (hdr.flags & isContinuedFlag))
            {
                pending.isContinued = false;
                target.add (std::move (pending));
                pending = {};
                ++linesCommitted;
            }
        }

        // If there's a partial continued line left (all rows were continued),
        // commit it — the continuation will be joined on the next drain.
        if (pending.cellCount > 0)
        {
            pending.isContinued = true;
            target.add (std::move (pending));
            ++linesCommitted;
        }

        return linesCommitted;
    }

    /** @brief Reallocates the ring. Called while processing is suspended.
     *  @param capacityInCells  New capacity in Cell-equivalent slots. */
    void reset (int capacityInCells) noexcept
    {
        const int totalSlots { capacityInCells + headerSlots * (capacityInCells / minCellsPerRow + 1) };
        fifo.setTotalSize (totalSlots);
        buffer.realloc (static_cast<size_t> (totalSlots));
    }

    /** @brief Number of Cell-equivalent slots ready to read. */
    int getNumReady() const noexcept { return fifo.getNumReady(); }

private:
    static constexpr int headerSlots { 1 };         ///< Header occupies 1 Cell-sized slot (8 bytes = int32 + uint8 + padding).
    static constexpr int minCellsPerRow { 1 };       ///< Minimum cells per row for capacity calculation.

public:
    static constexpr uint8_t isContinuedFlag { 0x01 };
    static constexpr uint8_t isJustifiedFlag { 0x02 };

private:

    struct Header
    {
        int32_t cellCount { 0 };
        uint8_t flags { 0 };
    };

    juce::AbstractFifo fifo;
    juce::HeapBlock<jam::Cell> buffer;

    void writeHeader (const Header& hdr, int s1, int b1, int /*s2*/, int /*b2*/) noexcept
    {
        // Header fits in 1 Cell-sized slot (8 bytes). Pack cellCount + flags into the slot.
        jam::Cell headerCell {};
        auto* raw { reinterpret_cast<char*> (&headerCell) };
        std::memcpy (raw, &hdr.cellCount, sizeof (int32_t));
        std::memcpy (raw + sizeof (int32_t), &hdr.flags, sizeof (uint8_t));

        buffer[s1] = headerCell;
    }

    void readHeader (Header& hdr, int s1, int b1, int /*s2*/, int /*b2*/) const noexcept
    {
        jam::Cell headerCell {};

        headerCell = buffer[s1];

        auto* raw { reinterpret_cast<const char*> (&headerCell) };
        std::memcpy (&hdr.cellCount, raw, sizeof (int32_t));
        std::memcpy (&hdr.flags, raw + sizeof (int32_t), sizeof (uint8_t));
    }

    //==========================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CellFifo)
};

/**______________________________END OF NAMESPACE______________________________*/
} // namespace terminal
