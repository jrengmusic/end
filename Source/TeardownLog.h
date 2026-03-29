#pragma once

/**
 * @file TeardownLog.h
 * @brief Temporary crash bisect logger. Appends timestamped lines to a file.
 *        DELETE THIS FILE after debugging.
 */

#include <fstream>
#include <mutex>
#include <chrono>

namespace TeardownLog
{

inline void log (const char* message)
{
    static std::mutex mtx;
    std::lock_guard<std::mutex> lock (mtx);

    std::ofstream file ("C:\\Users\\jreng\\Documents\\Poems\\dev\\end\\teardown.log",
                        std::ios::app);

    if (file.is_open())
    {
        auto now { std::chrono::steady_clock::now().time_since_epoch() };
        auto ms  { std::chrono::duration_cast<std::chrono::milliseconds> (now).count() };
        file << "[" << ms << "] " << message << "\n";
        file.flush();
    }
}

} // namespace TeardownLog

#define TEARDOWN_LOG(msg) TeardownLog::log (msg)
