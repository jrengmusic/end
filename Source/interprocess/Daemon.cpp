/**
 * @file Daemon.cpp
 * @brief Implementation of Interprocess::Daemon — JUCE-backed TCP connection server,
 *        broadcast registry, and per-session subscriber registry.
 *
 * @see Interprocess::Daemon
 * @see Nexus
 * @see Interprocess::Channel
 */

#include "Daemon.h"
#include "../nexus/Nexus.h"
#include "Channel.h"
#include "Message.h"
#include "EncoderDecoder.h"
#include "../AppIdentifier.h"
#include "../AppState.h"
#include "../terminal/logic/Session.h"
#include "../terminal/logic/Processor.h"
#include "../terminal/data/Identifier.h"
#include <algorithm>

namespace Interprocess
{
/*____________________________________________________________________________*/

// =============================================================================

/**
 * @brief Constructs the Daemon with a reference to the owning Nexus instance.
 *
 * @param host_  Owning Nexus — passed through to each Channel and used for
 *               session list queries in buildSessionsPayload.
 */
Daemon::Daemon (Nexus& host_)
    : nexus (host_)
{
}

/**
 * @brief Stops the daemon.
 */
Daemon::~Daemon()
{
    stop();
}

// =============================================================================

/**
 * @brief Starts listening and writes the bound port to AppState.
 *
 * Calls `beginWaitingForSocket (port, "127.0.0.1")`.  After a successful
 * bind, `getBoundPort()` returns the actual port (useful when @p port == 0).
 * On success, calls `AppState::getContext()->setPort(activePort)` which
 * persists the port to `~/.config/end/nexus/<uuid>.nexus`.
 *
 * @param port  Preferred port.  0 = OS-assigned.
 * @return `true` if the daemon started listening successfully.
 * @note NEXUS PROCESS MESSAGE THREAD.
 */
bool Daemon::start (int port)
{
    installPlatformProcessCleanup();

    const bool listening { beginWaitingForSocket (port, "127.0.0.1") };

    if (listening)
    {
        activePort = getBoundPort();

        if (activePort > 0)
            AppState::getContext()->setPort (activePort);
    }

    return listening;
}

/**
 * @brief Stops the daemon.
 *
 * Calls the base `InterprocessConnectionServer::stop()`.  Nexus state file
 * deletion is handled by AppState::deleteNexusFile() on quit.
 *
 * @note NEXUS PROCESS MESSAGE THREAD.
 */
void Daemon::stop()
{
    InterprocessConnectionServer::stop();
    activePort = 0;
    releasePlatformProcessCleanup();

    const juce::ScopedLock lock (connectionsLock);
    attached.clear();
}

/**
 * @brief Returns the currently bound port, or 0 if not listening.
 *
 * @return Active port number.
 * @note Any thread.
 */
int Daemon::getPort() const noexcept
{
    return activePort;
}

// =============================================================================

/**
 * @brief Creates and owns a Channel for each accepted client.
 *
 * Called by the base class listener thread when a new client connects.
 * Ownership is retained in `connections`; the base class receives a non-owning
 * raw pointer as required by the InterprocessConnectionServer contract.
 *
 * @return Raw pointer to the newly created Channel (non-owning).
 * @note NEXUS PROCESS LISTENER THREAD — called by juce::InterprocessConnectionServer::run().
 */
juce::InterprocessConnection* Daemon::createConnectionObject()
{
    return connections.add (std::make_unique<Channel> (*this, nexus)).get();
}

/**
 * @brief Removes and destroys the given Channel.
 *
 * Called from Channel::connectionLost() on the message thread.
 * Locates @p connection by raw pointer in the owner and removes (destroys) it.
 *
 * @param connection  Raw pointer to the connection to destroy.
 * @note NEXUS PROCESS MESSAGE THREAD.
 */
void Daemon::removeConnection (Channel* connection)
{
    const int index { connections.indexOf (connection) };

    if (index >= 0)
    {
        connections.remove (index);
    }
}

// =============================================================================

/**
 * @brief Removes all sessions and triggers daemon shutdown.
 *
 * Snapshots the session list via `nexus.list()` before iterating so that
 * each `nexus.remove()` call does not invalidate the iterator.  After all
 * sessions are removed, fires `onAllSessionsExited` to trigger the existing
 * quit path (deleteNexusFile + quit).
 *
 * @note NEXUS PROCESS MESSAGE THREAD.
 */
void Daemon::killAll()
{
    const juce::StringArray uuids { nexus.list() };

    for (const auto& uuid : uuids)
        nexus.remove (uuid);

    if (onAllSessionsExited != nullptr)
        onAllSessionsExited();
}

// =============================================================================
// Broadcast registry

/**
 * @brief Adds @p connection to the broadcast list.
 *
 * @note Acquires connectionsLock.  Any thread.
 */
void Daemon::attach (Channel& connection)
{
    const juce::ScopedLock lock (connectionsLock);
    attached.push_back (&connection);
}

/**
 * @brief Removes @p connection from the broadcast list AND every per-session
 *        subscriber list in one locked pass.
 *
 * @note Acquires connectionsLock.  Any thread.
 */
void Daemon::detach (Channel& connection)
{
    const juce::ScopedLock lock (connectionsLock);

    attached.erase (std::remove (attached.begin(), attached.end(), &connection), attached.end());

    for (auto it { subscribers.begin() }; it != subscribers.end();)
    {
        auto& list { it->second };
        list.erase (std::remove (list.begin(), list.end(), &connection), list.end());

        if (list.empty())
            it = subscribers.erase (it);
        else
            ++it;
    }
}

/**
 * @brief Builds the `Message::sessions` payload from the current Nexus sessions map.
 *
 * Wire format: uint16_t count | N × (uint32_t len + UTF-8 bytes).
 *
 * @note NEXUS PROCESS MESSAGE THREAD.
 */
juce::MemoryBlock Daemon::buildSessionsPayload() const
{
    const juce::StringArray uuids { nexus.list() };
    juce::MemoryBlock payload;
    writeUint16 (payload, static_cast<uint16_t> (uuids.size()));

    for (const auto& uuid : uuids)
        writeString (payload, uuid);

    return payload;
}

/**
 * @brief Sends a `Message::sessions` PDU to @p target only.
 *
 * Called from Channel::connectionMade() to deliver the current list
 * to a newly connected client.
 *
 * @note NEXUS PROCESS MESSAGE THREAD.
 */
void Daemon::broadcastSessions (Channel& target)
{
    target.sendPdu (Message::sessions, buildSessionsPayload());
}

/**
 * @brief Sends a `Message::sessions` PDU to every attached connection.
 *
 * Called after session creation or exit so all connected clients re-sync.
 *
 * @note Acquires connectionsLock.  NEXUS PROCESS MESSAGE THREAD.
 */
void Daemon::broadcastSessions()
{
    const juce::MemoryBlock payload { buildSessionsPayload() };
    const juce::ScopedLock lock (connectionsLock);

    for (auto* conn : attached)
        conn->sendPdu (Message::sessions, payload);
}

// =============================================================================
// Per-session subscriber registry

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
void Daemon::attachSession (const juce::String& uuid, Channel& target,
                             bool sendHistory, int cols, int rows)
{
    {
        const juce::ScopedLock lock (connectionsLock);

        if (sendHistory and nexus.has (uuid))
        {
            juce::MemoryBlock snapshot;
            nexus.get (uuid).getStateInformation (snapshot);

            juce::MemoryBlock payload;
            writeString (payload, uuid);
            payload.append (snapshot.getData(), snapshot.getSize());

            target.sendPdu (Message::loading, payload);
        }

        subscribers[uuid].push_back (&target); // operator[] intentional — inserts empty list for new uuid
    }

    // Resize after lock release — SIGWINCH output broadcast acquires connectionsLock.
    if (nexus.has (uuid))
        nexus.get (uuid).resize (cols, rows);
}

/**
 * @brief Unregisters @p connection from the subscriber list for @p uuid.
 *
 * @note Acquires connectionsLock.
 */
void Daemon::detachSession (const juce::String& uuid, Channel& connection)
{
    const juce::ScopedLock lock (connectionsLock);
    auto it { subscribers.find (uuid) };

    if (it != subscribers.end())
    {
        auto& list { it->second };
        list.erase (std::remove (list.begin(), list.end(), &connection), list.end());

        if (list.empty())
            subscribers.erase (it);
    }
}

// =============================================================================

/**
 * @brief Wires daemon-mode IPC callbacks on a newly created Terminal::Session.
 *
 * Delegates to three helpers, each wiring exactly one callback:
 * - wireOnBytes      → `session.onBytes`
 * - wireOnStateFlush → `session.onStateFlush`
 * - wireOnExit       → `session.onExit`
 *
 * Called by Nexus::create (TTY overload) when an Interprocess::Daemon is attached,
 * immediately after the standalone onExit is wired.  Replaces the standalone onExit.
 *
 * @param uuid     UUID of the session (used in closures for routing).
 * @param session  The Terminal::Session to wire.
 * @note NEXUS PROCESS MESSAGE THREAD.
 */
void Daemon::wireSessionCallbacks (const juce::String& uuid, Terminal::Session& session)
{
    wireOnBytes      (uuid, session);
    wireOnStateFlush (uuid, session);
    wireOnExit       (uuid, session);
}

// =============================================================================

/**
 * @brief Wires `session.onBytes` to broadcast output PDUs to per-session subscribers.
 *
 * Builds a `Message::output` PDU (uuid prefix + raw bytes) and pushes it to
 * every Channel registered in the subscriber list for @p uuid.  The lambda runs
 * on the reader thread and acquires `connectionsLock` for the subscriber lookup.
 *
 * @param uuid     Session UUID used as the PDU routing key.
 * @param session  Terminal::Session whose `onBytes` callback is being set.
 * @note NEXUS PROCESS MESSAGE THREAD (called at wire time; lambda fires on READER THREAD).
 */
void Daemon::wireOnBytes (const juce::String& uuid, Terminal::Session& session)
{
    jassert (session.getProcessor().getState().get().isValid());

    session.onBytes = [this, uuid] (const char* bytes, int len)
    {
        juce::MemoryBlock outputPayload;
        writeString (outputPayload, uuid);
        outputPayload.append (bytes, static_cast<size_t> (len));

        const juce::ScopedLock lock (connectionsLock);
        const auto it { subscribers.find (uuid) };

        if (it != subscribers.end())
        {
            for (auto* conn : it->second)
                conn->sendPdu (Message::output, outputPayload);
        }
    };
}

// =============================================================================

/**
 * @brief Wires `session.onStateFlush` to broadcast stateUpdate PDUs to per-session subscribers.
 *
 * Reads cwd and foreground-process strings from the Processor state on each flush.
 * Broadcasts only when at least one field changed relative to the last broadcast
 * (tracked via function-local `lastSentCwd` / `lastSentFg` shared_ptr captures
 * to avoid 60 Hz noise).
 *
 * @param uuid     Session UUID used as the PDU routing key.
 * @param session  Terminal::Session whose `onStateFlush` callback is being set.
 * @note NEXUS PROCESS MESSAGE THREAD (called at wire time; lambda fires on MESSAGE THREAD).
 */
void Daemon::wireOnStateFlush (const juce::String& uuid, Terminal::Session& session)
{
    jassert (session.getProcessor().getState().get().isValid());

    Terminal::Processor* procRawPtr { &session.getProcessor() };

    // lastSentCwd / lastSentFg must survive across flush calls — kept as shared_ptr
    // captures so they are function-local to this helper but lambda-lifetime.
    auto lastSentCwd { std::make_shared<juce::String>() };
    auto lastSentFg  { std::make_shared<juce::String>() };

    session.onStateFlush = [this, uuid, procRawPtr, lastSentCwd, lastSentFg]
    {
        const juce::String cwdStr { procRawPtr->getState().get().getProperty (Terminal::ID::cwd).toString() };
        const juce::String fgStr  { procRawPtr->getState().get().getProperty (Terminal::ID::foregroundProcess).toString() };

        if (cwdStr != *lastSentCwd or fgStr != *lastSentFg)
        {
            *lastSentCwd = cwdStr;
            *lastSentFg  = fgStr;

            juce::MemoryBlock statePayload;
            writeString (statePayload, uuid);
            writeString (statePayload, cwdStr);
            writeString (statePayload, fgStr);

            const juce::ScopedLock lock (connectionsLock);
            const auto it { subscribers.find (uuid) };

            if (it != subscribers.end())
            {
                for (auto* conn : it->second)
                    conn->sendPdu (Message::stateUpdate, statePayload);
            }
        }
    };
}

// =============================================================================

/**
 * @brief Wires `session.onExit` to broadcast sessionKilled and clean up Nexus state.
 *
 * Broadcasts `Message::sessionKilled` to all attached connections, then schedules
 * an async message-thread call to remove the session from Nexus, re-broadcast the
 * sessions list, and fire `onAllSessionsExited` when no sessions remain.
 *
 * @param uuid     Session UUID used as the PDU routing key.
 * @param session  Terminal::Session whose `onExit` callback is being set.
 * @note NEXUS PROCESS MESSAGE THREAD (called at wire time; lambda fires on READER THREAD → async MESSAGE THREAD).
 */
void Daemon::wireOnExit (const juce::String& uuid, Terminal::Session& session)
{
    jassert (session.getProcessor().getState().get().isValid());

    session.onExit = [this, uuid]
    {
        juce::MemoryBlock exitPayload;
        writeString (exitPayload, uuid);

        {
            const juce::ScopedLock lock (connectionsLock);

            for (auto* conn : attached)
                conn->sendPdu (Message::sessionKilled, exitPayload);
        }

        juce::MessageManager::callAsync (
            [this, uuid]
            {
                nexus.remove (uuid);
                broadcastSessions();

                if (nexus.list().isEmpty() and onAllSessionsExited != nullptr)
                    onAllSessionsExited();
            });
    };
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Interprocess
