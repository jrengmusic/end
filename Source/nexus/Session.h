/**
 * @file Session.h
 * @brief Unified session pool — owns Terminal::Session (PTY side) and
 *        Terminal::Processor (pipeline side) objects and routes input across
 *        local, daemon, and client modes.
 *
 * `Nexus::Session` is the single owner and router for terminal sessions in all
 * three process modes:
 *
 * - **Local** (`Session()`) — no IPC.  Owns both `Terminal::Session` (TTY+History)
 *   and `Terminal::Processor` (pipeline).  Wires onBytes → Processor::process.
 * - **Daemon** (`Session(DaemonTag)`) — headless IPC server.  Owns only
 *   `Terminal::Session` objects.  Wires onBytes → broadcast `Message::output` to
 *   attached subscribers.
 * - **Client** (`Session(ClientTag)`) — IPC client.  Owns only
 *   `Terminal::Processor` objects.  Receives `Message::output` / `Message::loading`
 *   from daemon via `Nexus::Loader`.
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
 * @see Nexus::ServerConnection
 * @see jreng::Context
 */

#pragma once
#include <juce_data_structures/juce_data_structures.h>
#include <jreng_core/jreng_core.h>
#include <map>
#include <vector>
#include <memory>

#include "../terminal/logic/Processor.h"
#include "Loader.h"

namespace Terminal { class Session; }

namespace Nexus
{
/*____________________________________________________________________________*/

/** ID value used for the LOADING operation that represents
 *  'waiting for daemon's initial processor list'. */
inline constexpr const char* nexusConnectOperationId { "nexus-connect" };

class Client;
class Server;
class ServerConnection;

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
 * Server methods: **NEXUS PROCESS MESSAGE THREAD** (startServer / stopServer / isServing).
 * `attach()` / `detach()` — any thread (lock guarded).
 * `attachConnection()` / `detachConnection()` — any thread (lock guarded).
 *
 * @see Terminal::Session
 * @see Terminal::Processor
 * @see Nexus::ServerConnection
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
     * @brief Constructs Session in daemon mode — constructs Server internally and calls startServer().
     * @note NEXUS PROCESS MESSAGE THREAD.
     */
    explicit Session (DaemonTag);

    /**
     * @brief Constructs Session in client mode — constructs Client internally and calls beginConnectAttempts().
     * @note NEXUS PROCESS MESSAGE THREAD.
     */
    explicit Session (ClientTag);

    ~Session() override;

    /**
     * @brief Creates a new terminal session with a freshly-generated UUID.
     *
     * In local mode: constructs both a `Terminal::Session` (PTY side) and a
     * `Terminal::Processor` (pipeline side), wires onBytes, returns Processor&.
     *
     * In daemon mode: constructs a `Terminal::Session` (PTY side only), wires
     * onBytes to broadcast `Message::output` to attached subscribers, returns a
     * stub Processor& (daemon has no display, but callers may inspect the uuid).
     *
     * In client mode: constructs a `Terminal::Processor` (pipeline side only),
     * sends `Message::spawnProcessor` to daemon, returns Processor&.
     *
     * @param shell   Shell program override.  Empty = use Config default.
     * @param args    Shell arguments override.  Empty = use Config default.
     * @param cwd     Initial working directory.  Empty = inherit.
     * @param cols    Initial column count.  Must be > 0.
     * @param rows    Initial row count.  Must be > 0.
     * @param envID   UUID of the parent session whose live PATH seeds the new shell.
     *                Empty = no seed env.  Ignored on Windows.
     * @return Mutable reference to the newly constructed Processor.
     * @note NEXUS PROCESS MESSAGE THREAD.
     */
    Terminal::Processor& create (const juce::String& shell,
                                  const juce::String& args,
                                  const juce::String& cwd,
                                  int cols,
                                  int rows,
                                  const juce::String& envID = {});

    /**
     * @brief Creates a terminal session with an explicit UUID.
     *
     * Used for state restoration and for the daemon-side spawnProcessor handler
     * which receives the client-assigned UUID over the wire.
     *
     * @param shell   Shell program override.  Empty = use Config default.
     * @param args    Shell arguments override.  Empty = use Config default.
     * @param cwd     Initial working directory.  Empty = inherit.
     * @param uuid    Explicit UUID to assign.  Must be non-empty.
     * @param cols    Initial column count.  Must be > 0.
     * @param rows    Initial row count.  Must be > 0.
     * @param envID   UUID of the parent session whose live PATH seeds the new shell.
     *                Empty = no seed env.  Ignored on Windows.
     * @return Mutable reference to the newly constructed Processor.
     * @note NEXUS PROCESS MESSAGE THREAD.
     */
    Terminal::Processor& create (const juce::String& shell,
                                  const juce::String& args,
                                  const juce::String& cwd,
                                  const juce::String& uuid,
                                  int cols,
                                  int rows,
                                  const juce::String& envID = {});

    /**
     * @brief Returns a mutable reference to the Processor with the given UUID.
     *
     * jasserts if no Processor with @p uuid exists.
     *
     * @param uuid  The UUID returned by `create()`.
     * @return Mutable reference to the owned Processor.
     * @note NEXUS PROCESS MESSAGE THREAD.
     */
    Terminal::Processor& get (const juce::String& uuid);

    /**
     * @brief Returns a const reference to the Processor with the given UUID.
     *
     * jasserts if no Processor with @p uuid exists.
     *
     * @param uuid  The UUID returned by `create()`.
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
     * Client mode: calls `Client::sendInput` to forward bytes over IPC to the daemon.
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
     * Client mode: calls `Client::sendResize` to forward the resize over IPC.
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
     * Client mode: called by Client when `Message::output` arrives.
     * Looks up the Processor by UUID and feeds bytes through `Processor::process`.
     * `Message::loading` bytes are handled by `Nexus::Loader` via `startLoading`.
     *
     * @param uuid   UUID of the target Processor.
     * @param data   Raw byte buffer.
     * @param size   Number of bytes.
     * @note MESSAGE THREAD (called from Client::messageReceived).
     */
    void feedBytes (const juce::String& uuid, const void* data, int size);

    /**
     * @brief Constructs a `Nexus::Loader` to parse @p bytes into the Processor for @p uuid.
     *
     * Called from Client::messageReceived when `Message::loading` arrives.
     * Appends an OPERATION child to the AppState LOADING node, then starts the Loader thread.
     * When the Loader finishes it erases itself from `loaders` and removes the OPERATION child.
     *
     * @param uuid   UUID of the target Processor.
     * @param bytes  Backlog byte buffer (moved into Loader ownership).
     * @note MESSAGE THREAD.
     */
    void startLoading (const juce::String& uuid, juce::MemoryBlock&& bytes);

    // =========================================================================
    /** @name Server
     *  Implemented in Session.cpp.
     * @{ */

    /**
     * @brief Creates the Server and starts listening.
     * @note NEXUS PROCESS MESSAGE THREAD.
     */
    void startServer();

    /**
     * @brief Stops the server and removes the lockfile.
     * @note NEXUS PROCESS MESSAGE THREAD.
     */
    void stopServer();

    /**
     * @brief Returns true if the server is active.
     * @note Any thread.
     */
    bool isServing() const noexcept;

    /** @} */

    // =========================================================================
    /** @name Broadcast registry
     *  Called by ServerConnection on the message thread.
     * @{ */

    /**
     * @brief Adds @p connection to the broadcast list.
     *
     * Called from ServerConnection::connectionMade().
     *
     * @param connection  The newly connected ServerConnection.
     * @note Acquires connectionsLock.  Any thread.
     */
    void attach (ServerConnection& connection);

    /**
     * @brief Sends a `Message::processorList` PDU to @p target only.
     *
     * Wire format: uint16_t count | N × (uint32_t len + UTF-8 bytes).
     * Called from ServerConnection::connectionMade().
     *
     * @param target  The newly connected ServerConnection to send to.
     * @note NEXUS PROCESS MESSAGE THREAD.
     */
    void broadcastProcessorList (ServerConnection& target);

    /**
     * @brief Sends a `Message::processorList` PDU to every attached connection.
     *
     * Called after processor creation or exit so all connected clients re-sync.
     *
     * @note Acquires connectionsLock.  NEXUS PROCESS MESSAGE THREAD.
     */
    void broadcastProcessorList();

    /**
     * @brief Removes @p connection from the broadcast list AND all per-processor
     *        subscriber lists.
     *
     * @param connection  The disconnecting ServerConnection.
     * @note Acquires connectionsLock.  Any thread.
     */
    void detach (ServerConnection& connection);

    /** @} */

    // =========================================================================
    /**
     * @brief Calls @p visitor with each live Processor, holding no lock.
     *
     * @param visitor  Callable with signature `void(Terminal::Processor&)`.
     * @note NEXUS PROCESS MESSAGE THREAD.
     */
    template <typename Visitor>
    void forEachProcessor (Visitor&& visitor)
    {
        for (auto& [uuid, ptr] : processors)
        {
            juce::ignoreUnused (uuid);
            visitor (*ptr);
        }
    }

    /**
     * @brief Calls @p visitor with each currently-attached ServerConnection.
     *
     * @param visitor  Callable with signature `void(ServerConnection&)`.
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
     * @brief Atomically sends loading snapshot then registers the connection as a subscriber.
     *
     * Holds `connectionsLock` across both operations so the reader thread's
     * onBytes broadcast cannot race between them.  Guarantees the target
     * receives history BEFORE any Message::output for the same uuid.
     *
     * @param uuid    Session UUID to sync.
     * @param target  Connection to register and send to.
     * @note NEXUS PROCESS MESSAGE THREAD.
     */
    void attachAndSync (const juce::String& uuid, ServerConnection& target);

    /**
     * @brief Registers @p connection as a byte-output subscriber for @p uuid.
     *
     * Called from ServerConnection when `Message::attachProcessor` is received.
     * Daemon sends `Message::loading` snapshot, then wires the Terminal::Session
     * onBytes to also broadcast to this connection via `Message::output`.
     *
     * @param uuid        UUID of the session to subscribe to.
     * @param connection  The ServerConnection to register.
     * @note Acquires connectionsLock.  Any thread.
     */
    void attachConnection (const juce::String& uuid, ServerConnection& connection);

    /**
     * @brief Unregisters @p connection as a byte-output subscriber for @p uuid.
     *
     * @param uuid        UUID of the session to unsubscribe from.
     * @param connection  The ServerConnection to unregister.
     * @note Acquires connectionsLock.  Any thread.
     */
    void detachConnection (const juce::String& uuid, ServerConnection& connection);

    /** @} */

private:
    /**
     * @brief Active Loader map: UUID → unique_ptr<Loader>.
     *
     * Non-empty only in client mode while a backlog is being replayed off the
     * message thread.  Each entry is erased from the Loader's own onFinished
     * callback, which fires on the message thread after `run()` completes.
     */
    std::map<juce::String, std::unique_ptr<Loader>> loaders;

    /**
     * @brief Owned Processor map: UUID → unique_ptr<Processor>.
     *
     * Present in local mode (both sides) and client mode (pipeline side only).
     * Empty in daemon mode (daemon has no display pipeline).
     */
    std::map<juce::String, std::unique_ptr<Terminal::Processor>> processors;

    /**
     * @brief Owned Terminal::Session map: UUID → unique_ptr<Terminal::Session>.
     *
     * Present in local mode (both sides) and daemon mode (PTY side only).
     * Empty in client mode (client has no PTY).
     */
    std::map<juce::String, std::unique_ptr<Terminal::Session>> terminalSessions;

    /**
     * @brief Per-session subscriber registry: UUID → list of raw ServerConnection pointers.
     *
     * Raw pointers are safe: ServerConnection ownership lives in
     * Server::connections (jreng::Owner).  Entries are registered via attachConnection()
     * and removed via detachConnection() or Session::detach() before the
     * ServerConnection is destroyed.
     *
     * Guarded by connectionsLock.
     */
    std::map<juce::String, std::vector<ServerConnection*>> subscribers;

    /** @brief JUCE-backed TCP listener.  Non-null in daemon mode after construction. */
    std::unique_ptr<Server> server;

    /**
     * @brief IPC client connector.  Non-null in client mode after construction.
     *
     * Private — no external caller sees or touches Client directly.
     */
    std::unique_ptr<Client> client;

    /**
     * @brief Non-owning broadcast list of active connections.
     *
     * Ownership lives in `Nexus::Server::connections` (jreng::Owner).
     * Every connected client receives session-lifecycle messages.
     *
     * Guarded by connectionsLock.
     */
    std::vector<ServerConnection*> attached;

    /** @brief Guards `attached` and `subscribers` for cross-thread access. */
    juce::CriticalSection connectionsLock;

    /**
     * @brief Fires onAllSessionsExited if the processor map is now empty.
     *
     * Called at every exit path (shell-exit and removeProcessor) after the
     * departing entry has already been erased from both `processors` and
     * `terminalSessions`.  `processors` is the SSOT for liveness in local and
     * daemon modes (client mode never sets onAllSessionsExited).
     *
     * @note NEXUS PROCESS MESSAGE THREAD.
     */
    void fireIfAllExited() noexcept;

    /**
     * @brief Creates the client-mode Processor pipeline for @p uuid.
     *
     * Sends spawnProcessor to the daemon if the uuid is not yet live, then
     * sends attachProcessor, constructs and registers a Processor, and returns
     * a reference to it.
     *
     * @note NEXUS PROCESS MESSAGE THREAD.
     */
    Terminal::Processor& createClientSession (const juce::String& shell,
                                              const juce::String& cwd,
                                              const juce::String& uuid,
                                              int cols, int rows,
                                              const juce::String& envID);

    /**
     * @brief Creates the daemon-mode PTY session for @p uuid.
     *
     * Constructs a Terminal::Session, wires onBytes to broadcast Message::output
     * to subscribers and onExit to broadcast Message::processorExited, then
     * constructs a stub Processor for UUID tracking.
     *
     * @note NEXUS PROCESS MESSAGE THREAD.
     */
    Terminal::Processor& createDaemonSession (const juce::String& effectiveShell,
                                              const juce::String& effectiveArgs,
                                              const juce::String& cwd,
                                              const juce::String& uuid,
                                              int cols, int rows,
                                              juce::StringPairArray seedEnv);

    /**
     * @brief Creates the local-mode PTY + Processor pair for @p uuid.
     *
     * Constructs both a Terminal::Session (PTY side) and a Terminal::Processor
     * (pipeline side), wires all callbacks, and returns the Processor reference.
     *
     * @note NEXUS PROCESS MESSAGE THREAD.
     */
    Terminal::Processor& createLocalSession (const juce::String& effectiveShell,
                                             const juce::String& effectiveArgs,
                                             const juce::String& cwd,
                                             const juce::String& uuid,
                                             int cols, int rows,
                                             juce::StringPairArray seedEnv);

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Session)
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Nexus
