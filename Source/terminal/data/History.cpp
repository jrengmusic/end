/**
 * @file History.cpp
 * @brief Implementation of Terminal::History — fixed-capacity byte ring buffer.
 *
 * @see History.h
 */

#include "History.h"

#include <cstring>

namespace Terminal
{ /*____________________________________________________________________________*/

/**
 * @brief Constructs a History with capacity = scrollbackLines × bytesPerLineEstimate.
 *
 * @param scrollbackLines  Must be > 0.
 */
History::History (int scrollbackLines)
    : capacity { static_cast<size_t> (scrollbackLines) * static_cast<size_t> (bytesPerLineEstimate) }
{
    jassert (scrollbackLines > 0);
    buffer.calloc (capacity);
}

/**
 * @brief Appends raw PTY bytes to the ring buffer.
 *
 * FIFO eviction: if @p len bytes do not fit, the oldest bytes are discarded
 * to make room.  If @p len >= capacity, only the last `capacity` bytes are
 * retained and the buffer is considered full.
 *
 * No early returns — positive nesting throughout.
 *
 * @note READER THREAD.
 */
void History::append (const char* data, size_t len) noexcept
{
    jassert (data != nullptr);

    if (len > 0 and capacity > 0)
    {
        const juce::ScopedLock sl (lock);

        if (len >= capacity)
        {
            // Entire capacity is replaced by the trailing bytes of this chunk.
            // Skip ahead in data to the last `capacity` bytes.
            const char* src { data + (len - capacity) };
            std::memcpy (buffer.getData(), src, capacity);
            head = 0;
            used = capacity;
        }
        else
        {
            // Evict old bytes if necessary then copy new bytes.
            // Because len < capacity there is always room after eviction.
            const size_t free { capacity - used };

            if (len > free)
            {
                // Advance head past the bytes we are about to evict.
                const size_t evict { len - free };
                head = (head + evict) % capacity;
                used -= evict;
            }

            // Write-head for the new bytes starts at (head + used) % capacity.
            size_t writeHead { (head + used) % capacity };
            const size_t firstChunk { capacity - writeHead };

            if (len <= firstChunk)
            {
                std::memcpy (buffer.getData() + writeHead, data, len);
            }
            else
            {
                std::memcpy (buffer.getData() + writeHead, data, firstChunk);
                std::memcpy (buffer.getData(), data + firstChunk, len - firstChunk);
            }

            used += len;
        }
    }
}

/**
 * @brief Returns a chronological snapshot of all buffered bytes.
 *
 * Linearises the ring into a contiguous `juce::MemoryBlock`.
 *
 * @return A copy of the history bytes, oldest first.
 * @note MESSAGE THREAD.
 */
juce::MemoryBlock History::snapshot() const noexcept
{
    const juce::ScopedLock sl (lock);

    juce::MemoryBlock result;

    if (used > 0)
    {
        result.setSize (used);
        char* dest { static_cast<char*> (result.getData()) };

        const size_t firstChunk { capacity - head };

        if (used <= firstChunk)
        {
            std::memcpy (dest, buffer.getData() + head, used);
        }
        else
        {
            std::memcpy (dest, buffer.getData() + head, firstChunk);
            std::memcpy (dest + firstChunk, buffer.getData(), used - firstChunk);
        }
    }

    return result;
}

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Terminal
