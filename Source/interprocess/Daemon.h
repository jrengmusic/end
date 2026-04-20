/**
 * @file Daemon.h
 * @brief JUCE-backed TCP server that accepts Interprocess client connections.
 *
 * `Interprocess::Daemon` wraps `juce::InterprocessConnectionServer` to listen on a
 * TCP port, write the bound port to AppState (which persists it to
 * `~/.config/end/nexus/<uuid>.nexus`), and create a `Interprocess::Channel`
 * for each accepted client.
 *
 * Daemon also owns the broadcast and per-session subscriber registries that were
 * previously held inline in `Nexus`.  Session callback wiring (onBytes,
 * onStateFlush, onExit) is applied via `wireSessionCallbacks()` after each
 * `Terminal::Session` is created by Nexus in daemon mode.
 *
 * Static platform helpers (`hideDockIcon`, `spawnDaemon`) are declared here so
 * callers need only one include.  Implementations live in `Daemon.mm` (macOS /
 * Linux) and `DaemonWindows.cpp` (Windows).
 *
 * @see Nexus
 * @see Interprocess::Channel
 */

#pragma once
#include <juce_events/juce_events.h>
#include <jam_core/jam_core.h>
#include <vector>
#include <unordered_map>

class Nexus;
namespace Terminal { class Session; }

namespace Interprocess
{
/*____________________________________________________________________________*/

class Channel;

/**
 * @class Interprocess::Daemon
 * @brief InterprocessConnectionServer that creates Channel objects and owns
 *        the broadcast and per-session subscriber registries.
 *
 * Owned by `ENDApplication`.  `start()` calls `beginWaitingForSocket()`, writes
 * the bound port to AppState (persisted to the nexus state file), and caches
 * it in `activePort`.  `stop()` calls the base `InterprocessConnectionServer::stop()`.
 * Nexus port file deletion is handled by AppState::deleteNexusFile() on quit.
 *
 * @par Thread context
 * `start()` / `stop()` — NEXUS PROCESS MESSAGE THREAD.
 * `createConnectionObject()` — NEXUS PROCESS LISTENER THREAD (called by base class).
 * `attach` / `detach` / `broadcastSessions` / subscriber methods — MESSAGE THREAD.
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

    explicit Daemon (Nexus& host);
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

    /**
     * @brief Removes all sessions and triggers daemon shutdown.
     *
     * Iterates all live sessions via `nexus.list()`, calls `nexus.remove()` for each,
     * then fires `onAllSessionsExited` to trigger the existing quit path.
     *
     * @note NEXUS PROCESS MESSAGE THREAD.
     */
    void killAll();

    // =========================================================================
    /** @name Broadcast registry
     *  Called by Channel on the message thread.
     * @{ */

    /**
     * @brief Adds @p connection to the broadcast list.
     *
     * Called from Channel::connectionMade().
     *
     * @param connection  The newly connected Channel.
     * @note Acquires connectionsLock.  Any thread.
     */
    void attach (Channel& connection);

    /**
     * @brief Removes @p connection from the broadcast list AND all per-session
     *        subscriber lists.
     *
     * @param connection  The disconnecting Channel.
     * @note Acquires connectionsLock.  Any thread.
     */
    void detach (Channel& connection);

    /**
     * @brief Sends a `Message::sessions` PDU to @p target only.
     *
     * Wire format: uint16_t count | N × (uint32_t len + UTF-8 bytes).
     * Called from Channel::connectionMade() to deliver the current list
     * to a newly connected client.
     *
     * @param target  The newly connected Channel to send to.
     * @note NEXUS PROCESS MESSAGE THREAD.
     */
    void broadcastSessions (Channel& target);

    /**
     * @brief Sends a `Message::sessions` PDU to every attached connection.
     *
     * Called after session creation or exit so all connected clients re-sync.
     *
     * @note Acquires connectionsLock.  NEXUS PROCESS MESSAGE THREAD.
     */
    void broadcastSessions();

    /** @} */

    // =========================================================================
    /** @name Per-session subscriber registry
     *  Implemented in Daemon.cpp.
     * @{ */

    /**
     * @brief Registers @p target as a byte-output subscriber for @p uuid.
     *
     * When @p sendHistory is true, snapshots the session's byte history and sends
     * it as a Message::loading PDU before registering the subscriber.  The lock
     * is held across both operations so the reader thread's onBytes broadcast
     * cannot interleave between history send and subscriber registration.
     *
     * @param uuid         Session UUID.
     * @param target       Connection to register and send to.
     * @param sendHistory  When true, send history snapshot before subscribing.
     * @param cols         Terminal column count for PTY resize after subscribing.
     * @param rows         Terminal row count for PTY resize after subscribing.
     * @note NEXUS PROCESS MESSAGE THREAD.
     */
    void attachSession (const juce::String& uuid, Channel& target,
                        bool sendHistory, int cols, int rows);

    /**
     * @brief Unregisters @p connection as a byte-output subscriber for @p uuid.
     *
     * @param uuid        UUID of the session to unsubscribe from.
     * @param connection  The Channel to unregister.
     * @note Acquires connectionsLock.  Any thread.
     */
    void detachSession (const juce::String& uuid, Channel& connection);

    /** @} */

    // =========================================================================

    /**
     * @brief Wires daemon-mode IPC callbacks on a newly created Terminal::Session.
     *
     * Replaces the standalone onExit with a daemon-mode one that:
     * - broadcasts `sessionKilled` to all attached connections,
     * - removes the session from Nexus asynchronously,
     * - re-broadcasts the sessions list,
     * - fires onAllSessionsExited if empty.
     *
     * Also wires `onBytes` and `onStateFlush` to broadcast output and state PDUs
     * to per-session subscribers.
     *
     * @param uuid     UUID of the session (used in closures for routing).
     * @param session  The Terminal::Session to wire.
     * @note NEXUS PROCESS MESSAGE THREAD.
     */
    void wireSessionCallbacks (const juce::String& uuid, Terminal::Session& session);

    // =========================================================================
    /**
     * @brief Called when the last session exits.
     *
     * Set by ENDApplication in headless daemon mode to trigger shutdown.
     */
    std::function<void()> onAllSessionsExited;

private:
    juce::InterprocessConnection* createConnectionObject() override;

    /**
     * @brief Builds the `Message::sessions` payload from the current Nexus sessions map.
     *
     * Wire format: uint16_t count | N × (uint32_t len + UTF-8 bytes).
     *
     * @note NEXUS PROCESS MESSAGE THREAD.
     */
    juce::MemoryBlock buildSessionsPayload() const;

    /**
     * @brief Creates and installs a Windows Job Object so child processes are
     *        killed when the daemon process exits.
     *
     * Windows: creates a Job Object with `JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE`
     * and assigns the current process to it.  Stores the handle in `jobObject`.
     * POSIX: no-op.
     *
     * @note NEXUS PROCESS MESSAGE THREAD.
     */
    void installPlatformProcessCleanup() noexcept;

    /**
     * @brief Releases the Job Object handle acquired by installPlatformProcessCleanup().
     *
     * Windows: closes the Job Object handle and resets `jobObject` to nullptr.
     * POSIX: no-op.
     *
     * @note NEXUS PROCESS MESSAGE THREAD.
     */
    void releasePlatformProcessCleanup() noexcept;

    // =========================================================================
    /** @name wireSessionCallbacks helpers
     *  Each wires exactly one callback on @p session.
     *  Called sequentially from wireSessionCallbacks().
     * @{ */

    /**
     * @brief Wires `session.onBytes` to broadcast output PDUs to per-session subscribers.
     *
     * Builds a `Message::output` PDU (uuid prefix + raw bytes) and pushes it to
     * every Channel registered in the subscriber list for @p uuid.  Runs on the
     * reader thread inside the lambda; acquires `connectionsLock`.
     *
     * @param uuid     Session UUID used as the PDU routing key.
     * @param session  Terminal::Session whose `onBytes` callback is being set.
     * @note NEXUS PROCESS MESSAGE THREAD (called at wire time; lambda fires on READER THREAD).
     */
    void wireOnBytes      (const juce::String& uuid, Terminal::Session& session);

    /**
     * @brief Wires `session.onStateFlush` to broadcast stateUpdate PDUs to per-session subscribers.
     *
     * Reads cwd and foreground-process strings from the session Processor state.
     * Broadcasts only when at least one field changed relative to the previous flush
     * (tracked via function-local `shared_ptr` captures to avoid 60 Hz noise).
     *
     * @param uuid     Session UUID used as the PDU routing key.
     * @param session  Terminal::Session whose `onStateFlush` callback is being set.
     * @note NEXUS PROCESS MESSAGE THREAD (called at wire time; lambda fires on MESSAGE THREAD).
     */
    void wireOnStateFlush (const juce::String& uuid, Terminal::Session& session);

    /**
     * @brief Wires `session.onExit` to broadcast sessionKilled and clean up Nexus state.
     *
     * On exit: broadcasts `Message::sessionKilled` to all attached connections, then
     * schedules an async call to remove the session from Nexus, re-broadcast the
     * sessions list, and fire `onAllSessionsExited` if no sessions remain.
     *
     * @param uuid     Session UUID used as the PDU routing key.
     * @param session  Terminal::Session whose `onExit` callback is being set.
     * @note NEXUS PROCESS MESSAGE THREAD (called at wire time; lambda fires on READER THREAD → async MESSAGE THREAD).
     */
    void wireOnExit       (const juce::String& uuid, Terminal::Session& session);

    /** @} */

    Nexus& nexus;
    int activePort { 0 };

#if JUCE_WINDOWS
    void* jobObject { nullptr };
#endif

    /** @brief Owns all live Channel objects. */
    jam::Owner<Channel> connections;

    /**
     * @brief Non-owning broadcast list of active connections.
     *
     * Ownership lives in `connections` (jam::Owner).
     * Every connected client receives session-lifecycle messages.
     *
     * Guarded by connectionsLock.
     */
    std::vector<Channel*> attached;

    /**
     * @brief Per-session subscriber registry: UUID → list of raw Channel pointers.
     *
     * Raw pointers are safe: Channel ownership lives in `connections` (jam::Owner).
     * Entries are registered via attachSession() and removed via detachSession() or
     * detach() before the Channel is destroyed.
     *
     * Guarded by connectionsLock.
     */
    std::unordered_map<juce::String, std::vector<Channel*>> subscribers;

    /** @brief Guards `attached` and `subscribers` for cross-thread access. */
    juce::CriticalSection connectionsLock;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Daemon)
};

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Interprocess
