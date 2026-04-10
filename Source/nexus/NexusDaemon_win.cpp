/**
 * @file NexusDaemon_win.cpp
 * @brief Windows implementation of NexusDaemon platform helpers.
 *
 * `hideDockIcon()` is a no-op on Windows (the taskbar button is managed by the
 * window, which is not created in daemon mode).
 *
 * `spawnDaemon(uuid)` uses `CreateProcessW` with `DETACHED_PROCESS |
 * CREATE_NEW_PROCESS_GROUP` to launch `end --nexus` fully detached from the
 * parent console/window.
 *
 * @note NEXUS PROCESS MESSAGE THREAD — both functions are called from ENDApplication::initialise().
 */

#include "NexusDaemon.h"
#include <JuceHeader.h>

#if JUCE_WINDOWS

#include <windows.h>

namespace Nexus
{
/*____________________________________________________________________________*/

void hideDockIcon() noexcept
{
    // No-op on Windows — no dock icon exists when no window is created.
}

bool spawnDaemon (const juce::String& uuid) noexcept
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
        DETACHED_PROCESS | CREATE_NEW_PROCESS_GROUP | CREATE_UNICODE_ENVIRONMENT,
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

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Nexus

#endif
