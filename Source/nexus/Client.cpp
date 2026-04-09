/**
 * @file Client.cpp
 * @brief Implementation of Nexus::Client — JUCE IPC connector to a remote Host.
 *
 * @see Nexus::Client
 * @see Nexus::Session
 * @see Nexus::ServerConnection
 */

#include "Client.h"
#include "Log.h"
#include "Server.h"
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
 * @brief Returns the path to the port lockfile — delegates to Server::getLockfile().
 *
 * Single source of truth for the lockfile path lives in Nexus::Server.
 */
juce::File Client::getLockfile()
{
    return Server::getLockfile();
}

// =============================================================================

/**
 * @brief Constructs the Client with message-thread callbacks (callbacksOnMessageThread=true).
 *
 * Using `callbacksOnMessageThread = true` so `connectionMade`, `connectionLost`,
 * and `messageReceived` all fire on the JUCE message thread.  No `callAsync`
 * indirection is needed in `messageReceived`.
 */
Client::Client()
    : juce::InterprocessConnection (true, magicHeader)
{
}

/**
 * @brief Calls `disconnect()` — JUCE contract.
 */
Client::~Client()
{
    disconnect();
}

// =============================================================================

/**
 * @brief Reads port from lockfile and connects to host at 127.0.0.1.
 *
 * @return `true` if connection succeeded.
 * @note NEXUS PROCESS MESSAGE THREAD.
 */
bool Client::connectToHost()
{
    bool connected { false };
    const juce::File lockfile { getLockfile() };

    if (lockfile.existsAsFile())
    {
        const int port { lockfile.loadFileAsString().getIntValue() };

        if (port > 0)
            connected = connectToSocket ("127.0.0.1", port, connectTimeoutMs);
    }

    return connected;
}

/**
 * @brief Disconnects from the host.
 * @note NEXUS PROCESS MESSAGE THREAD.
 */
void Client::disconnectFromHost()
{
    disconnect();
}

// =============================================================================

/**
 * @brief Kicks off async connection attempts to the daemon at 100 ms intervals.
 *
 * Reads @p lockfilePath on each tick for up to 50 ticks (5 seconds total).
 * On a successful socket connect JUCE fires `connectionMade()`, which sends
 * the hello PDU.  On exhaustion, a failure line is logged.
 *
 * @note NEXUS PROCESS MESSAGE THREAD.
 */
void Client::beginConnectAttempts (const juce::File& lockfilePath) noexcept
{
    connectTimer = std::make_unique<ConnectTimer> (*this, lockfilePath, connectMaxAttempts);
    connectTimer->startTimer (connectRetryIntervalMs);
}

/**
 * @brief Periodic retry callback fired every 100 ms by the JUCE timer.
 *
 * Each tick reads the lockfile, parses the port, and attempts `connectToSocket`.
 * On success the timer stops itself; JUCE will fire `Client::connectionMade`.
 * On exhaustion the timer stops and a failure line is logged.
 *
 * @note NEXUS PROCESS MESSAGE THREAD.
 */
void Client::ConnectTimer::timerCallback()
{
    bool connected { false };

    const bool lockfileExists { lockfile.existsAsFile() };
    Nexus::logLine ("Nexus::Client ConnectTimer: tick, attemptsRemaining=" + juce::String (attemptsRemaining)
                    + " lockfileExists=" + juce::String (lockfileExists ? "yes" : "no"));

    if (lockfileExists)
    {
        const int port { lockfile.loadFileAsString().getIntValue() };
        Nexus::logLine ("Nexus::Client ConnectTimer: port=" + juce::String (port));

        if (port > 0)
        {
            // Use a short per-probe timeout so the message thread is not blocked
            // for the full connectTimeoutMs on each 100 ms tick.
            static constexpr int probeTimeoutMs { 200 };
            connected = owner.connectToSocket ("127.0.0.1", port, probeTimeoutMs);
            Nexus::logLine ("Nexus::Client ConnectTimer: connectToSocket returned " + juce::String (connected ? "true" : "false"));
        }
    }

    if (connected)
    {
        Nexus::logLine ("Nexus::Client ConnectTimer: connection succeeded - stopping timer");
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
            Nexus::logLine ("Nexus::Client ConnectTimer: exhausted all attempts");
            stopTimer();
            auto* ownerPtr { &owner };
            juce::MessageManager::callAsync (
                [ownerPtr]
                {
                    ownerPtr->connectTimer = nullptr;
                    Nexus::logLine ("Client: connection failed after maximum retry attempts");
                });
        }
    }
}

// =============================================================================

/**
 * @brief Called by JUCE when the socket connects successfully.
 *
 * Sends the hello PDU only.  The PROCESSORS subtree is written later
 * when the daemon's `processorList` PDU arrives.
 *
 * @note NEXUS PROCESS MESSAGE THREAD (callbacksOnMessageThread = true).
 */
void Client::connectionMade()
{
    Nexus::logLine ("Nexus::Client: connectionMade() - sending hello PDU");
    sendPdu (Message::hello);
}

/**
 * @brief Called by JUCE when the connection is lost.
 *
 * @note NEXUS PROCESS MESSAGE THREAD (callbacksOnMessageThread = true).
 */
void Client::connectionLost()
{
}

// =============================================================================

// =============================================================================

/**
 * @brief Requests the host to create or attach to a PTY session.
 *
 * Payload: shell | args | cwd | uuid | cols (uint16_t LE) | rows (uint16_t LE) | envID
 *
 * @note Any thread.
 */
void Client::createSession (int cols, int rows,
                             const juce::String& shell,
                             const juce::String& cwd,
                             const juce::String& uuid,
                             const juce::String& envID)
{
    Nexus::logLine ("Client::createSession: sending createProcessor uuid=" + uuid
                    + " cols=" + juce::String (cols)
                    + " rows=" + juce::String (rows)
                    + " shell=" + shell
                    + " cwd=" + cwd
                    + " envID=" + envID);

    {
        const juce::ScopedLock lock (attachedUuidsLock);
        attachedUuids.add (uuid);
    }

    juce::MemoryBlock payload;
    writeString (payload, shell);
    writeString (payload, {});          // args — empty
    writeString (payload, cwd);
    writeString (payload, uuid);
    writeUint16 (payload, static_cast<uint16_t> (cols));
    writeUint16 (payload, static_cast<uint16_t> (rows));
    writeString (payload, envID);
    sendPdu (Message::createProcessor, payload);
}

/**
 * @brief Unsubscribes from render deltas for a session.
 *
 * @note Any thread.
 */
void Client::detachSession (const juce::String& uuid)
{
    {
        const juce::ScopedLock lock (attachedUuidsLock);
        attachedUuids.removeString (uuid);
    }

    juce::MemoryBlock payload;
    writeString (payload, uuid);
    sendPdu (Message::detachProcessor, payload);
}

/**
 * @brief Forwards raw input bytes to the shell in a session.
 *
 * Payload: uuid (length-prefixed) | raw input bytes.
 *
 * @note Any thread.
 */
void Client::sendInput (const juce::String& uuid, const void* data, int size)
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
void Client::sendResize (const juce::String& uuid, int cols, int rows)
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
void Client::sendRemove (const juce::String& uuid)
{
    Nexus::logLine ("Client::sendRemove: sending removeProcessor uuid=" + uuid);

    juce::MemoryBlock payload;
    writeString (payload, uuid);
    sendPdu (Message::removeProcessor, payload);
}

// =============================================================================

/**
 * @brief Takes ownership of @p processor and registers it for output/history PDU routing.
 *
 * UUID is read from processor->getUuid().  The Processor is owned by Client until
 * unregisterProcessor or Client destruction.
 *
 * @note NEXUS PROCESS MESSAGE THREAD.
 */
void Client::registerProcessor (std::unique_ptr<Terminal::Processor> processor)
{
    jassert (processor != nullptr);

    const juce::String uuid { processor->getUuid() };
    Nexus::logLine ("Client::registerProcessor: uuid=" + uuid
                    + " mapSizeBefore=" + juce::String ((int) hostedProcessors.size()));
    hostedProcessors[uuid] = std::move (processor);
    Nexus::logLine ("Client::registerProcessor: mapSizeAfter=" + juce::String ((int) hostedProcessors.size()));
}

/**
 * @brief Removes and destroys the Processor registered for @p uuid.
 * @note NEXUS PROCESS MESSAGE THREAD.
 */
void Client::unregisterProcessor (const juce::String& uuid)
{
    hostedProcessors.erase (uuid);
}

/**
 * @brief Returns a non-owning pointer to the Processor for @p uuid, or nullptr.
 * @note NEXUS PROCESS MESSAGE THREAD.
 */
Terminal::Processor* Client::getProcessor (const juce::String& uuid) const
{
    Terminal::Processor* result { nullptr };
    const auto it { hostedProcessors.find (uuid) };

    if (it != hostedProcessors.end())
        result = it->second.get();

    return result;
}

// =============================================================================

/**
 * @brief Encodes @p kind and @p payload, then calls sendMessage().
 *
 * Wire format: uint16_t kind (LE) | payload bytes.
 *
 * @note Any thread.
 */
void Client::sendPdu (Message kind, const juce::MemoryBlock& payload)
{
    juce::MemoryBlock message;
    const auto kindValue { static_cast<uint16_t> (kind) };
    message.append (&kindValue, sizeof (kindValue));
    message.append (payload.getData(), payload.getSize());
    sendMessage (message);
}

// =============================================================================

/**
 * @brief Dispatches an incoming message from the host.
 *
 * Decodes `Message` kind from the first 2 bytes and delegates to the
 * appropriate private handler method.  Mirrors ServerConnection::messageReceived.
 *
 * PDU kinds handled:
 * - `processorList`   → handleProcessorList
 * - `processorExited` → handleProcessorExited
 * - `output`          → handleOutput
 * - `loading`         → handleLoading
 * - all others        → handleUnknown (forwarded to onPdu)
 *
 * @note NEXUS PROCESS MESSAGE THREAD (callbacksOnMessageThread = true).
 */
void Client::messageReceived (const juce::MemoryBlock& message)
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
            case Message::processorList:    handleProcessorList   (payload, payloadSize); break;
            case Message::processorExited:  handleProcessorExited (payload, payloadSize); break;
            case Message::output:           handleOutput          (payload, payloadSize); break;
            case Message::loading:          handleLoading         (payload, payloadSize); break;
            case Message::stateUpdate:      handleStateUpdate     (payload, payloadSize); break;
            default:                        handleUnknown         (kind, payload, payloadSize); break;
        }
    }
}

// =============================================================================

/**
 * @brief Handles `Message::processorList` — rewrites AppState PROCESSORS subtree.
 *
 * Wire format: uint16_t count | N × (uint32_t len + UTF-8 bytes).
 * Removes the nexus-connect LOADING operation on first receipt.
 *
 * @note NEXUS PROCESS MESSAGE THREAD.
 */
void Client::handleProcessorList (const uint8_t* payload, int payloadSize)
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

        juce::ValueTree processorsNode { App::ID::PROCESSORS };

        for (const auto& uuid : list)
        {
            juce::ValueTree processor { App::ID::PROCESSOR };
            processor.setProperty (jreng::ID::id, uuid, nullptr);
            processorsNode.appendChild (processor, nullptr);
        }

        auto nexusNode { AppState::getContext()->getNexusNode() };
        auto existing { nexusNode.getChildWithName (App::ID::PROCESSORS) };

        if (existing.isValid())
            nexusNode.removeChild (existing, nullptr);

        nexusNode.appendChild (processorsNode, nullptr);
    }
}

/**
 * @brief Handles `Message::processorExited` — removes the exited UUID from AppState.
 *
 * Also forwards to `onPdu` if set.
 *
 * @note NEXUS PROCESS MESSAGE THREAD.
 */
void Client::handleProcessorExited (const uint8_t* payload, int payloadSize)
{
    juce::String uuid;
    readString (payload, payloadSize, uuid);

    if (uuid.isNotEmpty())
    {
        auto processorsNode { AppState::getContext()->getProcessorsNode() };
        auto exitedProcessor { jreng::ValueTree::getChildWithID (processorsNode, uuid) };

        if (exitedProcessor.isValid())
            exitedProcessor.getParent().removeChild (exitedProcessor, nullptr);
    }

    if (onPdu != nullptr)
    {
        const juce::MemoryBlock payloadBlock { payload, static_cast<size_t> (payloadSize) };
        onPdu (Message::processorExited, payloadBlock);
    }
}

/**
 * @brief Handles `Message::output` — feeds raw PTY bytes into the target Processor.
 *
 * Payload: uuid (length-prefixed) + raw PTY bytes.
 * Also forwards to `onPdu` if set.
 *
 * @note NEXUS PROCESS MESSAGE THREAD.
 */
void Client::handleOutput (const uint8_t* payload, int payloadSize)
{
    juce::String uuid;
    const int uuidConsumed { readString (payload, payloadSize, uuid) };

    Nexus::logLine ("Client::handleOutput: uuid=" + uuid
                    + " payloadSize=" + juce::String (payloadSize)
                    + " uuidConsumed=" + juce::String (uuidConsumed));

    if (uuid.isNotEmpty() and uuidConsumed > 0)
    {
        const void* bytes { payload + uuidConsumed };
        const int byteCount { payloadSize - uuidConsumed };

        Nexus::logLine ("Client::handleOutput: byteCount=" + juce::String (byteCount));

        if (byteCount > 0)
            Nexus::Session::getContext()->feedBytes (uuid, bytes, byteCount);
    }

    if (onPdu != nullptr)
    {
        const juce::MemoryBlock payloadBlock { payload, static_cast<size_t> (payloadSize) };
        onPdu (Message::output, payloadBlock);
    }
}

/**
 * @brief Handles `Message::loading` — receives Grid+State snapshot for restore.
 *
 * Payload: uuid (length-prefixed) + snapshot bytes.
 * Passes the snapshot to `Session::startLoading`, which calls `setStateInformation` directly.
 * Also forwards to `onPdu` if set.
 *
 * @note NEXUS PROCESS MESSAGE THREAD.
 */
void Client::handleLoading (const uint8_t* payload, int payloadSize)
{
    juce::String uuid;
    const int uuidConsumed { readString (payload, payloadSize, uuid) };

    Nexus::logLine ("Client::handleLoading: uuid=" + uuid
                    + " payloadSize=" + juce::String (payloadSize)
                    + " uuidConsumed=" + juce::String (uuidConsumed));

    if (uuid.isNotEmpty() and uuidConsumed > 0)
    {
        const int byteCount { payloadSize - uuidConsumed };

        Nexus::logLine ("Client::handleLoading: byteCount=" + juce::String (byteCount));

        if (byteCount > 0)
        {
            juce::MemoryBlock backlog;
            backlog.append (payload + uuidConsumed, static_cast<size_t> (byteCount));

            Nexus::Session::getContext()->startLoading (uuid, std::move (backlog));
        }
    }

    if (onPdu != nullptr)
    {
        const juce::MemoryBlock payloadBlock { payload, static_cast<size_t> (payloadSize) };
        onPdu (Message::loading, payloadBlock);
    }
}

/**
 * @brief Handles `Message::stateUpdate` — writes cwd and foreground process into the target Processor's ValueTree.
 *
 * Payload: uuid (length-prefixed) + cwd (length-prefixed) + fgProcess (length-prefixed).
 * Mirrors the standalone flush→setCwd / setForegroundProcess path, but driven by daemon push.
 *
 * @note NEXUS PROCESS MESSAGE THREAD.
 */
void Client::handleStateUpdate (const uint8_t* payload, int payloadSize)
{
    juce::String uuid;
    const int uuidConsumed { readString (payload, payloadSize, uuid) };

    if (uuidConsumed > 0)
    {
        juce::String cwd;
        const int cwdConsumed { readString (payload + uuidConsumed, payloadSize - uuidConsumed, cwd) };

        juce::String fgProcess;
        const int fgConsumed { readString (payload + uuidConsumed + cwdConsumed,
                                           payloadSize - uuidConsumed - cwdConsumed, fgProcess) };

        if (cwdConsumed > 0)
        {
            auto* proc { getProcessor (uuid) };

            if (proc != nullptr)
            {
                proc->getState().get().setProperty (Terminal::ID::cwd, cwd, nullptr);

                if (fgConsumed > 0)
                    proc->getState().get().setProperty (Terminal::ID::foregroundProcess, fgProcess, nullptr);
            }
        }
    }
}

/**
 * @brief Handles all unrecognised PDU kinds — forwards to `onPdu` if set.
 *
 * @note NEXUS PROCESS MESSAGE THREAD.
 */
void Client::handleUnknown (Message kind, const uint8_t* payload, int payloadSize)
{
    if (onPdu != nullptr)
    {
        const juce::MemoryBlock payloadBlock { payload, static_cast<size_t> (payloadSize) };
        onPdu (kind, payloadBlock);
    }
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Nexus
