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
#include "Log.h"
#include "Wire.h"
#include "../terminal/logic/Processor.h"
#include "../terminal/logic/Session.h"
#include "../terminal/data/Identifier.h"
#include "../config/Config.h"
#include "../AppState.h"
#include "../AppIdentifier.h"

#include <algorithm>
#include <cstring>

namespace Nexus
{
/*____________________________________________________________________________*/

/**
 * @brief Constructs the Session in local mode — no IPC.
 */
Session::Session()
{
    Nexus::logLine ("Session ctor: local mode");
    juce::ValueTree processorsNode { App::ID::PROCESSORS };
    AppState::getContext()->getNexusNode().appendChild (processorsNode, nullptr);
}

/**
 * @brief Constructs the Session in daemon mode — constructs Server and starts listening.
 *
 * @note NEXUS PROCESS MESSAGE THREAD.
 */
Session::Session (DaemonTag)
{
    Nexus::logLine ("Session ctor: daemon mode, starting server");
    startServer();
}

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
    Nexus::logLine ("Session ctor: client mode, constructing Client");
    client = std::make_unique<Client>();

    auto loadingNode { AppState::getContext()->getLoadingNode() };
    juce::ValueTree op { App::ID::OPERATION };
    op.setProperty (jreng::ID::id, nexusConnectOperationId, nullptr);
    loadingNode.appendChild (op, nullptr);

    const juce::File lockfilePath { Server::getLockfile() };
    Nexus::logLine ("Session ctor: calling beginConnectAttempts on " + lockfilePath.getFullPathName());
    client->beginConnectAttempts (lockfilePath);
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
 * @brief Creates a new session with a freshly-generated UUID.  Returns Processor reference.
 *
 * Delegates to the 7-argument overload with a generated UUID.
 *
 * @note NEXUS PROCESS MESSAGE THREAD.
 */
Terminal::Processor&
Session::create (const juce::String& shell, const juce::String& args, const juce::String& cwd,
                 int cols, int rows, const juce::String& envID)
{
    return create (shell, args, cwd, juce::Uuid().toString(), cols, rows, envID);
}

/**
 * @brief Creates a new session with an explicit UUID.  Returns Processor reference.
 *
 * Dispatches to `createClientSession`, `createDaemonSession`, or `createLocalSession`
 * based on the current mode.  If a session with @p uuid already exists the existing
 * Processor is returned immediately (idempotency guard for GUI reconnect).
 *
 * @param uuid  Must be non-empty.
 * @note NEXUS PROCESS MESSAGE THREAD.
 */
Terminal::Processor&
Session::create (const juce::String& shell, const juce::String& args, const juce::String& cwd,
                 const juce::String& uuid, int cols, int rows, const juce::String& envID)
{
    jassert (uuid.isNotEmpty());
    jassert (cols > 0);
    jassert (rows > 0);

    // Idempotency: if a session with this uuid already exists, return it.
    // Happens on GUI reconnect — client restores saved uuid and re-sends
    // spawnProcessor before the processorList push has updated its isLive cache.
    // Without this guard, the daemon would fork a second shell, fail to emplace,
    // and leave a dangling Processor reference.
    const auto existing { processors.find (uuid) };
    const bool alreadyExists { existing != processors.end() };

    Terminal::Processor* result { nullptr };

    if (alreadyExists)
    {
        Nexus::logLine ("Session::create: uuid=" + uuid + " already exists, returning existing");
        result = existing->second.get();
    }
    else if (client != nullptr)
    {
        result = &createClientSession (shell, cwd, uuid, cols, rows, envID);
    }
    else
    {
        const juce::String effectiveShell { shell.isNotEmpty()
            ? shell
            : Config::getContext()->getString (Config::Key::shellProgram) };
        const juce::String effectiveArgs { shell.isNotEmpty()
            ? args
            : Config::getContext()->getString (Config::Key::shellArgs) };

        juce::StringPairArray seedEnv;

#if ! JUCE_WINDOWS
        if (envID.isNotEmpty())
        {
            const auto parentIt { terminalSessions.find (envID) };

            if (parentIt != terminalSessions.end())
            {
                const int fgPid { parentIt->second->getForegroundPid() };

                if (fgPid > 0)
                {
                    const juce::String resolvedPath { parentIt->second->getEnvVar (fgPid, "PATH") };

                    if (resolvedPath.isNotEmpty())
                        seedEnv.set ("PATH", resolvedPath);
                }
            }
        }
#else
        juce::ignoreUnused (envID);
#endif

        if (server != nullptr)
            result = &createDaemonSession (effectiveShell, effectiveArgs, cwd, uuid, cols, rows, seedEnv);
        else
            result = &createLocalSession (effectiveShell, effectiveArgs, cwd, uuid, cols, rows, seedEnv);
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
    Terminal::Processor* result { nullptr };

    if (client != nullptr)
    {
        result = client->getProcessor (uuid);
    }
    else
    {
        const auto it { processors.find (uuid) };
        jassert (it != processors.end());

        if (it != processors.end())
            result = it->second.get();
    }

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
    const Terminal::Processor* result { nullptr };

    if (client != nullptr)
    {
        result = client->getProcessor (uuid);
    }
    else
    {
        const auto it { processors.find (uuid) };
        jassert (it != processors.end());

        if (it != processors.end())
            result = it->second.get();
    }

    jassert (result != nullptr);
    return *result;
}

// =============================================================================

/**
 * @brief Client-mode helper: constructs the pipeline-side Processor for @p uuid.
 *
 * Sends spawnProcessor to the daemon if the uuid is not yet live, sends
 * attachProcessor, constructs and registers the Processor, and returns a
 * reference to it.
 *
 * @note NEXUS PROCESS MESSAGE THREAD.
 */
Terminal::Processor&
Session::createClientSession (const juce::String& shell, const juce::String& cwd,
                               const juce::String& uuid, int cols, int rows,
                               const juce::String& envID)
{
    Nexus::logLine ("Session::create (client mode): uuid=" + uuid);

    auto processorsNode { AppState::getContext()->getProcessorsNode() };
    bool isLive { false };

    for (int i { 0 }; i < processorsNode.getNumChildren(); ++i)
    {
        if (processorsNode.getChild (i).getProperty (jreng::ID::id).toString() == uuid)
        {
            isLive = true;
            i = processorsNode.getNumChildren();
        }
    }

    if (not isLive)
        client->spawnSession (cols, rows, shell, cwd, uuid, envID);

    client->attachSession (uuid);

    auto proc { std::make_unique<Terminal::Processor> (cols, rows, uuid) };

    proc->getState().setId (uuid);

    // Wire parser responses (DSR, DA, CPR, etc.) back to the daemon via Client::sendInput.
    // Without this, vim/neovim start in degraded mode because terminal capability queries
    // get no reply.
    proc->setHostWriter ([this, uuid] (const char* data, int len)
    {
        client->sendInput (uuid, data, len);
    });

    client->registerProcessor (std::move (proc));

    Terminal::Processor* procPtr { client->getProcessor (uuid) };
    jassert (procPtr != nullptr);
    return *procPtr;
}

/**
 * @brief Daemon-mode helper: constructs the PTY session + stub Processor for @p uuid.
 *
 * Wires onBytes to broadcast Message::output to subscribers and onExit to
 * broadcast Message::processorExited to all attached connections.
 *
 * @note NEXUS PROCESS MESSAGE THREAD.
 */
Terminal::Processor&
Session::createDaemonSession (const juce::String& effectiveShell,
                               const juce::String& effectiveArgs,
                               const juce::String& cwd,
                               const juce::String& uuid,
                               int cols, int rows,
                               juce::StringPairArray seedEnv)
{
    auto termSession { std::make_unique<Terminal::Session> (
        cols,
        rows,
        effectiveShell,
        effectiveArgs,
        cwd,
        seedEnv) };

    // Daemon mode — wire onBytes to broadcast Message::output to subscribers.
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

    terminalSessions.emplace (uuid, std::move (termSession));

    // Daemon also needs a stub Processor so get() and forEachProcessor() work.
    auto proc { std::make_unique<Terminal::Processor> (cols, rows, uuid) };

    proc->getState().setId (uuid);

    Terminal::Processor& result { *proc };
    processors.emplace (uuid, std::move (proc));

    return result;
}

/**
 * @brief Local-mode helper: constructs the PTY + Processor pair for @p uuid.
 *
 * Wires onBytes → Processor::processWithLock, onDrainComplete → parser flush,
 * state onFlush → foreground process introspection, and onExit → onShellExited.
 *
 * @note NEXUS PROCESS MESSAGE THREAD.
 */
Terminal::Processor&
Session::createLocalSession (const juce::String& effectiveShell,
                              const juce::String& effectiveArgs,
                              const juce::String& cwd,
                              const juce::String& uuid,
                              int cols, int rows,
                              juce::StringPairArray seedEnv)
{
    auto termSession { std::make_unique<Terminal::Session> (
        cols,
        rows,
        effectiveShell,
        effectiveArgs,
        cwd,
        seedEnv) };

    // Local mode — wire onBytes → processor.process; wire parser.writeToHost → sendInput.
    auto proc { std::make_unique<Terminal::Processor> (cols, rows, uuid) };

    proc->getState().setId (uuid);

    // Wire parser responses back to PTY.
    Terminal::Session* termSessionPtr { termSession.get() };
    proc->setHostWriter ([termSessionPtr] (const char* data, int len)
    {
        termSessionPtr->sendInput (data, len);
    });

    // Wire drain-complete: flush parser responses and handle sync-resize.
    Terminal::Processor* procRawPtr { proc.get() };
    termSession->onDrainComplete = [procRawPtr, termSessionPtr]
    {
        procRawPtr->getParser().flushResponses();
        procRawPtr->getState().clearPasteEchoGate();

        if (procRawPtr->getState().consumeSyncResize())
            termSessionPtr->platformResize (procRawPtr->getGrid().getCols(), procRawPtr->getGrid().getVisibleRows());
    };

    // Wire state flush (foreground process introspection).
    procRawPtr->getState().onFlush = [procRawPtr, termSessionPtr]
    {
        const int fgPid { termSessionPtr->getForegroundPid() };

        if (fgPid > 0)
        {
            char fgNameBuf[Terminal::State::maxStringLength] {};
            termSessionPtr->getProcessName (fgPid, fgNameBuf, Terminal::State::maxStringLength);
            procRawPtr->getState().setForegroundProcess (fgNameBuf, static_cast<int> (std::strlen (fgNameBuf)));

            char cwdBuf[Terminal::State::maxStringLength] {};
            termSessionPtr->getCwd (fgPid, cwdBuf, Terminal::State::maxStringLength);
            procRawPtr->getState().setCwd (cwdBuf, static_cast<int> (std::strlen (cwdBuf)));
        }
    };

    // Wire TTY exit → processor lifecycle (via onShellExited callback).
    termSession->onExit = [this, uuid]
    {
        juce::MessageManager::callAsync (
            [this, uuid]
            {
                const auto it { processors.find (uuid) };

                if (it != processors.end() and it->second->onShellExited != nullptr)
                    it->second->onShellExited();
            });
    };

    // Wire onBytes → processor.process (with resize lock as before).
    termSession->onBytes = [procRawPtr] (const char* bytes, int len)
    {
        procRawPtr->processWithLock (bytes, len);
    };

    terminalSessions.emplace (uuid, std::move (termSession));

    Terminal::Processor& result { *proc };
    processors.emplace (uuid, std::move (proc));

    return result;
}

/**
 * @brief Removes and destroys the session with the given UUID.
 *
 * @note NEXUS PROCESS MESSAGE THREAD.
 */
void Session::remove (const juce::String& uuid)
{
    if (client != nullptr)
    {
        client->sendRemove (uuid);
        client->unregisterProcessor (uuid);
        client->detachSession (uuid);
    }
    else
    {
        const auto procIt { processors.find (uuid) };

        if (procIt != processors.end())
            processors.erase (procIt);

        terminalSessions.erase (uuid);

        fireIfAllExited();
    }
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
        for (const auto& pair : processors)
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
        Nexus::logLine ("Session::sendInput uuid=" + uuid + " size=" + juce::String (size) + " (local/daemon mode -> Terminal::Session::sendInput)");
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
 * @note MESSAGE THREAD (called from Client::messageReceived).
 */
void Session::feedBytes (const juce::String& uuid, const void* data, int size)
{
    Terminal::Processor* proc { client != nullptr ? client->getProcessor (uuid) : nullptr };

    if (proc == nullptr)
    {
        const auto it { processors.find (uuid) };

        if (it != processors.end())
            proc = it->second.get();
    }

    if (proc != nullptr and size > 0)
        proc->process (static_cast<const char*> (data), size);
}

/**
 * @brief Constructs a `Nexus::Loader` to parse @p bytes into the Processor for @p uuid.
 *
 * @note MESSAGE THREAD.
 */
void Session::startLoading (const juce::String& uuid, juce::MemoryBlock&& bytes)
{
    Terminal::Processor* proc { client != nullptr ? client->getProcessor (uuid) : nullptr };
    jassert (proc != nullptr);

    auto loadingNode { AppState::getContext()->getLoadingNode() };
    juce::ValueTree op { App::ID::OPERATION };
    op.setProperty (jreng::ID::id, uuid, nullptr);
    loadingNode.appendChild (op, nullptr);

    auto loader { std::make_unique<Loader> (*proc, std::move (bytes), uuid) };

    loader->onFinished = [this, uuid]
    {
        auto loadingNode { AppState::getContext()->getLoadingNode() };
        auto finishedOp { jreng::ValueTree::getChildWithID (loadingNode, uuid) };

        if (finishedOp.isValid())
            finishedOp.getParent().removeChild (finishedOp, nullptr);

        loaders.erase (uuid);
    };

    loaders.emplace (uuid, std::move (loader));
}

// =============================================================================

/**
 * @brief Creates the Server and starts listening on the default port.
 *
 * @note NEXUS PROCESS MESSAGE THREAD.
 */
void Session::startServer()
{
    Nexus::logLine ("startServer: entry");
    server = std::make_unique<Server> (*this);
    Nexus::logLine ("startServer: Server constructed, calling start");
    server->start();
    Nexus::logLine ("startServer: Server::start returned, isServing=" + juce::String (isServing() ? 1 : 0));
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
 * @brief Fires onAllSessionsExited if the processor map is now empty.
 *
 * `processors` is the SSOT for session liveness in local and daemon modes.
 * Both maps (`processors` and `terminalSessions`) are erased together at every
 * exit path before this helper is called, so checking `processors` is correct
 * for both.  Client mode never sets onAllSessionsExited, so this is safe
 * unconditionally.
 *
 * @note NEXUS PROCESS MESSAGE THREAD.
 */
void Session::fireIfAllExited() noexcept
{
    if (processors.empty() and onAllSessionsExited != nullptr)
        onAllSessionsExited();
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Nexus
