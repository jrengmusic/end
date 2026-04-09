/**
 * @file SessionFanout.cpp
 * @brief Nexus::Session subscriber registry and processorList broadcast.
 *
 * In the byte-forward architecture, live PTY output is pushed eagerly from
 * the READER THREAD via `Terminal::Session::onBytes → Message::output`.  There
 * is no fan-out loop needed in handleAsyncUpdate — that method is a no-op here.
 *
 * This file implements:
 * - `attach` / `detachConnection` — per-session subscriber registry.
 * - `broadcastProcessorList` — push processor UUID list to one or all clients.
 *
 * @see Nexus::Session
 */

#include "Session.h"
#include "ServerConnection.h"
#include "Message.h"
#include "Wire.h"
#include "../terminal/logic/Processor.h"
#include "../terminal/logic/Session.h"

#include <algorithm>

namespace Nexus
{
/*____________________________________________________________________________*/

// =============================================================================

/**
 * @brief Builds the `Message::processorList` payload from the current terminalSessions map.
 *
 * Wire format: uint16_t count | N × (uint32_t len + UTF-8 bytes).
 *
 * @note NEXUS PROCESS MESSAGE THREAD.
 */
juce::MemoryBlock Session::buildProcessorListPayload() const
{
    juce::MemoryBlock payload;
    writeUint16 (payload, static_cast<uint16_t> (terminalSessions.size()));

    for (const auto& [uuid, termSession] : terminalSessions)
    {
        juce::ignoreUnused (termSession);
        writeString (payload, uuid);
    }

    return payload;
}

/**
 * @brief Sends a `Message::processorList` PDU to @p target only.
 *
 * Called from ServerConnection::connectionMade() to deliver the current list
 * to a newly connected client.
 *
 * @note NEXUS PROCESS MESSAGE THREAD.
 */
void Session::broadcastProcessorList (ServerConnection& target)
{
    target.sendPdu (Message::processorList, buildProcessorListPayload());
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
    const juce::MemoryBlock payload { buildProcessorListPayload() };
    const juce::ScopedLock lock (connectionsLock);

    for (auto* conn : attached)
        conn->sendPdu (Message::processorList, payload);
}

// =============================================================================

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
void Session::attach (const juce::String& uuid, ServerConnection& target,
                      bool sendHistory, int cols, int rows)
{
    {
        const juce::ScopedLock lock (connectionsLock);

        if (sendHistory)
        {
            const auto termIt { terminalSessions.find (uuid) };

            if (termIt != terminalSessions.end())
            {
                juce::MemoryBlock snapshot;
                termIt->second->getProcessor().getStateInformation (snapshot);

                juce::MemoryBlock payload;
                writeString (payload, uuid);
                payload.append (snapshot.getData(), snapshot.getSize());

                target.sendPdu (Message::loading, payload);
            }
        }

        subscribers[uuid].push_back (&target);
    }

    // Resize after lock release — SIGWINCH output broadcast acquires connectionsLock.
    sendResize (uuid, cols, rows);
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
