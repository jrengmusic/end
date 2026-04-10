/**
 * @file Session.h
 * @brief Unified session pool — owns Terminal::Session objects and routes input
 *        across local, daemon, and client modes.
 *
 * `Nexus::Session` is the single owner and router for terminal sessions in all
 * three process modes:
 *
 * - **Local** (`Session()`) — no IPC.  Owns `Terminal::Session` objects (each of
 *   which owns its `Terminal::Processor` internally).  PTY pipeline wired internally
 *   by Terminal::Session.
 * - **Daemon** (`Session(DaemonTag)`) — headless IPC server.  Owns `Terminal::Session`
 *   objects.  Nexus wires `onBytes` to broadcast `Message::output` to attached subscribers
 *   and `onStateFlush` to broadcast `Message::stateUpdate`.
 * - **Client** (`Session(ClientTag)`) — IPC client.  Owns `Terminal::Session` objects
 *   (remote constructor — no TTY) in `terminalSessions`.  Receives `Message::output` /
 *   `Message::loading` from daemon via Grid+State snapshot; `startLoading` calls
 *   `setStateInformation` directly.
 *
 * `openTerminal` is the single entry point for session creation across all modes.
 *
 * ### Byte-forward flow
 * ```
 * Daemon:  PTY → Terminal::Session::onBytes → Message::output → Client
 * Client:  Message::output received → Processor::process → Grid → Display
 * Local:   PTY → Terminal::Session::onBytes → Processor::process → Grid → Display
 * ```
 *
 * ### Context
 * Session extends `jreng::Context<Session>` so any subsystem can reach it via
 * `Nexus::Session::getContext()` without a singleton pattern.  The single
 * instance is owned as a value member of `ENDApplication`.
 *
 * @note Session-management methods are **NEXUS PROCESS MESSAGE THREAD** only.
 *
 * @see Terminal::Session  — PTY side (TTY + History).
 * @see Terminal::Processor — pipeline side (Parser + Grid + Display).
 * @see Nexus::Channel
 * @see jreng::Context
 */

#pragma once
#include <juce_data_structures/juce_data_structures.h>
#include <jreng_core/jreng_core.h>
#include <map>
#include <vector>
#include <memory>

#include "../terminal/logic/Processor.h"

namespace Terminal { class Session; }

namespace Nexus
{
/*____________________________________________________________________________*/

class Link;
class Daemon;
class Channel;

/**
 * @class Nexus::Session
 * @brief Unified session pool — local, daemon, and client modes behind one API.
 *
 * Constructed once by `ENDApplication` before the main window.  Destroyed
 * after the main window so that all Display objects are torn down before
 * sessions are destroyed.
 *
 * @par Thread context
 * Session methods: **NEXUS PROCESS MESSAGE THREAD**.
 * Daemon methods: **NEXUS PROCESS MESSAGE THREAD** (startServer / stopServer / isServing).
 * `attach()` / `detach()` — any thread (lock guarded).
 * `attach(uuid, target, sendHistory)` / `detachConnection()` — any thread (lock guarded).
 *
 * @see Terminal::Session
 * @see Terminal::Processor
 * @see Nexus::Channel
 */
class Session : public jreng::Context<Session>
{
public:
    /**
     * @brief Disambiguation tag for daemon-mode constructor.
     *
     * Pass `Session::DaemonTag{}` to construct Session as a headless IPC server.
     */
    struct DaemonTag {};

    /**
     * @brief Disambiguation tag for client-mode constructor.
     *
     * Pass `Session::ClientTag{}` to construct Session as an IPC client.
     */
    struct ClientTag {};

    /** @brief Constructs Session in local mode — no IPC. */
    Session();

    /**
     * @brief Constructs Session in daemon mode — constructs Daemon internally and calls startServer().
     * @note NEXUS PROCESS MESSAGE THREAD.
     */
    explicit Session (DaemonTag);

    /**
     * @brief Constructs Session in client mode — constructs Link internally and calls beginConnectAttempts().
     * @note NEXUS PROCESS MESSAGE THREAD.
     */
    explicit Session (ClientTag);

    ~Session() override;

    /**
     * @brief Opens a terminal session with an explicit UUID.
     *
     * Single entry point for all session creation across all three modes.
     * All three modes construct a `Terminal::Session` stored in `terminalSessions`
     * and return a reference to its Processor.
     * Local/daemon mode: full session with TTY via Terminal::Session::create (resolves shell/args from config).
     * Client mode: remote session (no TTY); reads shell from Config locally; sends minimal PDU to daemon.
     * Returns the existing Processor immediately if @p uuid already exists (idempotency).
     *
     * @param cwd   Initial working directory.  Empty = inherit.
     * @param uuid  Explicit UUID to assign.  Must be non-empty.
     * @param cols  Initial column count.  Must be > 0.
     * @param rows  Initial row count.  Must be > 0.
     * @return Mutable reference to the newly constructed Processor.
     * @note NEXUS PROCESS MESSAGE THREAD.
     */
    Terminal::Processor& openTerminal (const juce::String& cwd,
                                        const juce::String& uuid,
                                        int cols,
                                        int rows);

    /**
     * @brief Returns a mutable reference to the Processor with the given UUID.
     *
     * jasserts if no Processor with @p uuid exists.
     *
     * @param uuid  The UUID returned by `openTerminal()`.
     * @return Mutable reference to the owned Processor.
     * @note NEXUS PROCESS MESSAGE THREAD.
     */
    Terminal::Processor& get (const juce::String& uuid);

    /**
     * @brief Returns a const reference to the Processor with the given UUID.
     *
     * jasserts if no Processor with @p uuid exists.
     *
     * @param uuid  The UUID returned by `openTerminal()`.
     * @return Const reference to the owned Processor.
     * @note NEXUS PROCESS MESSAGE THREAD.
     */
    const Terminal::Processor& get (const juce::String& uuid) const;

    /**
     * @brief Removes and destroys the session with the given UUID.
     *
     * @param uuid  The UUID of the session to remove.
     * @note NEXUS PROCESS MESSAGE THREAD.
     */
    void remove (const juce::String& uuid);

    /**
     * @brief Returns true if a session with @p uuid is live on this daemon.
     *
     * @param uuid  UUID to test.
     * @return true if the uuid is found in terminalSessions.
     * @note NEXUS PROCESS MESSAGE THREAD.
     */
    bool hasSession (const juce::String& uuid) const noexcept;

    /**
     * @brief Returns a snapshot of all live session UUIDs.
     *
     * @return StringArray of UUID strings for all currently live sessions.
     * @note NEXUS PROCESS MESSAGE THREAD.
     */
    juce::StringArray list() const;

    /**
     * @brief Forwards raw input bytes to the target session's PTY.
     *
     * Local/daemon mode: looks up the `Terminal::Session` by @p uuid and calls
     * `Terminal::Session::sendInput`.
     * Client mode: calls `Link::sendInput` to forward bytes over IPC to the daemon.
     *
     * @param uuid  UUID of the target session.
     * @param data  Pointer to the raw byte buffer.
     * @param size  Number of bytes.
     * @note MESSAGE THREAD.
     */
    void sendInput (const juce::String& uuid, const void* data, int size);

    /**
     * @brief Forwards a terminal resize to the target session's PTY.
     *
     * Local/daemon mode: looks up `Terminal::Session` by @p uuid and calls resize.
     * Client mode: calls `Link::sendResize` to forward the resize over IPC.
     *
     * @param uuid  UUID of the target session.
     * @param cols  New column count.
     * @param rows  New row count.
     * @note MESSAGE THREAD.
     */
    void sendResize (const juce::String& uuid, int cols, int rows);

    /**
     * @brief Routes incoming bytes from daemon to the Processor for @p uuid.
     *
     * Client mode: called by Link when `Message::output` arrives.
     * Looks up the Terminal::Session in terminalSessions by UUID and feeds bytes
     * through `Processor::process`.
     *
     * @param uuid   UUID of the target session.
     * @param data   Raw byte buffer.
     * @param size   Number of bytes.
     * @note MESSAGE THREAD (called from Link::messageReceived).
     */
    void feedBytes (const juce::String& uuid, const void* data, int size);

    /**
     * @brief Restores Processor state from a snapshot sent by the daemon on attach.
     *
     * Called from Link::messageReceived when `Message::loading` arrives.
     * Calls setStateInformation directly on the message thread.
     *
     * @param uuid   UUID of the target Processor.
     * @param bytes  Snapshot bytes (Grid+State) produced by daemon Processor::getStateInformation.
     * @note MESSAGE THREAD.
     */
    void startLoading (const juce::String& uuid, juce::MemoryBlock&& bytes);

    // =========================================================================
    /** @name Daemon
     *  Implemented in Session.cpp.
     * @{ */

    /**
     * @brief Creates the Daemon and starts listening.
     * @note NEXUS PROCESS MESSAGE THREAD.
     */
    void startServer();

    /**
     * @brief Stops the daemon and removes the lockfile.
     * @note NEXUS PROCESS MESSAGE THREAD.
     */
    void stopServer();

    /**
     * @brief Returns true if the daemon is active.
     * @note Any thread.
     */
    bool isServing() const noexcept;

    /** @} */

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
     * @brief Sends a `Message::sessions` PDU to @p target only.
     *
     * Wire format: uint16_t count | N × (uint32_t len + UTF-8 bytes).
     * Called from Channel::connectionMade().
     *
     * @param target  The newly connected Channel to send to.
     * @note NEXUS PROCESS MESSAGE THREAD.
     */
    void broadcastSessions (Channel& target);

    /**
     * @brief Sends a `Message::sessions` PDU to every attached connection.
     *
     * Called after processor creation or exit so all connected clients re-sync.
     *
     * @note Acquires connectionsLock.  NEXUS PROCESS MESSAGE THREAD.
     */
    void broadcastSessions();

    /**
     * @brief Removes @p connection from the broadcast list AND all per-processor
     *        subscriber lists.
     *
     * @param connection  The disconnecting Channel.
     * @note Acquires connectionsLock.  Any thread.
     */
    void detach (Channel& connection);

    /** @} */


    /**
     * @brief Calls @p visitor with each currently-attached Channel.
     *
     * @param visitor  Callable with signature `void(Channel&)`.
     * @note NEXUS PROCESS MESSAGE THREAD.
     */
    template <typename Visitor>
    void forEachAttached (Visitor&& visitor)
    {
        const juce::ScopedLock lock (connectionsLock);

        for (auto* conn : attached)
            visitor (*conn);
    }

    // =========================================================================
    /**
     * @brief Called when the last session exits and no window is visible.
     *
     * Set by ENDApplication in headless daemon mode.
     */
    std::function<void()> onAllSessionsExited;

    // =========================================================================
    /** @name Byte-output subscriber registry
     *  Implemented in SessionFanout.cpp.
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
    void attach (const juce::String& uuid, Channel& target,
                 bool sendHistory, int cols, int rows);

    /**
     * @brief Unregisters @p connection as a byte-output subscriber for @p uuid.
     *
     * @param uuid        UUID of the session to unsubscribe from.
     * @param connection  The Channel to unregister.
     * @note Acquires connectionsLock.  Any thread.
     */
    void detachConnection (const juce::String& uuid, Channel& connection);

    /** @} */

private:
    /**
     * @brief Owned Terminal::Session map: UUID → unique_ptr<Terminal::Session>.
     *
     * Present in all three modes.  Each Terminal::Session owns its Processor.
     * Local/daemon: full sessions with TTY.
     * Client: remote sessions (no TTY) constructed via Terminal::Session remote constructor.
     */
    std::map<juce::String, std::unique_ptr<Terminal::Session>> terminalSessions;

    /**
     * @brief Per-session subscriber registry: UUID → list of raw Channel pointers.
     *
     * Raw pointers are safe: Channel ownership lives in
     * Daemon::connections (jreng::Owner).  Entries are registered via attach()
     * and removed via detachConnection() or Session::detach() before the
     * Channel is destroyed.
     *
     * Guarded by connectionsLock.
     */
    std::map<juce::String, std::vector<Channel*>> subscribers;

    /** @brief JUCE-backed TCP listener.  Non-null in daemon mode after construction. */
    std::unique_ptr<Daemon> daemon;

    /**
     * @brief IPC client connector.  Non-null in client mode after construction.
     *
     * Private — no external caller sees or touches Link directly.
     */
    std::unique_ptr<Link> link;

    /**
     * @brief Non-owning broadcast list of active connections.
     *
     * Ownership lives in `Nexus::Daemon::connections` (jreng::Owner).
     * Every connected client receives session-lifecycle messages.
     *
     * Guarded by connectionsLock.
     */
    std::vector<Channel*> attached;

    /** @brief Guards `attached` and `subscribers` for cross-thread access. */
    juce::CriticalSection connectionsLock;

    /**
     * @brief Fires onAllSessionsExited if the terminalSessions map is now empty.
     *
     * Called at every exit path after the departing entry has been erased from
     * `terminalSessions`.  Client mode never sets onAllSessionsExited.
     *
     * @note NEXUS PROCESS MESSAGE THREAD.
     */
    void fireIfAllExited() noexcept;

    /**
     * @brief Builds the `Message::sessions` payload from the current terminalSessions map.
     *
     * Wire format: uint16_t count | N × (uint32_t len + UTF-8 bytes).
     *
     * @note NEXUS PROCESS MESSAGE THREAD.
     */
    juce::MemoryBlock buildSessionsPayload() const;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Session)
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Nexus
