/**
 * @file NexusDaemon.h
 * @brief Platform utilities for headless Nexus daemon mode.
 *
 * Provides two platform-specific operations needed when END launches as a
 * headless daemon (`end --nexus`):
 *
 * - `hideDockIcon()` — removes the Dock/taskbar icon so the daemon is invisible
 *   to the user (macOS: NSApplicationActivationPolicyAccessory; no-op elsewhere).
 * - `spawnDaemon()` — launches a detached `end --nexus` child process and
 *   returns immediately.  The child is fully detached (new session, stdio closed
 *   to /dev/null) so the parent UI process is unaffected.
 *
 * @note NEXUS PROCESS MESSAGE THREAD — both functions must be called from the JUCE message thread.
 */

#pragma once

namespace Nexus
{
/*____________________________________________________________________________*/

/**
 * @brief Hides the application from the Dock / taskbar.
 *
 * macOS: sets `NSApplicationActivationPolicyAccessory`.
 * Windows / Linux: no-op.
 *
 * @note NEXUS PROCESS MESSAGE THREAD.  Must be called early in `initialise()`.
 */
void hideDockIcon() noexcept;

/**
 * @brief Spawns a detached `end --nexus` daemon process.
 *
 * Reads the current executable path, spawns a child with `--nexus` in a new
 * session (POSIX) or as a detached process (Windows), then returns immediately.
 * The child inherits no file descriptors (stdin/stdout/stderr redirected to
 * /dev/null on POSIX, or default DETACHED_PROCESS on Windows).
 *
 * @return `true` if the OS accepted the spawn call.
 * @note NEXUS PROCESS MESSAGE THREAD.
 */
bool spawnDaemon() noexcept;

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Nexus
