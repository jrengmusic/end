/**
 * @file Grid.h
 * @brief SPSC FIFO transport between the reader thread and the message thread.
 *
 * Grid is a pure transport — it has no knowledge of terminal state, geometry,
 * or rendering.  The reader thread (PTY/parser) pushes Command objects into the
 * FIFO.  The message thread drains them in batch for processing.
 *
 * Ownership model:
 *   - One writer: reader thread (push)
 *   - One reader: message thread (drain, resize)
 *
 * No synchronisation primitives beyond juce::AbstractFifo are needed.
 * Command must be trivially copyable (enforced by std::memcpy in drain).
 */

#pragma once

#include <JuceHeader.h>

#include "../data/Command.h"

namespace Terminal
{ /*____________________________________________________________________________*/

/**
 * @class Grid
 * @brief SPSC FIFO transport wrapping juce::AbstractFifo and a HeapBlock of
 *        Command slots.
 *
 * push() is safe to call from the reader thread only.
 * drain() and resize() are safe to call from the message thread only.
 * isEmpty() may be called from either thread (AbstractFifo::getNumReady is
 * atomic).
 */
class Grid
{
public:

    /**
     * @brief Constructs the Grid with a fixed-size command buffer.
     * @param capacity  Maximum number of Command slots to allocate.
     *
     * Called from the message thread before the reader thread starts.
     */
    explicit Grid (int capacity) noexcept
        : fifo (capacity)
    {
        ops.allocate (capacity, true);
    }

    /**
     * @brief Pushes one command into the FIFO.
     *
     * Called from the reader thread only.  No-op when the FIFO is full
     * (backpressure — command is silently dropped).
     *
     * @param command  The command to enqueue.
     */
    void push (const Command& command) noexcept
    {
        int start1 { 0 };
        int size1 { 0 };
        int start2 { 0 };
        int size2 { 0 };
        fifo.prepareToWrite (1, start1, size1, start2, size2);

        if (size1 > 0)
        {
            ops[start1] = command;
            fifo.finishedWrite (1);
        }
    }

    /**
     * @brief Drains up to maxCommands pending commands into outBuffer.
     *
     * Called from the message thread only.  Uses std::memcpy — Command must
     * be trivially copyable.
     *
     * @param outBuffer    Destination buffer; caller owns and sizes it.
     * @param maxCommands  Maximum number of commands to drain in one call.
     * @return             Number of commands actually drained.
     */
    int drain (Command* outBuffer, int maxCommands) noexcept
    {
        int start1 { 0 };
        int size1 { 0 };
        int start2 { 0 };
        int size2 { 0 };
        fifo.prepareToRead (maxCommands, start1, size1, start2, size2);

        const int total { size1 + size2 };

        if (size1 > 0)
            std::memcpy (outBuffer, ops.getData() + start1, static_cast<size_t> (size1) * sizeof (Command));

        if (size2 > 0)
            std::memcpy (outBuffer + size1, ops.getData() + start2, static_cast<size_t> (size2) * sizeof (Command));

        fifo.finishedRead (total);
        return total;
    }

    /**
     * @brief Returns true when no commands are pending.
     *
     * May be called from either thread.  AbstractFifo::getNumReady uses an
     * atomic load internally.
     */
    bool isEmpty() const noexcept
    {
        return fifo.getNumReady() == 0;
    }

    /**
     * @brief Reallocates the FIFO to a new capacity, discarding all pending
     *        commands.
     *
     * Called from the message thread only.  The reader thread must not be
     * pushing during this call.
     *
     * @param newCapacity  New maximum number of Command slots.
     */
    void resize (int newCapacity) noexcept
    {
        fifo.setTotalSize (newCapacity);
        ops.allocate (newCapacity, true);
    }

private:

    juce::AbstractFifo fifo;
    juce::HeapBlock<Command> ops;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Grid)
};

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Terminal
