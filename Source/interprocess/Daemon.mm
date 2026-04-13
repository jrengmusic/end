/**
 * @file Daemon.mm
 * @brief macOS and Linux implementations of Daemon platform helpers.
 *
 * macOS:  `hideDockIcon()` uses NSApplicationActivationPolicyAccessory.
 *         `spawnDaemon(uuid)` uses `posix_spawn` with `POSIX_SPAWN_SETSID`.
 *
 * Linux:  `hideDockIcon()` is a no-op.
 *         `spawnDaemon(uuid)` uses `posix_spawn` with `POSIX_SPAWN_SETSID`.
 *
 * @note NEXUS PROCESS MESSAGE THREAD — both functions are called from ENDApplication::initialise().
 */

#include "Daemon.h"
#include <JuceHeader.h>

#if JUCE_MAC || JUCE_LINUX

#include <spawn.h>
#include <fcntl.h>
#include <unistd.h>

#if JUCE_MAC
#import <Cocoa/Cocoa.h>
#endif

extern char** environ;

namespace Interprocess
{
/*____________________________________________________________________________*/

void Daemon::hideDockIcon() noexcept
{
#if JUCE_MAC
    [NSApp setActivationPolicy: NSApplicationActivationPolicyAccessory];
#endif
    // No-op on Linux — no single dock API to abstract.
}

bool Daemon::spawnDaemon (const juce::String& uuid) noexcept
{
    const auto execPath { juce::File::getSpecialLocation (juce::File::currentExecutableFile).getFullPathName() };

    posix_spawnattr_t attr;
    posix_spawnattr_init (&attr);
    posix_spawnattr_setflags (&attr, POSIX_SPAWN_SETSID);

    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init (&actions);
    posix_spawn_file_actions_addopen (&actions, STDIN_FILENO,  "/dev/null", O_RDONLY, 0);
    posix_spawn_file_actions_addopen (&actions, STDOUT_FILENO, "/dev/null", O_WRONLY, 0);
    posix_spawn_file_actions_addopen (&actions, STDERR_FILENO, "/dev/null", O_WRONLY, 0);

    const char* argv[] { execPath.toRawUTF8(), "--nexus", uuid.toRawUTF8(), nullptr };

    pid_t pid { 0 };
    const int result { posix_spawn (&pid, argv[0], &actions,
                                    &attr, const_cast<char**> (argv), environ) };

    posix_spawn_file_actions_destroy (&actions);
    posix_spawnattr_destroy (&attr);

    return result == 0;
}

void Daemon::installPlatformProcessCleanup() noexcept { }

void Daemon::releasePlatformProcessCleanup() noexcept { }

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Interprocess

#endif // JUCE_MAC || JUCE_LINUX
