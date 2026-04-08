/**
 * @file History.h
 * @brief Fixed-capacity byte ring buffer for PTY output history.
 *
 * `Terminal::History` records raw PTY bytes in a fixed-capacity ring buffer.
 * When the buffer is full, the oldest bytes are evicted FIFO to make room for
 * new arrivals.
 *
 * ### Thread ownership
 * - `append()` — READER THREAD (called from TTY::onData)
 * - `snapshot()` — MESSAGE THREAD
 *
 * Both are protected by an internal `juce::CriticalSection`.
 *
 * ### Architecture
 * History is owned by `Terminal::Session` alongside the TTY.  It is the
 * daemon-side record of all bytes that flowed through the PTY.  On client
 * attach, `Session::snapshotHistory()` returns a `juce::MemoryBlock` that
 * is forwarded to the client as a `Message::history` PDU and fed through
 * `Processor::process` to reconstruct the display state.
 *
 * @see Terminal::Session
 * @see Terminal::Processor
 */

#pragma once

#include <JuceHeader.h>

namespace Terminal
{ /*____________________________________________________________________________*/

/**
 * @class History
 * @brief Fixed-capacity byte ring buffer — records raw PTY output bytes.
 *
 * Capacity is established at construction and never changes.  When full,
 * the oldest bytes are silently evicted to make room.  Thread-safe via an
 * internal `juce::CriticalSection`.
 *
 * Single responsibility: store bytes, produce snapshots.  No parser, no
 * state, no display logic.
 */
class History
{
public:
    /** @brief Estimated bytes per terminal line used for capacity calculation. */
    static constexpr int bytesPerLineEstimate { 256 };

    /**
     * @brief Constructs a History with capacity sized for @p scrollbackLines.
     *
     * Capacity = `scrollbackLines × bytesPerLineEstimate`.
     *
     * @param scrollbackLines  Number of scrollback lines to budget for.
     *                         Must be > 0.
     */
    explicit History (int scrollbackLines);

    /**
     * @brief Appends raw PTY bytes to the ring buffer.
     *
     * If @p len exceeds the available free space, the oldest bytes are evicted
     * FIFO until there is room.  If @p len exceeds the entire capacity, only
     * the trailing `capacity` bytes of the input are retained (the oldest bytes
     * in a single oversized chunk are also evicted).
     *
     * @param data  Pointer to raw PTY bytes.  Must not be null.
     * @param len   Number of bytes to append.
     * @note READER THREAD — called from TTY::onData.
     */
    void append (const char* data, size_t len) noexcept;

    /**
     * @brief Returns a snapshot of all buffered bytes in chronological order.
     *
     * The returned block is a contiguous copy — safe to forward over IPC.
     *
     * @return A `juce::MemoryBlock` containing the history bytes oldest-first.
     * @note MESSAGE THREAD — called to produce the initial `Message::history` payload.
     */
    juce::MemoryBlock snapshot() const noexcept;

private:
    /** @brief Maximum number of bytes this ring buffer holds. */
    const size_t capacity;

    /** @brief The ring buffer storage. */
    juce::HeapBlock<char> buffer;

    /** @brief Write-head: physical index of the next byte to write. */
    size_t head { 0 };

    /** @brief Number of valid bytes currently stored (<= capacity). */
    size_t used { 0 };

    /** @brief Guards append() and snapshot() across reader/message threads. */
    mutable juce::CriticalSection lock;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (History)
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Terminal
