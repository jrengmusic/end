/**
 * @file Link.cpp
 * @brief Implementation of Nexus::Link — JUCE IPC connector to a remote Host.
 *
 * @see Nexus::Link
 * @see Nexus::Session
 * @see Nexus::Channel
 */

#include "Link.h"
#include "Session.h"
#include "Wire.h"
#include "../AppState.h"
#include "../AppIdentifier.h"
#include "../terminal/data/Identifier.h"
#include "../terminal/logic/Processor.h"

namespace Nexus
{
/*____________________________________________________________________________*/

// =============================================================================

/**
 * @brief Constructs the Link with message-thread callbacks (callbacksOnMessageThread=true).
 *
 * Using `callbacksOnMessageThread = true` so `connectionMade`, `connectionLost`,
 * and `messageReceived` all fire on the JUCE message thread.  No `callAsync`
 * indirection is needed in `messageReceived`.
 */
Link::Link()
    : juce::InterprocessConnection (true, magicHeader)
{
}

/**
 * @brief Calls `disconnect()` — JUCE contract.
 */
Link::~Link()
{
    disconnect();
}

// =============================================================================

/**
 * @brief Disconnects from the host.
 * @note NEXUS PROCESS MESSAGE THREAD.
 */
void Link::disconnectFromHost()
{
    disconnect();
}

// =============================================================================

/**
 * @brief Kicks off async connection attempts to the daemon at 100 ms intervals.
 *
 * Reads the port from the `.nexus` file on disk on each tick for up to 50
 * ticks (5 seconds total).  On a successful socket connect JUCE fires
 * `connectionMade()`, which sends the hello PDU.  On exhaustion, a failure
 * line is logged.
 *
 * @note NEXUS PROCESS MESSAGE THREAD.
 */
void Link::beginConnectAttempts() noexcept
{
    connectTimer = std::make_unique<ConnectTimer> (*this, connectMaxAttempts);
    connectTimer->startTimer (connectRetryIntervalMs);
}

/**
 * @brief Periodic retry callback fired every 100 ms by the JUCE timer.
 *
 * Each tick reads the port from the `.nexus` file on disk and attempts
 * `connectToSocket`.  Reading from disk rather than AppState in-memory means
 * the timer can succeed on the very first tick that the daemon has written its
 * port file, even before AppState has been updated.
 * On success the timer stops itself; JUCE will fire `Link::connectionMade`.
 * On exhaustion the timer stops and a failure line is logged.
 *
 * @note NEXUS PROCESS MESSAGE THREAD.
 */
void Link::ConnectTimer::timerCallback()
{
    bool connected { false };
    const juce::File nexusFile { AppState::getContext()->getNexusFile() };

    if (nexusFile.existsAsFile())
    {
        const int port { nexusFile.loadFileAsString().trim().getIntValue() };

        if (port > 0)
        {
            // Use a short per-probe timeout so the message thread is not blocked
            // for the full connectTimeoutMs on each 100 ms tick.
            connected = owner.connectToSocket ("127.0.0.1", port, perProbeTimeoutMs);
        }
    }

    if (connected)
    {
        stopTimer();
        // Defer self-destruction: nulling connectTimer destroys `this`, which must
        // not happen inside the timer callback.  Post to the message thread so the
        // callback frame completes before the destructor runs.
        auto* ownerPtr { &owner };
        juce::MessageManager::callAsync ([ownerPtr] { ownerPtr->connectTimer = nullptr; });
        // connectionMade() will fire asynchronously via JUCE — no further action here.
    }
    else
    {
        --attemptsRemaining;

        if (attemptsRemaining <= 0)
        {
            stopTimer();
            auto* ownerPtr { &owner };
            juce::MessageManager::callAsync (
                [ownerPtr]
                {
                    ownerPtr->connectTimer = nullptr;
                });
        }
    }
}

// =============================================================================

/**
 * @brief Called by JUCE when the socket connects successfully.
 *
 * Sends the hello PDU and marks this instance as connected in AppState.
 * The SESSIONS subtree is written later when the daemon's `sessions` PDU arrives.
 *
 * @note NEXUS PROCESS MESSAGE THREAD (callbacksOnMessageThread = true).
 */
void Link::connectionMade()
{
    AppState::getContext()->setConnected (true);
    sendPdu (Message::hello);
}

/**
 * @brief Called by JUCE when the connection is lost.
 *
 * Marks this instance as disconnected in AppState and writes state to disk immediately.
 *
 * @note NEXUS PROCESS MESSAGE THREAD (callbacksOnMessageThread = true).
 */
void Link::connectionLost()
{
    AppState::getContext()->setConnected (false);
}

// =============================================================================

// =============================================================================

/**
 * @brief Unsubscribes from render deltas for a session.
 *
 * @note Any thread.
 */
void Link::detachSession (const juce::String& uuid)
{
    juce::MemoryBlock payload;
    writeString (payload, uuid);
    sendPdu (Message::detachSession, payload);
}

/**
 * @brief Forwards raw input bytes to the shell in a session.
 *
 * Payload: uuid (length-prefixed) | raw input bytes.
 *
 * @note Any thread.
 */
void Link::sendInput (const juce::String& uuid, const void* data, int size)
{
    if (size > 0)
    {
        juce::MemoryBlock payload;
        writeString (payload, uuid);
        payload.append (data, static_cast<size_t> (size));
        sendPdu (Message::input, payload);
    }
}

/**
 * @brief Notifies the host of a terminal resize.
 *
 * Payload: uuid (length-prefixed) | cols (uint16_t LE) | rows (uint16_t LE).
 *
 * @note Any thread.
 */
void Link::sendResize (const juce::String& uuid, int cols, int rows)
{
    juce::MemoryBlock payload;
    writeString (payload, uuid);
    writeUint16 (payload, static_cast<uint16_t> (cols));
    writeUint16 (payload, static_cast<uint16_t> (rows));
    sendPdu (Message::resizeSession, payload);
}

/**
 * @brief Requests the host to kill the shell for a session.
 *
 * Payload: uuid (length-prefixed string).
 *
 * @note Any thread.
 */
void Link::sendRemove (const juce::String& uuid)
{
    juce::MemoryBlock payload;
    writeString (payload, uuid);
    sendPdu (Message::killSession, payload);
}

// =============================================================================

/**
 * @brief Encodes @p kind and @p payload, then calls sendMessage().
 *
 * Wire format: uint16_t kind (LE) | payload bytes.
 *
 * @note Any thread.
 */
void Link::sendPdu (Message kind, const juce::MemoryBlock& payload)
{
    sendMessage (encodePdu (kind, payload));
}

// =============================================================================

/**
 * @brief Dispatches an incoming message from the host.
 *
 * Decodes `Message` kind from the first 2 bytes and delegates to the
 * appropriate private handler method.  Mirrors Channel::messageReceived.
 *
 * PDU kinds handled:
 * - `sessions`      → handleSessions
 * - `sessionKilled` → handleSessionKilled
 * - `output`        → handleOutput
 * - `loading`       → handleLoading
 * - `stateUpdate`   → handleStateUpdate
 * - all others      → ignored (logged in debug)
 *
 * @note NEXUS PROCESS MESSAGE THREAD (callbacksOnMessageThread = true).
 */
void Link::messageReceived (const juce::MemoryBlock& message)
{
    const int total { static_cast<int> (message.getSize()) };

    if (total >= 2)
    {
        const auto* data { static_cast<const uint8_t*> (message.getData()) };
        uint16_t rawKind { 0 };
        std::memcpy (&rawKind, data, sizeof (rawKind));

        const auto kind { static_cast<Message> (rawKind) };
        const uint8_t* payload { data + 2 };
        const int payloadSize { total - 2 };

        switch (kind)
        {
            case Message::sessions:      handleSessions      (payload, payloadSize); break;
            case Message::sessionKilled: handleSessionKilled (payload, payloadSize); break;
            case Message::output:           handleOutput          (payload, payloadSize); break;
            case Message::loading:          handleLoading         (payload, payloadSize); break;
            case Message::stateUpdate:      handleStateUpdate     (payload, payloadSize); break;
            default:                        break;
        }
    }
}

// =============================================================================

/**
 * @brief Handles `Message::sessions` — rewrites AppState SESSIONS subtree.
 *
 * Wire format: uint16_t count | N × (uint32_t len + UTF-8 bytes).
 * Removes the nexus-connect LOADING operation on first receipt.
 *
 * @note NEXUS PROCESS MESSAGE THREAD.
 */
void Link::handleSessions (const uint8_t* payload, int payloadSize)
{
    if (payloadSize >= 2)
    {
        const uint16_t count { readUint16 (payload) };
        int cursor { 2 };

        juce::StringArray list;

        for (uint16_t i { 0 }; i < count; ++i)
        {
            juce::String entry;
            const int consumed { readString (payload + cursor, payloadSize - cursor, entry) };

            if (consumed > 0)
            {
                list.add (entry);
                cursor += consumed;
            }
        }

        juce::ValueTree sessionsNode { App::ID::SESSIONS };

        for (const auto& uuid : list)
        {
            juce::ValueTree session { App::ID::SESSION };
            session.setProperty (jreng::ID::id, uuid, nullptr);
            sessionsNode.appendChild (session, nullptr);
        }

        auto nexusNode { AppState::getContext()->getNexusNode() };
        auto existing { nexusNode.getChildWithName (App::ID::SESSIONS) };

        if (existing.isValid())
            nexusNode.removeChild (existing, nullptr);

        nexusNode.appendChild (sessionsNode, nullptr);
    }
}

/**
 * @brief Handles `Message::sessionKilled` — removes the exited UUID from AppState.
 *
 * @note NEXUS PROCESS MESSAGE THREAD.
 */
void Link::handleSessionKilled (const uint8_t* payload, int payloadSize)
{
    juce::String uuid;
    readString (payload, payloadSize, uuid);

    if (uuid.isNotEmpty())
    {
        auto sessionsNode { AppState::getContext()->getSessionsNode() };
        auto exitedSession { jreng::ValueTree::getChildWithID (sessionsNode, uuid) };

        if (exitedSession.isValid())
            exitedSession.getParent().removeChild (exitedSession, nullptr);
    }
}

/**
 * @brief Handles `Message::output` — feeds raw PTY bytes into the target Processor.
 *
 * Payload: uuid (length-prefixed) + raw PTY bytes.
 *
 * @note NEXUS PROCESS MESSAGE THREAD.
 */
void Link::handleOutput (const uint8_t* payload, int payloadSize)
{
    juce::String uuid;
    const int uuidConsumed { readString (payload, payloadSize, uuid) };

    if (uuid.isNotEmpty() and uuidConsumed > 0)
    {
        const void* bytes { payload + uuidConsumed };
        const int byteCount { payloadSize - uuidConsumed };

        if (byteCount > 0)
            Nexus::Session::getContext()->feedBytes (uuid, bytes, byteCount);
    }
}

/**
 * @brief Handles `Message::loading` — receives Grid+State snapshot for restore.
 *
 * Payload: uuid (length-prefixed) + snapshot bytes.
 * Passes the snapshot to `Session::startLoading`, which calls `setStateInformation` directly.
 *
 * @note NEXUS PROCESS MESSAGE THREAD.
 */
void Link::handleLoading (const uint8_t* payload, int payloadSize)
{
    juce::String uuid;
    const int uuidConsumed { readString (payload, payloadSize, uuid) };

    if (uuid.isNotEmpty() and uuidConsumed > 0)
    {
        const int byteCount { payloadSize - uuidConsumed };

        if (byteCount > 0)
        {
            juce::MemoryBlock backlog;
            backlog.append (payload + uuidConsumed, static_cast<size_t> (byteCount));

            Nexus::Session::getContext()->startLoading (uuid, std::move (backlog));
        }
    }
}

/**
 * @brief Handles `Message::stateUpdate` — writes cwd and foreground process into the target Processor's ValueTree.
 *
 * Payload: uuid (length-prefixed) + cwd (length-prefixed) + fgProcess (length-prefixed).
 * Mirrors the standalone flush→setCwd / setForegroundProcess path, but driven by daemon push.
 * Routes via Nexus::Session::get() — Terminal::Session (remote) is owned by terminalSessions.
 *
 * @note NEXUS PROCESS MESSAGE THREAD.
 */
void Link::handleStateUpdate (const uint8_t* payload, int payloadSize)
{
    juce::String uuid;
    const int uuidConsumed { readString (payload, payloadSize, uuid) };

    if (uuidConsumed > 0)
    {
        juce::String cwd;
        const int cwdConsumed { readString (payload + uuidConsumed, payloadSize - uuidConsumed, cwd) };

        juce::String fgProcess;
        readString (payload + uuidConsumed + cwdConsumed,
                    payloadSize - uuidConsumed - cwdConsumed, fgProcess);

        if (cwdConsumed > 0)
        {
            auto* nexus { Nexus::Session::getContext() };

            if (nexus != nullptr and nexus->hasSession (uuid))
            {
                auto& proc { nexus->get (uuid) };

                if (cwd.isNotEmpty())
                    proc.getState().get().setProperty (Terminal::ID::cwd, cwd, nullptr);

                if (fgProcess.isNotEmpty())
                    proc.getState().get().setProperty (Terminal::ID::foregroundProcess, fgProcess, nullptr);
            }
        }
    }
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Nexus
