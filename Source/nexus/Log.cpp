/**
 * @file Log.cpp
 * @brief Persistent log implementation for the Nexus connection path.
 *
 * The `juce::FileLogger` singleton is owned here.  `initLog()` constructs it
 * (which truncates the file and writes a header); `logLine()` delegates to
 * `FileLogger::logMessage`, which is internally mutex-guarded and flushes after
 * every write.
 *
 * @see Nexus::Log.h
 */

#include "Log.h"

namespace Nexus
{
/*____________________________________________________________________________*/

namespace
{
    /** @brief Singleton FileLogger — initialised by `initLog()`, never reset. */
    std::unique_ptr<juce::FileLogger> sLogger;
} // namespace

// =============================================================================

/**
 * @brief Returns `~/.config/end/<filename>`.
 *
 * @param filename  Filename relative to `~/.config/end/`.
 */
juce::File getLogFile (const juce::String& filename)
{
    return juce::File::getSpecialLocation (juce::File::userHomeDirectory)
               .getChildFile (".config/end/" + filename);
}

/**
 * @brief Truncates the named log file and writes a start header.
 *
 * Constructs `sLogger` with `fileSizeLimit = 0` so JUCE does not rotate the
 * file.  `juce::FileLogger`'s constructor creates the file (or truncates it if
 * it already exists) and writes the welcome message as the first line.
 *
 * Client process passes `"end-nexus-client.log"`.
 * Daemon process passes `"end-nexus-daemon.log"`.
 *
 * @param filename  Filename relative to `~/.config/end/`.
 *
 * @note NEXUS PROCESS MAIN THREAD.
 */
void initLog (const juce::String& filename)
{
    const juce::File logFile { getLogFile (filename) };

    // Truncate by deleting any existing file before constructing FileLogger.
    // FileLogger opens in append mode; deletion + construction gives truncate semantics.
    if (logFile.existsAsFile())
        logFile.deleteFile();

    const juce::String timestamp { juce::Time::getCurrentTime().formatted ("%Y-%m-%d %H:%M:%S") };
    const juce::String welcomeMessage { "=== start === " + timestamp };

    // maxInitialFileSizeBytes = 0 disables rotation.
    sLogger = std::make_unique<juce::FileLogger> (logFile, welcomeMessage, 0);
}

/**
 * @brief Appends `[HH:MM:SS.mmm] message` to the nexus log.
 *
 * No-op when `sLogger` is null (i.e., `initLog()` was not called).
 * `juce::FileLogger::logMessage` flushes after every write.
 *
 * @note NEXUS PROCESS — any thread.
 */
void logLine (const juce::String& message)
{
    if (sLogger != nullptr)
    {
        const juce::Time now { juce::Time::getCurrentTime() };
        const juce::String prefix { "[" + now.formatted ("%H:%M:%S") + "."
                                    + juce::String (now.getMilliseconds()).paddedLeft ('0', 3) + "] " };
        sLogger->logMessage (prefix + message);
    }
}

/**
 * @brief Destroys the FileLogger so JUCE's leak detector does not report it.
 *
 * Resets `sLogger` to null.  After this call `logLine` is a no-op.
 * ENDApplication calls this at the end of its `shutdown()` override, inside
 * JUCE's controlled teardown sequence, before the leak detector runs.
 *
 * @note NEXUS PROCESS MAIN THREAD.
 */
void shutdownLog()
{
    sLogger = nullptr;
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Nexus
