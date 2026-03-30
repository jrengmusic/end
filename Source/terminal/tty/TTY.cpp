/**
 * @file TTY.cpp
 * @brief TTY reader thread implementation — shared across all platform backends.
 *
 * This file contains only TTY::run(), the reader thread loop that is common to
 * all platform implementations.  Platform-specific code lives in UnixTTY.cpp
 * and WindowsTTY.cpp.
 *
 * ### Reader thread lifecycle
 *
 * 1. Thread starts inside the platform `open()` call via `startThread()`.
 * 2. `run()` sets thread priority to high and enters the main poll loop.
 * 3. Each iteration blocks on `waitForData`.
 * 4. When data arrives the inner drain loop calls `read()` until no bytes
 *    remain, delivering each chunk to `onData`.
 * 5. After a full drain `onDrainComplete` is called once.
 * 6. On EOF (`read()` returns -1) the thread sets `shellExited`, dispatches
 *    `onExit` to the message thread, and returns.
 * 7. Thread stops when `threadShouldExit()` becomes true (set by `close()`).
 *
 * @see TTY::run()
 * @see UnixTTY
 * @see WindowsTTY
 */

#include "TTY.h"

/**
 * @brief Reader thread main loop.
 *
 * Runs at high priority.  Handles resize detection, data delivery, drain
 * notification, and shell-exit detection.
 *
 * @par Data delivery
 * After `waitForData` returns `true` the inner loop calls `read()` repeatedly
 * until it returns 0 (no more data) or -1 (EOF / error).  Each positive read
 * delivers the chunk to `onData`.  The tight inner loop avoids re-entering
 * `waitForData` between chunks, which would add unnecessary latency when the
 * kernel buffer holds multiple chunks.
 *
 * @par Drain notification
 * `onDrainComplete` fires once after the inner loop exits with 0 (no more
 * data).  This gives the owner a hook to flush deferred writes — for example,
 * sending the initial shell prompt query after the shell has finished printing
 * its startup banner and switched to raw / no-echo mode.
 *
 * @par Shell exit
 * When `read()` returns -1 the shell has closed the master fd (EOF).
 * `shellExited` is set and `onExit` is dispatched to the message thread via
 * `juce::MessageManager::callAsync` before the thread returns.
 *
 * @note READER THREAD context.  Do not call directly.
 */

void TTY::run()
{
    setPriority (Thread::Priority::high);
    char chunk[READ_CHUNK_SIZE];

    auto drainPty = [&]() -> bool
    {
        int n { read (chunk, static_cast<int> (READ_CHUNK_SIZE)) };

        while (n > 0)
        {
            if (onData)
            {
                onData (chunk, n);
            }

            n = read (chunk, static_cast<int> (READ_CHUNK_SIZE));
        }

        const bool isEof { n < 0 };

        if (not isEof and onDrainComplete)
        {
            onDrainComplete();
        }

        if (isEof)
        {
            shellExited.store (true, std::memory_order_release);

            if (onExit)
            {
                juce::MessageManager::callAsync (onExit);
            }
        }

        return isEof;
    };

    bool shellDone { false };

    while (not threadShouldExit() and not shellDone)
    {
        if (waitForData (100))
        {
            shellDone = drainPty();
        }
    }
}
