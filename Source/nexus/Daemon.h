/**
 * @file Daemon.h
 * @brief JUCE-backed TCP server that accepts Nexus client connections.
 *
 * `Nexus::Daemon` wraps `juce::InterprocessConnectionServer` to listen on a
 * TCP port, write the bound port to AppState (which persists it to
 * `~/.config/end/nexus/<uuid>.nexus`), and create a `Nexus::Channel`
 * for each accepted client.
 *
 * Static platform helpers (`hideDockIcon`, `spawnDaemon`) are declared here so
 * callers need only one include.  Implementations live in `Daemon.mm` (macOS /
 * Linux) and `Daemon.cpp` (Windows).
 *
 * @see Nexus::Session
 * @see Nexus::Channel
 */

#pragma once
#include <juce_events/juce_events.h>
#include <jreng_core/jreng_core.h>

namespace Nexus
{
/*____________________________________________________________________________*/

class Session;
class Channel;

/**
 * @class Nexus::Daemon
 * @brief InterprocessConnectionServer that creates Channel objects.
 *
 * Owned by `Nexus::Session`.  `start()` calls `beginWaitingForSocket()`, writes
 * the bound port to AppState (persisted to the nexus state file), and caches
 * it in `activePort`.  `stop()` calls the base `InterprocessConnectionServer::stop()`.
 * Nexus port file deletion is handled by AppState::deleteNexusFile() on quit.
 *
 * @par Thread context
 * `start()` / `stop()` — NEXUS PROCESS MESSAGE THREAD.
 * `createConnectionObject()` — NEXUS PROCESS LISTENER THREAD (called by base class).
 *
 * @see juce::InterprocessConnectionServer
 */
class Daemon : public juce::InterprocessConnectionServer
{
public:
    /** @brief Default port: 0 instructs the OS to assign a free ephemeral port. */
    static constexpr int defaultPort { 0 };

    /**
     * @brief Hides the application from the Dock / taskbar.
     *
     * macOS: sets `NSApplicationActivationPolicyAccessory`.
     * Windows / Linux: no-op.
     *
     * @note NEXUS PROCESS MESSAGE THREAD.  Must be called early in `initialise()`.
     */
    static void hideDockIcon() noexcept;

    /**
     * @brief Spawns a detached `end --nexus <uuid>` daemon process.
     *
     * Reads the current executable path, spawns a child with `--nexus <uuid>` in a new
     * session (POSIX) or as a detached process (Windows), then returns immediately.
     * The child inherits no file descriptors (stdin/stdout/stderr redirected to
     * /dev/null on POSIX, or default DETACHED_PROCESS on Windows).
     *
     * @param uuid  The instance UUID the daemon should use for its lockfile and state file.
     * @return `true` if the OS accepted the spawn call.
     * @note NEXUS PROCESS MESSAGE THREAD.
     */
    static bool spawnDaemon (const juce::String& uuid) noexcept;

    explicit Daemon (Session& host);
    ~Daemon() override;

    /**
     * @brief Starts listening on @p port and writes the bound port to AppState.
     *
     * Tries @p port first.  If @p port is 0, `beginWaitingForSocket` returns the
     * OS-assigned port via `getBoundPort()`.  On success, calls
     * `AppState::getContext()->setPort(activePort)` which persists the port to
     * `~/.config/end/nexus/<uuid>.nexus` so clients can probe it.
     *
     * @param port  Port to try.  0 = let OS choose.
     * @return `true` if `beginWaitingForSocket` succeeded.
     * @note NEXUS PROCESS MESSAGE THREAD.
     */
    bool start (int port = defaultPort);

    /**
     * @brief Stops the daemon.
     * @note NEXUS PROCESS MESSAGE THREAD.
     */
    void stop();

    /**
     * @brief Returns the port currently bound, or 0 if not listening.
     * @note Any thread.
     */
    int getPort() const noexcept;

    /**
     * @brief Removes and destroys the connection owned by this daemon.
     *
     * Called from Channel::connectionLost() on the message thread.
     * Triggers destruction of the Channel.
     *
     * @param connection  Raw pointer to the connection to remove.
     * @note NEXUS PROCESS MESSAGE THREAD.
     */
    void removeConnection (Channel* connection);

private:
    juce::InterprocessConnection* createConnectionObject() override;

    Session& host;
    int activePort { 0 };

    /** @brief Owns all live Channel objects. */
    jreng::Owner<Channel> connections;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Daemon)
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Nexus
