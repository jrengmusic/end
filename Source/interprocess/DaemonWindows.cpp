/**
 * @file DaemonWindows.cpp
 * @brief Windows implementations of Daemon platform helpers.
 *
 * Windows: `hideDockIcon()` is a no-op — no dock icon exists when no window is created.
 *          `spawnDaemon(uuid)` uses `CreateProcessW` with `DETACHED_PROCESS | CREATE_NEW_PROCESS_GROUP`.
 *          `installPlatformProcessCleanup()` creates a Job Object with `JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE`.
 *          `releasePlatformProcessCleanup()` closes the Job Object handle.
 *
 * @note NEXUS PROCESS MESSAGE THREAD — all functions are called from Daemon::start() / Daemon::stop()
 *       or ENDApplication::initialise().
 */

#include "Daemon.h"
#include <windows.h>

#if JUCE_WINDOWS

namespace Interprocess
{
/*____________________________________________________________________________*/

/**
 * @brief No-op on Windows — no dock icon exists when no window is created.
 */
void Daemon::hideDockIcon() noexcept
{
    // No-op on Windows — no dock icon exists when no window is created.
}

/**
 * @brief Spawns a detached `end --nexus <uuid>` process via CreateProcessW.
 *
 * Uses `DETACHED_PROCESS | CREATE_NEW_PROCESS_GROUP` so the child is fully
 * detached from the parent console/window.
 *
 * @param uuid  Instance UUID for the daemon.
 * @return `true` if CreateProcessW succeeded.
 * @note NEXUS PROCESS MESSAGE THREAD.
 */
bool Daemon::spawnDaemon (const juce::String& uuid) noexcept
{
    const auto execPath { juce::File::getSpecialLocation (juce::File::currentExecutableFile).getFullPathName() };
    const juce::String cmdLine { "\"" + execPath + "\" --nexus " + uuid };

    STARTUPINFOW si {};
    si.cb = sizeof (si);
    PROCESS_INFORMATION pi {};

    const BOOL ok { CreateProcessW (
        nullptr,
        const_cast<LPWSTR> (cmdLine.toWideCharPointer()),
        nullptr,
        nullptr,
        FALSE,
        DETACHED_PROCESS | CREATE_NEW_PROCESS_GROUP | CREATE_UNICODE_ENVIRONMENT | CREATE_BREAKAWAY_FROM_JOB,
        nullptr,
        nullptr,
        &si,
        &pi) };

    if (ok != 0)
    {
        CloseHandle (pi.hProcess);
        CloseHandle (pi.hThread);
    }

    return ok != 0;
}

/**
 * @brief Creates a Job Object and assigns the current process to it.
 *
 * The `JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE` flag ensures all child processes
 * are killed when the daemon exits and closes the handle.
 *
 * @note NEXUS PROCESS MESSAGE THREAD.
 */
void Daemon::installPlatformProcessCleanup() noexcept
{
    jobObject = CreateJobObject (nullptr, nullptr);

    if (jobObject != nullptr)
    {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION info {};
        info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE
                                              | JOB_OBJECT_LIMIT_BREAKAWAY_OK;
        SetInformationJobObject (jobObject, JobObjectExtendedLimitInformation, &info, sizeof (info));
        AssignProcessToJobObject (jobObject, GetCurrentProcess());
    }
}

/**
 * @brief Closes the Job Object handle acquired by installPlatformProcessCleanup().
 *
 * @note NEXUS PROCESS MESSAGE THREAD.
 */
void Daemon::releasePlatformProcessCleanup() noexcept
{
    if (jobObject != nullptr)
    {
        CloseHandle (jobObject);
        jobObject = nullptr;
    }
}

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Interprocess

#endif // JUCE_WINDOWS
