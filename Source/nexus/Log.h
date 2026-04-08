/**
 * @file Log.h
 * @brief Persistent log for the Nexus connection path.
 *
 * Provides two free functions:
 *
 * - `Nexus::initLog(filename)` — called once at process startup; truncates the
 *   named log file and writes a timestamped "=== start ===" header.  The
 *   filename is relative to `~/.config/end/`.
 *
 *   Client process calls: `initLog("end-nexus-client.log")`
 *   Daemon process calls: `initLog("end-nexus-daemon.log")`
 *
 * - `Nexus::logLine(message)` — appends one line with a `[HH:MM:SS.mmm]` prefix.
 *   Thread-safe: `juce::FileLogger::logMessage` uses an internal mutex.
 *   No-op when `initLog` has not been called.
 *
 * @note NEXUS PROCESS — callable from any thread.
 */

#pragma once
#include <juce_core/juce_core.h>

namespace Nexus
{
/*____________________________________________________________________________*/

/**
 * @brief Returns the path to a nexus log file by filename.
 *
 * @param filename  Filename relative to `~/.config/end/` (e.g. `"end-nexus-daemon.log"`).
 */
juce::File getLogFile (const juce::String& filename);

/**
 * @brief Truncates the named log file and writes a timestamped start header.
 *
 * Must be called once per process before any `logLine` call.  Client and
 * daemon each pass a distinct filename so their logs do not collide.
 *
 * @param filename  Filename relative to `~/.config/end/`.
 *
 * @note NEXUS PROCESS MAIN THREAD.
 */
void initLog (const juce::String& filename);

/**
 * @brief Appends `[HH:MM:SS.mmm] message\n` to the nexus log file.
 *
 * No-op if `initLog()` has not been called.  Thread-safe via
 * `juce::FileLogger::logMessage`.
 *
 * @note NEXUS PROCESS — any thread.
 */
void logLine (const juce::String& message);

/**
 * @brief Destroys the FileLogger and flushes the log file.
 *
 * Must be called during JUCE-controlled shutdown — before the JUCE leak
 * detector runs — so the FileLogger is not reported as a leak.  After this
 * call, `logLine` is a no-op.
 *
 * @note NEXUS PROCESS MAIN THREAD.
 */
void shutdownLog();

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Nexus
