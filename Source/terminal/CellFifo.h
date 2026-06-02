/**
 * @file CellFifo.h
 * @brief Lock-free SPSC ring buffers for cross-thread Char row delivery.
 *
 * Video pushes raw Char rows on the reader thread via pushHistory() and
 * pushActive().  The View drains on the message thread via drainHistory()
 * and drainActive().
 *
 * Two independent rings — each a jam::BufferSPSC (index-only, drop-oldest):
 *
 *   history ring — carries departed scrollback lines.  pushHistory() writes to
 *                  it; drainHistory() joins continued rows into one logical
 *                  jam::CodeLine (isContinued join — accumulate until a
 *                  non-continued row finalises the run; a partial run remaining
 *                  at drain-end is returned with isContinued = true).
 *
 *   active ring  — carries live viewport rows.  pushActive() writes to it;
 *                  drainActive() yields one jam::CodeLine per entry, no joining.
 *
 * Both rings are DROP-OLDEST: BufferSPSC::prepareToWrite on full advances
 * validStart (drop-oldest) until the requested space fits — the write always
 * succeeds, the producer never stalls.  Back-pressure via getHistoryFreeSpace()
 * / getActiveFreeSpace() is available for callers that prefer to avoid drops.
 *
 * Per-entry layout in each ring (variable-length, in uint64_t slots):
 *   [headerSlot (1) | Char cells[cellCount]]
 *
 * headerSlot packs: int32_t cellCount | uint8_t flags | 3 bytes pad
 *
 * flags bit 0: isContinued (DECAWM flexWrap)
 * flags bit 1: isJustified
 *
 * Seqlock (per-entry torn-read guard):
 *   Each ring carries a HeapBlock<std::atomic<uint64_t>> epoch array — one
 *   epoch per ring slot.  Only header slots carry live epochs; cell-data slots
 *   retain their initial value of epochNeverWritten (0).
 *
 *   Producer write path for entry with header at ring position h:
 *     epoch[h].store(existing_epoch | epochOddBit, release)  — in-progress
 *     memcpy headerSlot + cells into storage
 *     epoch[h].store((existing_epoch | epochOddBit) + 1, release)  — stable
 *
 *   Consumer read path:
 *     before = epoch[h].load(acquire)
 *     if before == epochNeverWritten → never-written slot; skip 1, return torn
 *     if before & epochOddBit       → write-in-progress; skip 1, return torn
 *     memcpy header from storage[h]
 *     after = epoch[h].load(acquire)
 *     if after != before             → reclaimed mid-read; skip 1, return torn
 *     — header is clean; read cellCount and proceed with cells.
 *     epoch check (same s1) before + after cell memcpy; if changed → skip, return torn.
 *     On any torn result the consumer advances past the corrupted slot(s) and stops.
 *     The next drain call re-enters at the new read front.
 *
 * Resync after drop-oldest:
 *   Because drop-oldest advances validStart in raw slots, the read front may
 *   land on a cell-data slot after the producer drops into the middle of the
 *   oldest entry.  Cell-data slots carry epochNeverWritten (0) — the consumer's
 *   "before == epochNeverWritten" guard fires, the slot is skipped (finishedRead
 *   advances by 1), and drain exits via ReadResult::torn.  On the next drain
 *   call the front has advanced past the severed cell data toward the next valid
 *   header.  One torn result per dropped fragment; all are silent (no delivery).
 *
 * @see jam::BufferSPSC
 */
#pragma once

namespace terminal
{
/*____________________________________________________________________________*/

class CellFifo
{
public:
    static constexpr uint8_t isContinuedFlag { 0x01 };
    static constexpr uint8_t isJustifiedFlag { 0x02 };

    /** @brief Constructs with independent capacities for history and active rings.
     *  @param historyCapacityInChars  Maximum total Chars the history ring can hold.
     *  @param activeCapacityInChars   Maximum total Chars the active ring can hold. */
    CellFifo (int historyCapacityInChars, int activeCapacityInChars) noexcept
        : historyRing    (toRingSlots (historyCapacityInChars))
        , activeRing     (toRingSlots (activeCapacityInChars))
        , historyStorage (static_cast<size_t> (historyRing.getTotalSize()))
        , activeStorage  (static_cast<size_t> (activeRing.getTotalSize()))
        , historyEpochs  (static_cast<size_t> (historyRing.getTotalSize()))
        , activeEpochs   (static_cast<size_t> (activeRing.getTotalSize()))
    {
        resetEpochs (historyEpochs, historyRing.getTotalSize());
        resetEpochs (activeEpochs,  activeRing.getTotalSize());
    }

    /** @brief Pushes one row of Chars into the history ring.  Reader thread only.
     *
     *  Packs the row into a uint64_t header + cells and writes via BufferSPSC
     *  prepareToWrite / finishedWrite with seqlock stamping.  BufferSPSC
     *  drop-oldest fires inside prepareToWrite when full — the write always
     *  succeeds.
     *
     *  @param chars   Pointer to the row's Char data.
     *  @param count   Number of Chars in the row.
     *  @param flags   Row flags — bit 0: isContinued, bit 1: isJustified. */
    void pushHistory (const jam::Char* chars, int count, uint8_t flags) noexcept
    {
        pushEntry (historyRing, historyStorage, historyEpochs, chars, count, flags);
    }

    /** @brief Drains one logical history line from the history ring.  Message thread only.
     *
     *  Accumulates continued rows into an internal pending buffer, joining them
     *  into one logical jam::CodeLine.  Returns true (with outLine populated) only
     *  when the last non-continued row of a logical line is reached.  A partial
     *  continued history run remaining at drain-end is returned with isContinued = true.
     *
     *  @param outLine  Receives the joined jam::CodeLine.
     *  @return true if a complete (or partial tail) entry was produced; false when
     *          the history ring is empty or a run has not yet completed. */
    bool drainHistory (jam::CodeLine& outLine) noexcept
    {
        bool produced { false };

        while (not produced)
        {
            jam::CodeLine raw {};
            const ReadResult result { readRawRow (historyRing, historyStorage, historyEpochs, raw) };

            if (result == ReadResult::empty or result == ReadResult::torn)
                break;

            if (result == ReadResult::zeroCellEntry)
            {
                // Zero-cell entry: emit pending if any, then continue draining.
                if (pending.cellCount > 0)
                {
                    pending.isContinued = false;
                    outLine  = std::move (pending);
                    pending  = {};
                    produced = true;
                }
                // else: silent — loop continues to next entry.
            }
            else
            {
                // result == ReadResult::rowProduced — join into pending.
                const int prevCount { pending.cellCount };
                pending.chars.realloc (prevCount + raw.cellCount);

                std::memcpy (pending.chars.getData() + prevCount,
                             raw.chars.getData(),
                             static_cast<size_t> (raw.cellCount) * sizeof (jam::Char));

                pending.cellCount = prevCount + raw.cellCount;

                if (raw.isJustified)
                    pending.isJustified = true;

                if (not raw.isContinued)
                {
                    // End of logical line — emit.
                    pending.isContinued = false;
                    outLine  = std::move (pending);
                    pending  = {};
                    produced = true;
                }
                // else: row is continued — loop to consume the next ring entry.
            }
        }

        if (not produced and pending.cellCount > 0
            and historyRing.getNumReady() < headerSlots)
        {
            // Partial continued history run at drain-end — emit with isContinued set.
            pending.isContinued = true;
            outLine  = std::move (pending);
            pending  = {};
            produced = true;
        }

        return produced;
    }

    /** @brief Pushes one row of Chars into the active ring.  Reader thread only.
     *
     *  @param chars   Pointer to the row's Char data.
     *  @param count   Number of Chars in the row.
     *  @param flags   Row flags — bit 0: isContinued, bit 1: isJustified. */
    void pushActive (const jam::Char* chars, int count, uint8_t flags) noexcept
    {
        pushEntry (activeRing, activeStorage, activeEpochs, chars, count, flags);
    }

    /** @brief Drains one viewport row from the active ring.  Message thread only.
     *
     *  Builds one jam::CodeLine per ring entry — no joining.
     *
     *  @param outLine  Receives the viewport row jam::CodeLine.
     *  @return true if an entry was produced; false when the active ring is empty. */
    bool drainActive (jam::CodeLine& outLine) noexcept
    {
        bool produced { false };

        while (not produced)
        {
            jam::CodeLine raw {};
            const ReadResult result { readRawRow (activeRing, activeStorage, activeEpochs, raw) };

            if (result == ReadResult::empty or result == ReadResult::torn)
                break;

            if (result == ReadResult::rowProduced)
            {
                outLine  = std::move (raw);
                produced = true;
            }
            // Zero-cell entry on the active ring: skip silently (no join accumulator here).
        }

        return produced;
    }

    /** @brief Reallocates both rings, storage, and epoch arrays.
     *  Called while processing is suspended (message thread, no concurrent push).
     *  @param historyCapacityInChars  New capacity for the history ring in Char-equivalent slots.
     *  @param activeCapacityInChars   New capacity for the active ring in Char-equivalent slots. */
    void setSize (int historyCapacityInChars, int activeCapacityInChars) noexcept
    {
        const int historySlots { toRingSlots (historyCapacityInChars) };
        const int activeSlots  { toRingSlots (activeCapacityInChars) };

        historyRing.setTotalSize (historySlots);
        activeRing.setTotalSize  (activeSlots);

        historyStorage.realloc (static_cast<size_t> (historySlots));
        activeStorage.realloc  (static_cast<size_t> (activeSlots));

        historyEpochs.realloc (static_cast<size_t> (historySlots));
        activeEpochs.realloc  (static_cast<size_t> (activeSlots));

        resetEpochs (historyEpochs, historySlots);
        resetEpochs (activeEpochs,  activeSlots);

        pending = {};
    }

    /** @brief Number of free uint64_t slots in the history ring (approximation).
     *  Reader thread only — used for back-pressure gating. */
    int getHistoryFreeSpace() const noexcept { return historyRing.getFreeSpace(); }

    /** @brief Number of occupied uint64_t slots in the history ring (approximation). */
    int getHistoryNumReady() const noexcept { return historyRing.getNumReady(); }

    /** @brief Number of free uint64_t slots in the active ring (approximation).
     *  Reader thread only — used for back-pressure gating. */
    int getActiveFreeSpace() const noexcept { return activeRing.getFreeSpace(); }

    /** @brief Number of occupied uint64_t slots in the active ring (approximation). */
    int getActiveNumReady() const noexcept { return activeRing.getNumReady(); }

private:
    // -------------------------------------------------------------------------
    // Ring sizing helper

    static constexpr int minCharsPerRow { 1 };
    static constexpr int headerSlots    { 1 }; ///< Slots per entry header (one uint64_t).

    /** Convert a Char-count capacity into a uint64_t ring slot count.
     *  Adds per-entry overhead headroom: one header slot per minCharsPerRow cells,
     *  plus one sentinel slot (AbstractFifo invariant: one slot permanently reserved). */
    static int toRingSlots (int capacityInChars) noexcept
    {
        const int entries    { capacityInChars / minCharsPerRow + 1 };
        const int totalSlots { capacityInChars + headerSlots * entries + 1 };
        return juce::jmax (2, totalSlots);
    }

    // -------------------------------------------------------------------------
    // Seqlock epoch constants

    /** Epoch value for a slot that has never been written as a header.
     *  Cell-data slots retain this value; consumers skip them. */
    static constexpr uint64_t epochNeverWritten { 0 };

    /** Bit mask for the in-progress (odd) epoch indicator. */
    static constexpr uint64_t epochOddBit       { 1 };

    // -------------------------------------------------------------------------
    // Header pack / unpack

    /** @brief Pack cellCount + flags into one uint64_t slot (strict-aliasing-clean). */
    static uint64_t packHeader (int32_t cellCount, uint8_t flags) noexcept
    {
        uint64_t word { 0 };
        std::memcpy (&word,                                            &cellCount, sizeof (int32_t));
        std::memcpy (reinterpret_cast<char*> (&word) + sizeof (int32_t), &flags, sizeof (uint8_t));
        return word;
    }

    /** @brief Unpack cellCount + flags from one uint64_t slot. */
    static void unpackHeader (uint64_t word, int32_t& cellCount, uint8_t& flags) noexcept
    {
        std::memcpy (&cellCount, &word, sizeof (int32_t));
        std::memcpy (&flags, reinterpret_cast<const char*> (&word) + sizeof (int32_t), sizeof (uint8_t));
    }

    // -------------------------------------------------------------------------
    // Epoch array helpers

    using EpochBlock = juce::HeapBlock<std::atomic<uint64_t>>;

    /** @brief Reset all epoch slots to epochNeverWritten (zero-initialise). */
    static void resetEpochs (EpochBlock& epochs, int slotCount) noexcept
    {
        for (int i { 0 }; i < slotCount; ++i)
            epochs[i].store (epochNeverWritten, std::memory_order_relaxed);
    }

    // -------------------------------------------------------------------------
    // ReadResult

    /** @brief Result of a single readRawRow() call — four mutually exclusive states. */
    enum class ReadResult : uint8_t
    {
        empty,         ///< Ring had no ready data — caller should stop draining.
        torn,          ///< Seqlock detected a torn read — caller should stop draining.
        zeroCellEntry, ///< Entry consumed; cellCount == 0 — caller handles join semantics.
        rowProduced,   ///< Entry consumed; outLine is populated.
    };

    // -------------------------------------------------------------------------
    // Per-drain scratch buffer (heap; reallocated as needed)

    static constexpr int scratchInitialSlots { 256 };
    juce::HeapBlock<uint64_t> scratch        { scratchInitialSlots };
    int                       scratchCapacity { scratchInitialSlots };

    /** @brief Ensure scratch can hold at least `needed` uint64_t slots. */
    void ensureScratch (int needed) noexcept
    {
        if (needed > scratchCapacity)
        {
            scratch.realloc (static_cast<size_t> (needed));
            scratchCapacity = needed;
        }
    }

    // -------------------------------------------------------------------------
    // Producer write helper — shared by pushHistory and pushActive

    /** @brief Packs one entry and writes it into the ring with seqlock stamping.
     *
     *  BufferSPSC::prepareToWrite handles drop-oldest internally — the write always
     *  succeeds.  The seqlock stamps the header slot: odd before, even after.
     *
     *  @param ring     The BufferSPSC managing indices.
     *  @param storage  The uint64_t backing store aligned with the ring.
     *  @param epochs   Epoch array (one entry per ring slot).
     *  @param chars    Source Char data.
     *  @param count    Number of Chars.
     *  @param flags    Row flags. */
    void pushEntry (jam::BufferSPSC&          ring,
                    juce::HeapBlock<uint64_t>& storage,
                    EpochBlock&                epochs,
                    const jam::Char*           chars,
                    int                        count,
                    uint8_t                    flags) noexcept
    {
        const int slotsNeeded { headerSlots + count };

        int s1 { 0 }, b1 { 0 }, s2 { 0 }, b2 { 0 };
        ring.prepareToWrite (slotsNeeded, s1, b1, s2, b2);

        // s1 is the header slot position.  Stamp odd (in-progress) before writing.
        const uint64_t prevEpoch { epochs[s1].load (std::memory_order_relaxed) };
        const uint64_t oddEpoch  { (prevEpoch | epochOddBit) };
        epochs[s1].store (oddEpoch, std::memory_order_release);

        // Write header into s1.
        storage[s1] = packHeader (static_cast<int32_t> (count), flags);

        // Write cells after the header, respecting the ring wrap.
        // Block 1 covers up to (ringSize - s1) slots from s1.
        // If the entry wraps, block 2 picks up the remainder from slot 0.
        // The cell region starts immediately after the header slot.
        if (count > 0)
        {
            const int ringSize  { ring.getTotalSize() };
            int       cellsLeft { count };
            int       srcOffset { 0 };

            // Cell start is one slot past the header.
            int cellPos { s1 + 1 };

            if (cellPos >= ringSize)
                cellPos -= ringSize;

            // First contiguous segment: min(count, ringSize - cellPos) cells.
            const int seg1Cells { juce::jmin (cellsLeft, ringSize - cellPos) };
            std::memcpy (storage.getData() + cellPos,
                         reinterpret_cast<const uint64_t*> (chars) + srcOffset,
                         static_cast<size_t> (seg1Cells) * sizeof (uint64_t));

            cellsLeft -= seg1Cells;
            srcOffset += seg1Cells;

            // Second contiguous segment (wrap-around): starts at slot 0.
            if (cellsLeft > 0)
            {
                std::memcpy (storage.getData(),
                             reinterpret_cast<const uint64_t*> (chars) + srcOffset,
                             static_cast<size_t> (cellsLeft) * sizeof (uint64_t));
            }
        }

        // Stamp even (stable) — must be strictly greater than oddEpoch.
        epochs[s1].store (oddEpoch + 1, std::memory_order_release);

        ring.finishedWrite (slotsNeeded);
    }

    // -------------------------------------------------------------------------
    // Consumer read helper — shared by drainHistory and drainActive

    /** @brief Reads one raw ring entry from a BufferSPSC ring into outLine.
     *
     *  Shared mechanics for drainHistory and drainActive.  Checks seqlock
     *  epoch at the header slot before and after copying — discards and reports
     *  ReadResult::torn if the producer reclaimed the entry mid-read.
     *
     *  Resync: a cell-data slot at the read front carries epochNeverWritten (0);
     *  the epoch guard fires, the slot is consumed via finishedRead(s1, 1), and
     *  ReadResult::torn is returned so the caller stops this drain.  The next
     *  drain call re-enters past the stale slot.
     *
     *  @param ring     The BufferSPSC managing indices.
     *  @param storage  The uint64_t backing store.
     *  @param epochs   Epoch array.
     *  @param outLine  Receives the raw CodeLine when ReadResult::rowProduced.
     *  @return ReadResult indicating outcome. */
    ReadResult readRawRow (jam::BufferSPSC&          ring,
                           juce::HeapBlock<uint64_t>& storage,
                           EpochBlock&                epochs,
                           jam::CodeLine&             outLine) noexcept
    {
        if (ring.getNumReady() < headerSlots)
            return ReadResult::empty;

        // ---- Read header slot ----
        int s1 { 0 }, b1 { 0 }, s2 { 0 }, b2 { 0 };
        ring.prepareToRead (headerSlots, s1, b1, s2, b2);

        if (b1 < headerSlots)
            return ReadResult::empty;

        // Seqlock: check epoch before reading.
        const uint64_t epochBefore { epochs[s1].load (std::memory_order_acquire) };

        if (epochBefore == epochNeverWritten or (epochBefore & epochOddBit) != 0)
        {
            // Never-written cell-data slot or write-in-progress — skip 1 slot and report torn.
            ring.finishedRead (s1, headerSlots);
            return ReadResult::torn;
        }

        // Copy header word.
        const uint64_t headerWord { storage[s1] };

        // Seqlock: check epoch after copying.
        const uint64_t epochAfter { epochs[s1].load (std::memory_order_acquire) };

        if (epochAfter != epochBefore)
        {
            // Producer reclaimed this slot mid-read — skip 1 slot and report torn.
            ring.finishedRead (s1, headerSlots);
            return ReadResult::torn;
        }

        // Header is stable — unpack.
        int32_t cellCount { 0 };
        uint8_t flags     { 0 };
        unpackHeader (headerWord, cellCount, flags);

        // Guard: cellCount must be non-negative and plausible.
        // A cell-data slot that somehow carries a non-zero even epoch (recycled header)
        // could still yield an implausible cellCount.
        const int ringCapacity { ring.getTotalSize() };
        const bool cellCountValid { cellCount >= 0 and cellCount < ringCapacity };

        if (not cellCountValid)
        {
            // Implausible cellCount — treat as torn: skip 1 slot.
            ring.finishedRead (s1, headerSlots);
            return ReadResult::torn;
        }

        if (cellCount == 0)
        {
            ring.finishedRead (s1, headerSlots);
            return ReadResult::zeroCellEntry;
        }

        // ---- Read cell slots ----
        // Verify the ring has enough ready slots (header + cells).
        if (ring.getNumReady() < headerSlots + cellCount)
        {
            // Partial entry visible — producer hasn't finished writing yet.
            // Do NOT consume the header; leave the read front where it is.
            return ReadResult::empty;
        }

        ensureScratch (cellCount);

        // Cell data occupies the slots immediately after the header slot (s1).
        // prepareToRead is const / non-advancing — s1 is still the ring front.
        // Compute cell region directly: starts at slot (s1 + headerSlots) % ringSize.
        {
            const int ringSize  { ring.getTotalSize() };
            int       cellsLeft { cellCount };
            int       dstOffset { 0 };

            int cellPos { s1 + headerSlots };

            if (cellPos >= ringSize)
                cellPos -= ringSize;

            const int seg1Cells { juce::jmin (cellsLeft, ringSize - cellPos) };

            // Seqlock epoch check before cell copy.
            const uint64_t cellEpochBefore { epochs[s1].load (std::memory_order_acquire) };

            std::memcpy (scratch.getData() + dstOffset,
                         storage.getData() + cellPos,
                         static_cast<size_t> (seg1Cells) * sizeof (uint64_t));

            cellsLeft -= seg1Cells;
            dstOffset += seg1Cells;

            if (cellsLeft > 0)
            {
                std::memcpy (scratch.getData() + dstOffset,
                             storage.getData(),
                             static_cast<size_t> (cellsLeft) * sizeof (uint64_t));
            }

            // Seqlock epoch check after cell copy.
            const uint64_t cellEpochAfter { epochs[s1].load (std::memory_order_acquire) };

            if (cellEpochAfter != cellEpochBefore)
            {
                // Producer reclaimed this entry while we were copying cells.
                ring.finishedRead (s1, headerSlots + cellCount);
                return ReadResult::torn;
            }
        }

        // Entry is clean — allocate and populate the CodeLine.
        outLine.chars.allocate (cellCount, false);

        std::memcpy (outLine.chars.getData(),
                     scratch.getData(),
                     static_cast<size_t> (cellCount) * sizeof (jam::Char));

        outLine.cellCount   = cellCount;
        outLine.isContinued = (flags & isContinuedFlag) != 0;
        outLine.isJustified = (flags & isJustifiedFlag) != 0;

        ring.finishedRead (s1, headerSlots + cellCount);

        return ReadResult::rowProduced;
    }

    // =========================================================================

    jam::BufferSPSC           historyRing;
    jam::BufferSPSC           activeRing;

    juce::HeapBlock<uint64_t> historyStorage;
    juce::HeapBlock<uint64_t> activeStorage;

    EpochBlock                historyEpochs;
    EpochBlock                activeEpochs;

    /** @brief Pending partial history line — accumulates continued rows across drainHistory calls. */
    jam::CodeLine pending;

    //==========================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CellFifo)
};

/**______________________________END OF NAMESPACE______________________________*/
} // namespace terminal
