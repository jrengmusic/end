/**
 * @file SessionFanout.cpp
 * @brief Nexus::Session subscriber registry and processorList broadcast.
 *
 * In the byte-forward architecture, live PTY output is pushed eagerly from
 * the READER THREAD via `Terminal::Session::onBytes → Message::output`.  There
 * is no fan-out loop needed in handleAsyncUpdate — that method is a no-op here.
 *
 * This file implements:
 * - `attachConnection` / `detachConnection` — per-session subscriber registry.
 * - `broadcastProcessorList` — push processor UUID list to one or all clients.
 *
 * @see Nexus::Session
 */

#include "Session.h"
#include "ServerConnection.h"
#include "Message.h"
#include "Wire.h"
#include "Log.h"
#include "../terminal/logic/Processor.h"
#include "../terminal/logic/Session.h"

#include <algorithm>

namespace Nexus
{
/*____________________________________________________________________________*/

// =============================================================================

/**
 * @brief Sends a `Message::processorList` PDU to @p target only.
 *
 * Wire format: uint16_t count | N × (uint32_t len + UTF-8 bytes).
 * Called from ServerConnection::connectionMade() to deliver the current list
 * to a newly connected client.
 *
 * @note NEXUS PROCESS MESSAGE THREAD.
 */
void Session::broadcastProcessorList (ServerConnection& target)
{
    juce::MemoryBlock payload;
    writeUint16 (payload, static_cast<uint16_t> (processors.size()));

    for (const auto& [uuid, proc] : processors)
    {
        juce::ignoreUnused (proc);
        writeString (payload, uuid);
    }

    target.sendPdu (Message::processorList, payload);
}

/**
 * @brief Sends a `Message::processorList` PDU to every attached connection.
 *
 * Called after processor creation or exit so all connected clients re-sync.
 *
 * @note Acquires connectionsLock.  NEXUS PROCESS MESSAGE THREAD.
 */
void Session::broadcastProcessorList()
{
    juce::MemoryBlock payload;
    writeUint16 (payload, static_cast<uint16_t> (processors.size()));

    for (const auto& [uuid, proc] : processors)
    {
        juce::ignoreUnused (proc);
        writeString (payload, uuid);
    }

    const juce::ScopedLock lock (connectionsLock);

    for (auto* conn : attached)
        conn->sendPdu (Message::processorList, payload);
}

// =============================================================================

/**
 * @brief Atomically sends history snapshot then registers the connection as a subscriber.
 *
 * Inlines both operations under a single connectionsLock acquisition so the
 * reader thread's onBytes broadcast (which also acquires connectionsLock) cannot
 * interleave between history send and subscriber registration.
 *
 * NOTE: `attachConnection` is intentionally NOT called here to avoid recursive
 * lock acquisition confusion even though CriticalSection is recursive.
 *
 * @note NEXUS PROCESS MESSAGE THREAD.
 */
void Session::attachAndSync (const juce::String& uuid, ServerConnection& target)
{
    const juce::ScopedLock lock (connectionsLock);

    // Send loading snapshot FIRST so the client is fully synced before live bytes arrive.
    const auto tsIt { terminalSessions.find (uuid) };

    if (tsIt != terminalSessions.end())
    {
        const juce::MemoryBlock historyBytes { tsIt->second->snapshotHistory() };

        juce::MemoryBlock payload;
        writeString (payload, uuid);
        payload.append (historyBytes.getData(), historyBytes.getSize());

        Nexus::logLine ("Session::attachAndSync: sending loading uuid=" + uuid
                        + " historyBytes=" + juce::String ((int) historyBytes.getSize()));

        target.sendPdu (Message::loading, payload);
    }

    // Register as subscriber — any onBytes broadcast after this point will
    // include `target`, but it cannot run until we release connectionsLock.
    // (Inlined from attachConnection.)
    subscribers[uuid].push_back (&target);

    Nexus::logLine ("Session::attachAndSync: subscriber registered uuid=" + uuid);
}

/**
 * @brief Registers @p connection as a byte-output subscriber for @p uuid.
 *
 * @note Acquires connectionsLock.
 */
void Session::attachConnection (const juce::String& uuid, ServerConnection& connection)
{
    const juce::ScopedLock lock (connectionsLock);
    subscribers[uuid].push_back (&connection);
}

/**
 * @brief Unregisters @p connection from the subscriber list for @p uuid.
 *
 * @note Acquires connectionsLock.
 */
void Session::detachConnection (const juce::String& uuid, ServerConnection& connection)
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

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Nexus
