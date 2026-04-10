/**
 * @file Session.cpp
 * @brief Implementation of Nexus::Session — unified session pool and IPC server lifecycle.
 *
 * @see Nexus::Session
 * @see Terminal::Session
 * @see Terminal::Processor
 * @see Nexus::Server
 * @see Nexus::ServerConnection
 */

#include "Session.h"
#include "Client.h"
#include "Server.h"
#include "ServerConnection.h"
#include "Message.h"
#include "Wire.h"
#include "../terminal/logic/Processor.h"
#include "../terminal/logic/Session.h"
#include "../terminal/data/Identifier.h"
#include "../AppState.h"
#include "../AppIdentifier.h"
#include "../config/Config.h"

#include <algorithm>
#include <cstring>

// =============================================================================

namespace Nexus
{
/*____________________________________________________________________________*/

/**
 * @brief Constructs the Session in local mode — no IPC.
 */
Session::Session()
{
    juce::ValueTree processorsNode { App::ID::PROCESSORS };
    AppState::getContext()->getNexusNode().appendChild (processorsNode, nullptr);
}

/**
 * @brief Constructs the Session in daemon mode — constructs Server and starts listening.
 *
 * @note NEXUS PROCESS MESSAGE THREAD.
 */
Session::Session (DaemonTag) { startServer(); }

/**
 * @brief Constructs the Session in client mode — constructs Client and begins connect attempts.
 *
 * Adds a LOADING OPERATION child for the initial connection phase.  The child
 * is removed when the first processorList PDU arrives in Client::messageReceived.
 *
 * @note NEXUS PROCESS MESSAGE THREAD.
 */
Session::Session (ClientTag)
{
    client = std::make_unique<Client>();
    client->beginConnectAttempts();
}

/**
 * @brief Destructs the Session — disconnects client or stops server before releasing sessions.
 */
Session::~Session()
{
    if (client != nullptr)
    {
        client->disconnectFromHost();
        client = nullptr;
    }

    stopServer();
}

// =============================================================================

/**
 * @brief Opens a terminal session with an explicit UUID.  Returns Processor reference.
 *
 * Single entry point for all session creation across all three modes.
 * All three modes store the resulting Terminal::Session in terminalSessions.
 * - Client mode: reads shell from Config locally; constructs a remote Terminal::Session (no TTY);
 *   sends minimal PDU (cwd | uuid | cols | rows) to daemon; wires IPC callbacks.
 * - Local/daemon mode: constructs a full Terminal::Session via Terminal::Session::create
 *   (resolves shell/args from Config internally); then wires Nexus callbacks.
 *
 * If a session with @p uuid already exists the existing Processor is returned
 * immediately (idempotency guard for GUI reconnect).
 *
 * @param cwd   Initial working directory.  Empty = inherit.
 * @param uuid  Must be non-empty.
 * @param cols  Must be > 0.
 * @param rows  Must be > 0.
 * @note NEXUS PROCESS MESSAGE THREAD.
 */
Terminal::Processor& Session::openTerminal (const juce::String& cwd,
                                             const juce::String& uuid,
                                             int cols,
                                             int rows)
{
    jassert (uuid.isNotEmpty());
    jassert (cols > 0);
    jassert (rows > 0);

    // Idempotency: if a session with this uuid already exists, return it.
    // Happens on GUI reconnect — client restores saved uuid and re-sends
    // createProcessor before the daemon-side guard triggers.
    const auto existingTerm { terminalSessions.find (uuid) };
    const bool alreadyExists { existingTerm != terminalSessions.end() };

    Terminal::Processor* result { nullptr };

    if (alreadyExists)
    {
        result = &existingTerm->second->getProcessor();
    }
    else if (client != nullptr)
    {
        // Send minimal PDU: cwd | uuid | cols | rows.  Daemon resolves shell/args from its config.
        juce::MemoryBlock payload;
        Nexus::writeString (payload, cwd);
        Nexus::writeString (payload, uuid);
        Nexus::writeUint16 (payload, static_cast<uint16_t> (cols));
        Nexus::writeUint16 (payload, static_cast<uint16_t> (rows));
        client->sendPdu (Message::createProcessor, payload);

        // Remote session — Processor + State, no TTY.  Daemon owns the PTY.
        // Read shell from local Config so State/display logic has a shell name.
        const juce::String shell { Config::getContext()->getString (Config::Key::shellProgram) };
        auto termSession { std::make_unique<Terminal::Session> (cols, rows, cwd, shell, uuid) };

        Terminal::Processor* procRawPtr { &termSession->getProcessor() };

        // Wire user input (keyboard, mouse) to daemon via Client IPC.
        procRawPtr->writeInput = [this, uuid] (const char* data, int len)
        {
            client->sendInput (uuid, data, len);
        };

        // Wire resize to daemon via Client IPC.
        procRawPtr->onResize = [this, uuid] (int cols, int rows)
        {
            client->sendResize (uuid, cols, rows);
        };

        result = procRawPtr;
        terminalSessions.emplace (uuid, std::move (termSession));
    }
    else
    {
        auto termSession { Terminal::Session::create (cwd, cols, rows, {}, {}, {}, uuid) };

        Terminal::Processor* procRawPtr { &termSession->getProcessor() };

        if (server != nullptr)
        {
            // Daemon mode — Terminal::Session already wires processWithLock internally.
            // Nexus wires onBytes to ALSO broadcast output PDU to subscribers.
            termSession->onBytes = [this, uuid] (const char* bytes, int len)
            {
                // READER THREAD — build output PDU and push to all subscribers.
                juce::MemoryBlock payload;
                writeString (payload, uuid);
                payload.append (bytes, static_cast<size_t> (len));

                const juce::ScopedLock lock (connectionsLock);
                const auto it { subscribers.find (uuid) };

                if (it != subscribers.end())
                {
                    for (auto* conn : it->second)
                        conn->sendPdu (Message::output, payload);
                }
            };

            // Daemon mode — wire onStateFlush to broadcast stateUpdate PDU to subscribers.
            // Terminal::Session has already updated cwd and foreground process in State by the
            // time onStateFlush fires.  lastSentCwd and lastSentFg track the previously-broadcast
            // values to avoid 60Hz noise on unchanged state.
            auto lastSentCwd { std::make_shared<juce::String>() };
            auto lastSentFg { std::make_shared<juce::String>() };

            termSession->onStateFlush = [this, uuid, procRawPtr, lastSentCwd, lastSentFg]
            {
                const juce::String cwdStr { procRawPtr->getState().get().getProperty (Terminal::ID::cwd).toString() };
                const juce::String fgStr {
                    procRawPtr->getState().get().getProperty (Terminal::ID::foregroundProcess).toString()
                };

                if (cwdStr != *lastSentCwd or fgStr != *lastSentFg)
                {
                    *lastSentCwd = cwdStr;
                    *lastSentFg = fgStr;

                    juce::MemoryBlock payload;
                    Nexus::writeString (payload, uuid);
                    Nexus::writeString (payload, cwdStr);
                    Nexus::writeString (payload, fgStr);

                    const juce::ScopedLock lock (connectionsLock);
                    const auto it { subscribers.find (uuid) };

                    if (it != subscribers.end())
                    {
                        for (auto* conn : it->second)
                            conn->sendPdu (Message::stateUpdate, payload);
                    }
                }
            };

            // Daemon mode — wire onExit to broadcast processorExited to all attached connections.
            termSession->onExit = [this, uuid]
            {
                juce::MemoryBlock payload;
                const auto* utf8 { uuid.toRawUTF8() };
                const auto len { static_cast<uint32_t> (uuid.getNumBytesAsUTF8()) };
                payload.append (&len, sizeof (len));
                payload.append (utf8, static_cast<size_t> (len));

                {
                    const juce::ScopedLock lock (connectionsLock);

                    for (auto* conn : attached)
                        conn->sendPdu (Message::processorExited, payload);
                }

                juce::MessageManager::callAsync (
                    [this, uuid]
                    {
                        remove (uuid);
                        broadcastProcessorList();
                        fireIfAllExited();
                    });
            };
        }
        else
        {
            // Local mode — Terminal::Session already wires the full pipeline internally.
            // Nexus only needs to fire onShellExited on the Processor when the TTY exits.
            termSession->onExit = [this, uuid]
            {
                juce::MessageManager::callAsync (
                    [this, uuid]
                    {
                        const auto it { terminalSessions.find (uuid) };

                        if (it != terminalSessions.end() and it->second->getProcessor().onShellExited != nullptr)
                            it->second->getProcessor().onShellExited();
                    });
            };
        }

        result = procRawPtr;
        terminalSessions.emplace (uuid, std::move (termSession));
    }

    jassert (result != nullptr);
    return *result;
}

/**
 * @brief Returns a mutable reference to the Processor with the given UUID.
 *
 * @note NEXUS PROCESS MESSAGE THREAD.
 */
Terminal::Processor& Session::get (const juce::String& uuid)
{
    const auto it { terminalSessions.find (uuid) };
    jassert (it != terminalSessions.end());

    Terminal::Processor* result { nullptr };

    if (it != terminalSessions.end())
        result = &it->second->getProcessor();

    jassert (result != nullptr);
    return *result;
}

/**
 * @brief Returns a const reference to the Processor with the given UUID.
 *
 * @note NEXUS PROCESS MESSAGE THREAD.
 */
const Terminal::Processor& Session::get (const juce::String& uuid) const
{
    const auto it { terminalSessions.find (uuid) };
    jassert (it != terminalSessions.end());

    const Terminal::Processor* result { nullptr };

    if (it != terminalSessions.end())
        result = &it->second->getProcessor();

    jassert (result != nullptr);
    return *result;
}

// =============================================================================

/**
 * @brief Removes and destroys the session with the given UUID.
 *
 * @note NEXUS PROCESS MESSAGE THREAD.
 */
void Session::remove (const juce::String& uuid)
{
    if (client != nullptr)
    {
        // Notify daemon to destroy the PTY process and unsubscribe from output.
        client->sendRemove (uuid);
        client->detachSession (uuid);
    }

    // Terminal::Session (remote or local) is always owned by terminalSessions.
    terminalSessions.erase (uuid);
    fireIfAllExited();
}

/**
 * @brief Returns true if a session with @p uuid is live on this daemon.
 *
 * @note NEXUS PROCESS MESSAGE THREAD.
 */
bool Session::hasSession (const juce::String& uuid) const noexcept
{
    return terminalSessions.find (uuid) != terminalSessions.end();
}

/**
 * @brief Returns a snapshot of all live session UUIDs.
 *
 * @note NEXUS PROCESS MESSAGE THREAD.
 */
juce::StringArray Session::list() const
{
    juce::StringArray uuids;

    if (client != nullptr)
    {
        auto processorsNode { AppState::getContext()->getProcessorsNode() };

        for (int i { 0 }; i < processorsNode.getNumChildren(); ++i)
            uuids.add (processorsNode.getChild (i).getProperty (jreng::ID::id).toString());
    }
    else
    {
        for (const auto& pair : terminalSessions)
            uuids.add (pair.first);
    }

    return uuids;
}

/**
 * @brief Forwards raw input bytes to the target session's PTY.
 *
 * Local/daemon mode: looks up Terminal::Session by uuid and calls sendInput.
 * Client mode: calls Client::sendInput to forward over IPC.
 *
 * @note MESSAGE THREAD.
 */
void Session::sendInput (const juce::String& uuid, const void* data, int size)
{
    if (client != nullptr)
    {
        client->sendInput (uuid, data, size);
    }
    else
    {
        const auto it { terminalSessions.find (uuid) };
        jassert (it != terminalSessions.end());
        it->second->sendInput (static_cast<const char*> (data), size);
    }
}

/**
 * @brief Forwards a terminal resize to the target session's PTY.
 *
 * Local/daemon mode: looks up Terminal::Session by uuid and calls resize.
 * Client mode: calls Client::sendResize to forward over IPC.
 *
 * @note MESSAGE THREAD.
 */
void Session::sendResize (const juce::String& uuid, int cols, int rows)
{
    if (client != nullptr)
    {
        client->sendResize (uuid, cols, rows);
    }
    else
    {
        const auto it { terminalSessions.find (uuid) };
        jassert (it != terminalSessions.end());
        it->second->resize (cols, rows);
    }
}

/**
 * @brief Routes incoming bytes from daemon to the Processor for @p uuid.
 *
 * Client mode only: called by Client when Message::output arrives.
 * Looks up the Terminal::Session by UUID and feeds bytes to its Processor.
 *
 * @note MESSAGE THREAD (called from Client::messageReceived).
 */
void Session::feedBytes (const juce::String& uuid, const void* data, int size)
{
    const auto it { terminalSessions.find (uuid) };

    if (it != terminalSessions.end() and size > 0)
    {
        Terminal::Processor& proc { it->second->getProcessor() };
        proc.process (static_cast<const char*> (data), size);
        proc.getParser().flushResponses();
    }
}

/**
 * @brief Restores Processor state from a snapshot sent by the daemon on attach.
 *
 * Calls setStateInformation directly on the message thread — no Loader thread.
 *
 * @note MESSAGE THREAD.
 */
void Session::startLoading (const juce::String& uuid, juce::MemoryBlock&& bytes)
{
    const auto it { terminalSessions.find (uuid) };
    jassert (it != terminalSessions.end());

    if (it != terminalSessions.end())
    {
        Terminal::Processor& proc { it->second->getProcessor() };
        proc.setStateInformation (bytes.getData(), static_cast<int> (bytes.getSize()));
    }
}

// =============================================================================

/**
 * @brief Creates the Server and starts listening on the default port.
 *
 * @note NEXUS PROCESS MESSAGE THREAD.
 */
void Session::startServer()
{
    server = std::make_unique<Server> (*this);
    server->start();
}

/**
 * @brief Stops the server and removes the lockfile.
 *
 * @note NEXUS PROCESS MESSAGE THREAD.
 */
void Session::stopServer()
{
    if (server != nullptr)
    {
        server->stop();
        server = nullptr;

        const juce::ScopedLock lock (connectionsLock);
        attached.clear();
    }
}

/**
 * @brief Returns true if the server is active.
 *
 * @note Any thread.
 */
bool Session::isServing() const noexcept { return server != nullptr and server->getPort() > 0; }

// =============================================================================

/**
 * @brief Adds @p connection to the broadcast list.
 *
 * @note Acquires connectionsLock.  Any thread.
 */
void Session::attach (ServerConnection& connection)
{
    const juce::ScopedLock lock (connectionsLock);
    attached.push_back (&connection);
}

/**
 * @brief Removes @p connection from the broadcast list AND every per-processor
 *        subscriber list in one locked pass.
 *
 * @note Acquires connectionsLock.  Any thread.
 */
void Session::detach (ServerConnection& connection)
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
 * @brief Fires onAllSessionsExited if the terminalSessions map is now empty.
 *
 * `terminalSessions` is the SSOT for session liveness in local and daemon modes.
 * Client mode never sets onAllSessionsExited, so this is safe unconditionally.
 *
 * @note NEXUS PROCESS MESSAGE THREAD.
 */
void Session::fireIfAllExited() noexcept
{
    if (terminalSessions.empty() and onAllSessionsExited != nullptr)
        onAllSessionsExited();
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Nexus

