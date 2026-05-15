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
#include <chrono>

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

    // Diagnostic counters (reader thread only -- no sync needed).
    int64_t totalDrains { 0 };
    int64_t totalChunks { 0 };
    int64_t totalBytes  { 0 };
    double  totalDrainMs   { 0.0 };
    double  totalPollMs    { 0.0 };
    double  totalProcessMs { 0.0 };
    auto lastReport { std::chrono::steady_clock::now() };

    auto drainPty = [&]() -> bool
    {
        const auto drainStart { std::chrono::steady_clock::now() };

        int n { read (chunk, static_cast<int> (READ_CHUNK_SIZE)) };

        while (n > 0)
        {
            if (onData)
            {
                const auto procStart { std::chrono::steady_clock::now() };
                onData (chunk, n);
                const auto procEnd { std::chrono::steady_clock::now() };
                totalProcessMs += std::chrono::duration<double, std::milli> (procEnd - procStart).count();
            }

            totalBytes += n;
            ++totalChunks;
            n = read (chunk, static_cast<int> (READ_CHUNK_SIZE));
        }

        const bool isEof { n < 0 };

        if (not isEof and onDrainComplete)
        {
            onDrainComplete();
        }

        const auto drainEnd { std::chrono::steady_clock::now() };
        totalDrainMs += std::chrono::duration<double, std::milli> (drainEnd - drainStart).count();
        ++totalDrains;

        // Report every 500ms.
        const auto now { std::chrono::steady_clock::now() };
        const double sinceLast { std::chrono::duration<double, std::milli> (now - lastReport).count() };

        if (sinceLast >= 500.0)
        {
            jam::debug::Log::write (
                juce::String ("READER: drains=") + juce::String (totalDrains)
                + " chunks=" + juce::String (totalChunks)
                + " bytes=" + juce::String (totalBytes)
                + " drainMs=" + juce::String (totalDrainMs, 1)
                + " pollMs=" + juce::String (totalPollMs, 1)
                + " processMs=" + juce::String (totalProcessMs, 1)
                + " avgChunkBytes=" + juce::String (totalChunks > 0 ? totalBytes / totalChunks : 0));

            totalDrains    = 0;
            totalChunks    = 0;
            totalBytes     = 0;
            totalDrainMs   = 0.0;
            totalPollMs    = 0.0;
            totalProcessMs = 0.0;
            lastReport     = now;
        }

        if (isEof)
        {
            // Final report.
            jam::debug::Log::write (
                juce::String ("READER EOF: drains=") + juce::String (totalDrains)
                + " chunks=" + juce::String (totalChunks)
                + " bytes=" + juce::String (totalBytes)
                + " drainMs=" + juce::String (totalDrainMs, 1)
                + " pollMs=" + juce::String (totalPollMs, 1)
                + " processMs=" + juce::String (totalProcessMs, 1));

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
        const auto pollStart { std::chrono::steady_clock::now() };
        const bool dataReady { waitForData (100) };
        const auto pollEnd { std::chrono::steady_clock::now() };
        totalPollMs += std::chrono::duration<double, std::milli> (pollEnd - pollStart).count();

        if (dataReady)
        {
            shellDone = drainPty();
        }
    }
}

void TTY::platformResize (int cols, int rows, int pixelWidth, int pixelHeight)
{
    if (cols != lastResizeCols or rows != lastResizeRows)
    {
        doPlatformResize (cols, rows, pixelWidth, pixelHeight);
        lastResizeCols = cols;
        lastResizeRows = rows;
    }
}

void TTY::rememberDimensions (int cols, int rows) noexcept
{
    lastResizeCols = cols;
    lastResizeRows = rows;
}
