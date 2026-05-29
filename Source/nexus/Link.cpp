/**
 * @file Link.cpp
 * @brief Implementation of nexus::Link — JUCE IPC connector to a remote Host.
 *
 * @see nexus::Link
 * @see Nexus
 * @see nexus::Channel
 */

#include "Link.h"

namespace nexus
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
    Nexus::getContext()->events.addListener (this);
}

/**
 * @brief Unregisters from Nexus events, cleans up clientSessionStateRoots listeners,
 *        then calls `disconnect()` — JUCE contract.
 */
Link::~Link()
{
    Nexus::getContext()->events.removeListener (this);

    for (auto& entry : clientSessionStateRoots)
        entry.second.removeListener (this);

    clientSessionStateRoots.clear();
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
 * Sends the hello PDU to initiate the handshake.
 * The SESSIONS subtree is written later when the daemon's `sessions` PDU arrives.
 *
 * @note NEXUS PROCESS MESSAGE THREAD (callbacksOnMessageThread = true).
 */
void Link::connectionMade()
{
    sendPdu (Message::hello);
}

/**
 * @brief Called by JUCE when the connection is lost.
 *
 * @note NEXUS PROCESS MESSAGE THREAD (callbacksOnMessageThread = true).
 */
void Link::connectionLost()
{
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
    Codec::writeString (payload, uuid);
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
        Codec::writeString (payload, uuid);
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
    Codec::writeString (payload, uuid);
    Codec::writeUint16 (payload, static_cast<uint16_t> (cols));
    Codec::writeUint16 (payload, static_cast<uint16_t> (rows));
    sendPdu (Message::resizeSession, payload);
}

/**
 * @brief Sends a `Message::createSession` PDU to the daemon.
 *
 * Payload: cwd (length-prefixed) | uuid (length-prefixed) | cols (uint16_t LE) | rows (uint16_t LE).
 *
 * @note Any thread.
 */
void Link::sendCreateSession (const juce::String& cwd, const juce::String& uuid, int cols, int rows)
{
    juce::MemoryBlock payload;
    Codec::writeString (payload, cwd);
    Codec::writeString (payload, uuid);
    Codec::writeUint16 (payload, static_cast<uint16_t> (cols));
    Codec::writeUint16 (payload, static_cast<uint16_t> (rows));
    sendPdu (Message::createSession, payload);
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
    Codec::writeString (payload, uuid);
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
        const uint16_t count { Codec::readUint16 (payload) };
        int cursor { 2 };

        juce::StringArray list;

        for (uint16_t i { 0 }; i < count; ++i)
        {
            juce::String entry;
            const int consumed { Codec::readString (payload + cursor, payloadSize - cursor, entry) };

            if (consumed > 0)
            {
                list.add (entry);
                cursor += consumed;
            }
        }

        juce::ValueTree sessionsNode { app::id::SESSIONS };

        for (const auto& uuid : list)
        {
            juce::ValueTree session { app::id::SESSION };
            session.setProperty (jam::ID::id, uuid, nullptr);
            sessionsNode.appendChild (session, nullptr);
        }

        auto nexusNode { AppState::getContext()->getNexusNode() };
        auto existing { nexusNode.getChildWithName (app::id::SESSIONS) };

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
    Codec::readString (payload, payloadSize, uuid);

    if (uuid.isNotEmpty())
    {
        auto sessionsNode { AppState::getContext()->getSessionsNode() };
        auto exitedSession { jam::ValueTree::getChildWithID (sessionsNode, uuid) };

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
    const int uuidConsumed { Codec::readString (payload, payloadSize, uuid) };

    if (uuid.isNotEmpty() and uuidConsumed > 0)
    {
        const void* bytes { payload + uuidConsumed };
        const int byteCount { payloadSize - uuidConsumed };

        if (byteCount > 0)
        {
            Nexus* ctx { Nexus::getContext() };
            jassert (ctx != nullptr);

            if (ctx->has (uuid))
                ctx->get (uuid).process (static_cast<const char*> (bytes), byteCount);
        }
    }
}

/**
 * @brief Handles `Message::loading` — receives Grid+State snapshot for restore.
 *
 * Payload: uuid (length-prefixed) + snapshot bytes.
 * Passes the snapshot to `terminal::Session::setStateInformation` directly.
 *
 * @note NEXUS PROCESS MESSAGE THREAD.
 */
void Link::handleLoading (const uint8_t* payload, int payloadSize)
{
    juce::String uuid;
    const int uuidConsumed { Codec::readString (payload, payloadSize, uuid) };

    if (uuid.isNotEmpty() and uuidConsumed > 0)
    {
        const int byteCount { payloadSize - uuidConsumed };

        if (byteCount > 0)
        {
            juce::MemoryBlock backlog;
            backlog.append (payload + uuidConsumed, static_cast<size_t> (byteCount));

            Nexus* ctx { Nexus::getContext() };
            jassert (ctx != nullptr);

            if (ctx->has (uuid))
                ctx->get (uuid).setStateInformation (backlog.getData(), static_cast<int> (backlog.getSize()));
        }
    }
}

/**
 * @brief Handles `Message::stateUpdate` — writes cwd and foreground process into the target Processor's ValueTree.
 *
 * Payload: uuid (length-prefixed) + cwd (length-prefixed) + fgProcess (length-prefixed).
 * Both cwd and foregroundProcess are written via direct setProperty on the SESSION ValueTree node
 * (MESSAGE thread). cwd is the authoritative path from OSC 7 as received by the daemon and pushed
 * here. foregroundProcess is the OS process name queried by the daemon on outputBlockTop.
 * Routes via Nexus::get() — terminal::Session (remote) is owned by the Nexus sessions map.
 *
 * @note NEXUS PROCESS MESSAGE THREAD.
 */
void Link::handleStateUpdate (const uint8_t* payload, int payloadSize)
{
    juce::String uuid;
    const int uuidConsumed { Codec::readString (payload, payloadSize, uuid) };

    if (uuidConsumed > 0)
    {
        juce::String cwd;
        const int cwdConsumed { Codec::readString (payload + uuidConsumed, payloadSize - uuidConsumed, cwd) };

        juce::String fgProcess;
        Codec::readString (payload + uuidConsumed + cwdConsumed,
                           payloadSize - uuidConsumed - cwdConsumed, fgProcess);

        if (cwdConsumed > 0)
        {
            Nexus* ctx { Nexus::getContext() };
            jassert (ctx != nullptr);

            if (ctx->has (uuid))
            {
                auto& proc { ctx->get (uuid).getProcessor() };

                if (cwd.isNotEmpty())
                    proc.getState().get().setProperty (terminal::id::cwd, cwd, nullptr);

                if (fgProcess.isNotEmpty())
                    proc.getState().get().setProperty (terminal::id::foregroundProcess, fgProcess, nullptr);
            }
        }
    }
}

// =============================================================================

/**
 * @brief Reacts to session creation on Nexus::events.
 *
 * When a "SESSION" child is appended to `nexus.events` in client mode, sends a
 * createSession PDU to the daemon, wires user input to daemon via IPC, and
 * registers this Link as a listener on the session State VT for resize forwarding.
 *
 * @note NEXUS PROCESS MESSAGE THREAD.
 */
void Link::valueTreeChildAdded (juce::ValueTree&, juce::ValueTree& child)
{
    if (child.getType().toString() == "SESSION")
    {
        const auto uuid { child.getProperty (jam::ID::id).toString() };
        Nexus* ctx { Nexus::getContext() };

        if (uuid.isNotEmpty() and ctx->has (uuid))
        {
            auto& session { ctx->get (uuid) };

            // Send createSession PDU to daemon.
            sendCreateSession (session.getProcessor().getState().getCwd(),
                               uuid,
                               session.getProcessor().getState().getCols().value,
                               session.getProcessor().getState().getVisibleRows().value);

            // Wire user input (keyboard, mouse) to daemon via Link IPC.
            session.getProcessor().setInputWriter ([this, uuid] (const char* data, int len)
            {
                sendInput (uuid, data, len);
            });

            // Track session State VT for resize forwarding.
            juce::ValueTree stateRoot { session.getProcessor().getState().get() };
            clientSessionStateRoots[uuid] = stateRoot;
            stateRoot.addListener (this);
        }
    }
}

/**
 * @brief Reacts to session removal on Nexus::events.
 *
 * When a "SESSION" child is removed from `nexus.events`, sends a remove PDU to
 * the daemon and cleans up the clientSessionStateRoots entry.
 *
 * @note NEXUS PROCESS MESSAGE THREAD.
 */
void Link::valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree& child, int)
{
    if (child.getType().toString() == "SESSION")
    {
        const auto uuid { child.getProperty (jam::ID::id).toString() };

        if (uuid.isNotEmpty())
        {
            sendRemove (uuid);

            const auto it { clientSessionStateRoots.find (uuid) };

            if (it != clientSessionStateRoots.end())
            {
                it->second.removeListener (this);
                clientSessionStateRoots.erase (it);
            }
        }
    }
}

/**
 * @brief Detects winsize changes on client-mode session State VTs and
 *        forwards a resize PDU to the daemon.
 *
 * Watches for the `winsize` property on the TEXT_EDITOR child node.
 * Matches the tree's parent (SESSION root) against `clientSessionStateRoots`
 * to find the affected UUID.  Unpacks the packed Bounds to obtain cols and rows.
 *
 * @note NEXUS PROCESS MESSAGE THREAD.
 */
void Link::valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property)
{
    if (property == jam::TextEditor::properties.at (jam::TextEditor::viewportId)
        and tree.getType() == jam::TextEditor::properties.at (jam::TextEditor::textEditorId))
    {
        const juce::ValueTree sessionRoot { tree.getParent() };

        juce::String resizeUuid;

        for (const auto& entry : clientSessionStateRoots)
        {
            if (entry.second == sessionRoot)
            {
                resizeUuid = entry.first;
                break;
            }
        }

        if (resizeUuid.isNotEmpty())
        {
            const auto dims { jam::Cell::Rectangle::unpack (static_cast<int64_t> (static_cast<int> (tree.getProperty (property)))) };
            sendResize (resizeUuid, dims.getWidth().value, dims.getHeight().value);
        }
    }
}

/**______________________________END OF NAMESPACE______________________________*/
} // namespace nexus
